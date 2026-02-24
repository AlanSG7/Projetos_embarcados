/*
Embarcatech - Trilha de Software Embarcado - Projeto Final
Arquivo principal
Aluno: Alan Sovano Gomes
Matrícula: 202420110940138
Mentora: Gabriela Teixeira
*/

// Bibliotecas a serem utilizadas
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include <math.h>
#include "ssd1306.h"
#include "sensores.h"
#include "semphr.h"
#include "conectividade.h"

// Definindo limiares para a detecção de queda
#define LIMIAR_QUEDA 0.5f // 0.5 g
#define LIMIAR_IMPACTO 2.0f // 2.0 g
#define LIMIAR_VAR_ORIENTACAO 0.7f // 0.7 no prod escalar (45 graus)
#define LIMIAR_VARIANCIA  0.1f // 0.1 g
#define TEMPO_QUEDA 300000 // 300 ms
#define TEMPO_INATIVIDADE 2000000 // 2 s
#define TEMPO_CONFIRMACAO 2000000 // 2 s
#define TEMPO_SEM_DEDO 5000000 // 5 s

// Variável para a máquina de estados da queda
typedef enum {
    EST_NORMAL = 0,
    EST_QUEDA_LIVRE,
    EST_IMPACTO,
    EST_INATIVIDADE,
    EST_QUEDA_CONFIRMADA
} estado_queda_t;

// Variáveis globlais
struct repeating_timer temporizador_aquisicao;
bool flag_printf = 0;

// Guardam os dados que serão mostrados no display OLED
const char *words[]= {"Sistema IoMT", "Projeto Final","Embarcatech", "ANGULO DE INCLINACAO:", "ANGULO MAIOR QUE 90!"}; // Strings utilizadas no display OLED
char info [25]; 
char info2 [50];
char info3 [25];
char info4 [25];
char info5 [25];

// Variáveis do MQTT (vindas de conectividade.c)
extern bool mqtt_conectado; // Verifica se a conexão MQTT foi realizada com sucesso
extern bool mqtt_conectando; // Verifica se a conexão MQTT está em processo de conexão
   
// Declarando as filas a serem utilizadas para a sincronização entre tarfas
QueueHandle_t queue_dados_acelerometro, queue_dados_angulo, queue_dados_GPS, queue_dados_PPG, queue_dados_GPS_OLED;

// Declaração de semaphoro para mutex
SemaphoreHandle_t i2c_sensores_mutex;

// Flags para envio de mensagens e interface com "conectividade.c"
bool flag_queda = false;
bool flag_localizacao = false;
bool flag_desliga_alarme = false;
bool flag_liga_alarme = false;

// Variáveis de configuração do PWM do buzzer
// f_PWM = 125*(10^6)/((5)*(PERIOD+1))
// Se PERIOD = 24999 -> f_PWM = 1 kHz
const uint16_t PERIOD_BUZZER =24999;
const float DIVIDER_PWM = 5;

// Para x% do valor do led, a entrada deve ser (x/100)*(PERIOD+1) de acordo com o datasheet
volatile uint16_t pwm_level = 50*(PERIOD_BUZZER+1)/100;



/****************************************************** Declarando as tasks do projeto ************************************************************/

// Task 1: Aquisicao de dados da MPU
void aquisicao_dados_MPU_task(void *pvParameters)
{
    // variável utilizada para tratar os dados na task
    dados_MPU6050_t dados_acc;

    // Laço inifnito da task
    for(;;)
    {
        // Utilização do MUTEX para o acesso ao barramento I2C
        xSemaphoreTake(i2c_sensores_mutex, portMAX_DELAY);

        // Captura dos dados disponibilizados pela
        capta_medicao_mpu6050(&dados_acc.ax, &dados_acc.ay, &dados_acc.az, &dados_acc.gx, &dados_acc.gy, &dados_acc.gz);
        
        // Liberação do barramneto I2C
        xSemaphoreGive(i2c_sensores_mutex);

        // Enviando os dados do acelerômetro para uma fila
        xQueueSend(queue_dados_acelerometro, &dados_acc, portMAX_DELAY); 
        
        // delay de 50 ms para a task 
        vTaskDelay(pdMS_TO_TICKS(50)); 
        
    }
}

// Task 2: Aquisicao de dados do GPS
void aquisicao_dados_GPS_task(void *pvParameters)
{
    // Variáveis utilizadas para tratar os dados na task
    dados_GPS_t saida_GPS;
    char linha[128];
    int elemento_linha = 0;

    // Laço inifnito da task
    for(;;)
    {
        // Verifica se há o que ler na UART
        if (uart_is_readable(UART_ID)) 
        {
            // Capta o caracter que está vindo pela uart para análise
            char c = uart_getc(UART_ID);

            // Avalia se foi obtida uma linha de informações completa do GPS
            if(c=='\n')
            {
                // Inclui o caracter nulo no final da string, processa a string e envia para uma fila de dados
                linha[elemento_linha]='\0';
                saida_GPS.estado = dados_GPS(linha,saida_GPS.latitude, saida_GPS.longitude,saida_GPS.hem_latitude, saida_GPS.hem_longitude);
                elemento_linha = 0;
                xQueueSend(queue_dados_GPS, &saida_GPS, portMAX_DELAY); 
            }
            else{
                if(elemento_linha<sizeof(linha)-1)
                {
                    linha[elemento_linha++]=c;
                }
            }
   
        }

        // Delay mínimo (para captar continuamente os dados pela UART)
        vTaskDelay(pdMS_TO_TICKS(1));

    }
}

// TASK 3: Aquisição dos dados do fotopletismógrafo
void aquisicao_dados_MAX30102_task(void *pvParameters)
{

    // Estado do processamento
    PulseState st; 
    ps_init(&st);

    // idx: contador de amostras (0,1,2,...) — usado para tempos e RR
    int idx = 0;


    // Laço principal da task (verifica a FIFO, processa e envia para uma fila)
    for(;;)
    {

        // Usa o MUTEX para se apropriar do barramento I2C        
        xSemaphoreTake(i2c_sensores_mutex, portMAX_DELAY);

        // Verifica quantas amostras estão disponíveis
        uint8_t avail = fifo_available();

        // Variáveis para ler o buffer
        uint32_t red, ir;
        uint32_t red_buf[MAX_FIFO_SAMPLES], ir_buf[MAX_FIFO_SAMPLES];
        
        // Se não há amostras válidas, larga o barramento I2C, dá prioridade para outra task e vai para próxima iteração
        if (avail == 0) 
        { 
            xSemaphoreGive(i2c_sensores_mutex);
            vTaskDelay(pdMS_TO_TICKS(5)); 
            continue; 
        }

        // Colocando as amostras em um buffer para liberar o mutex e processar após a liberação
        for (uint8_t i = 0; i < avail; i++) 
        {
            
            if (!read_sample(&red, &ir)) 
            {
                break;
            }

            /* Salva as amostras em buffer local */
            red_buf[i] = red;
            ir_buf[i]  = ir;
        }

        xSemaphoreGive(i2c_sensores_mutex);


        // Lê e processa amostra por amostra
        for (uint8_t i=0; i<avail; i++){
            
            // Processa a amostra atual e obtém uma estimativa de SpO2
            float spo2 = process(&st, red_buf[i], ir_buf[i], idx++);

            // Considera “pulso recente” se última batida foi há < 1 s
            bool pulse_recent = (idx - st.last_peak) < (int)(FS_HZ * 1.0f);
            // Valida a faixa de BPM e se há dedo presente
            bool valid = st.finger_on && pulse_recent && (st.bpm >= 35.0f && st.bpm <= 200.0f);

            dados_PPG_t data = 
            {
                .bpm = valid ? st.bpm : 0.0f,
                .spo2 = (valid && !isnan(spo2)) ? spo2 : 0.0f,
                .finger_on = valid,
                .timestamp_ms = to_ms_since_boot(get_absolute_time())
            };

            // Envia sem bloquear (descarta se fila cheia)
            xQueueOverwrite(queue_dados_PPG, &data);


        }

        // delay de 100 ms para a task 
        vTaskDelay(pdMS_TO_TICKS(100)); 


    }

}

// Task 4: Processamento de dados
void processa_dados_task(void *pvParameters)
{
    // Variáveis para recebimento de dados por filas
    dados_MPU6050_t dados_recebidos_MPU, orientacao_ref_norm, orientacao_norm;
    dados_GPS_t dados_recebidos_GPS;

    // Variável para definir os estados da MEF
    static estado_queda_t estado_queda = EST_NORMAL;

    // Variáveis para auxiliar na marcação temporal dos subeventos da queda
    absolute_time_t tempo_queda, tempo_impacto,tempo_inatividade; 
    float delta_a = 0.0;
    float norma_ac_prev = 0.0;
    float norma_ac = 0.0;
    float phi = 0.0;
    float prod_escalar = 0.0;
    float quad_norma = 0.0;

    // Laço infnito da task
    for(;;)
    {

       // Recebendo dados da MPU
        xQueueReceive(queue_dados_acelerometro, &dados_recebidos_MPU, portMAX_DELAY); 


        // Cálculo da norma do vetor de aceleração
        quad_norma = dados_recebidos_MPU.ax*dados_recebidos_MPU.ax +
        dados_recebidos_MPU.ay*dados_recebidos_MPU.ay +
        dados_recebidos_MPU.az*dados_recebidos_MPU.az;  
        norma_ac = sqrtf(quad_norma);

        // Calcula o ângulo de inclinação, convertendo de radianos para segundos (phi é a incliniação a partir do eixo z)
        phi = 57.2958f * acosf(dados_recebidos_MPU.az / norma_ac);
    

        // Algoritmo de detecção de queda
        switch (estado_queda)
        {
            case EST_NORMAL:
        
                // Primeira etapa da detecção de queda: sensação de ausência parcial da gravidade (pelo sensor)
                if (norma_ac < LIMIAR_QUEDA) 
                {
                    // Garante que a norma do vetor é maior que zero (evita possíveis erros numéricos)
                    if(norma_ac<0.1f)
                        break;

                    // Avança para o próximo estado
                    estado_queda = EST_QUEDA_LIVRE;

                    // Salva o instante da mudança de estado
                    tempo_queda = get_absolute_time();

                    // Salvando a orientação para etapas futuras da MEF (normalizado)                    
                    orientacao_ref_norm.ax = dados_recebidos_MPU.ax/norma_ac;
                    orientacao_ref_norm.ay = dados_recebidos_MPU.ay/norma_ac;
                    orientacao_ref_norm.az = dados_recebidos_MPU.az/norma_ac;

                    // DEBUG
                    // printf("OK 1 \n");
                }
                break;

            // Segundo estado: avaliação se houve um impacto após a queda livre
            case EST_QUEDA_LIVRE:

                // Se houve um impacto (aceleração brusca) em um tempo relativamente curto
                if (norma_ac > LIMIAR_IMPACTO && (absolute_time_diff_us(tempo_queda, get_absolute_time())) < TEMPO_QUEDA) 
                {
                    // Vai para o próximo estado e salva o tempo de detecção do estado atual
                    estado_queda = EST_IMPACTO;
                    tempo_impacto = get_absolute_time();

                    // DEBUG
                   // printf("OK 2 \n");
                }

                // Se o tempo passou e não entrou na condição anterior, volta ao pimeiro etado
                else if (absolute_time_diff_us(tempo_queda, get_absolute_time()) > TEMPO_QUEDA) {
                    estado_queda = EST_NORMAL;
                }
                break;

            // Terceiro estado: avaliação se houve uma inatividade após o impacto
            case EST_IMPACTO:

                // Calcula a variação da aceleração
                delta_a = fabsf(norma_ac - norma_ac_prev);

                // Se houve pouca variação da aceleração por muito tempo, houve queda
                if (delta_a<LIMIAR_VARIANCIA && absolute_time_diff_us(tempo_impacto, get_absolute_time())>TEMPO_INATIVIDADE) 
                {

                    // Vai para o próximo estado e guarda o momento em que o estado atual foi detectado
                    estado_queda = EST_INATIVIDADE;
                    tempo_inatividade = get_absolute_time();

                    // DEBUG
                    // printf ("OK 3 \n");
                }

                // Se há uma variação grande da aceleração e já passou um tempo grande, não houve queda
                else if(delta_a>LIMIAR_VARIANCIA && absolute_time_diff_us(tempo_impacto, get_absolute_time())>TEMPO_INATIVIDADE )
                {
                    estado_queda = EST_NORMAL;
                }

                // Atualiza a norma da aceleração para comparar com o próximo valor
                norma_ac_prev = norma_ac;
                break;
        
            // Quarto estado: avalia se, enquanto há inatividade, houve mudança de orientação do acelerômetro  
            case EST_INATIVIDADE:

                // Garante que a norma do vetor é maior que zero (evita possíveis erros numéricos)
                if(norma_ac<0.1f)
                    break;

                // Normaliza o vetor aceleração atual
                orientacao_norm.ax = dados_recebidos_MPU.ax/norma_ac;
                orientacao_norm.ay = dados_recebidos_MPU.ay/norma_ac;
                orientacao_norm.az = dados_recebidos_MPU.az/norma_ac;

                // Produto escalar entre vetores unitários (o atual e o salvo no início da detecção de queda)
                prod_escalar = fabsf(orientacao_norm.ax*orientacao_ref_norm.ax +
                orientacao_norm.ay*orientacao_ref_norm.ay +
                orientacao_norm.az*orientacao_ref_norm.az);

                // DEBUG
               // printf("%.2f \n", prod_escalar);

                // Se o produto escalar é alto (mais perto de 1) por um tempo, a orientação não mudou (não houve queda)
                
                if(prod_escalar > LIMIAR_VAR_ORIENTACAO && absolute_time_diff_us(tempo_inatividade, get_absolute_time())>TEMPO_QUEDA)  
                {
                    estado_queda = EST_NORMAL;
                }

                // Se o produto escalar é baixo após um tempo considerável, a orientação mudou (e a mudança persiste)
                else if(prod_escalar < LIMIAR_VAR_ORIENTACAO && absolute_time_diff_us(tempo_inatividade, get_absolute_time())>TEMPO_QUEDA) 
                {
                    // Houve queda
                    estado_queda = EST_QUEDA_CONFIRMADA;

                    // DEBUG
                    // printf ("OK 4 \n");
                }
                
                break;

            // Quinto estado: A queda foi confirmada
            case EST_QUEDA_CONFIRMADA:

                // Mexe no estado das flags de envio de mensagem e liga os alarmes
                //printf ("queda confirmada!");
                flag_queda = true;
                flag_liga_alarme = false;
                estado_queda = EST_NORMAL;
                gpio_put(LED_PIN_R, true);
                pwm_set_gpio_level(BUZZER, pwm_level);
 
                break;

            // Estado padrão: estado normal
            default:
                estado_queda = EST_NORMAL;
                break;
        }


        // Avaliando flags para controle de mensagens
        if(flag_desliga_alarme == true)
        {
            // Desliga alarmes
            gpio_put(LED_PIN_R, false);
            pwm_set_gpio_level(BUZZER, 0);
            flag_queda = false;
            flag_desliga_alarme = false;
            flag_liga_alarme = false;
        }

        else if(flag_liga_alarme == true)
        {
            // Liga alarmes fora da condição de queda
            gpio_put(LED_PIN_R, true);
            pwm_set_gpio_level(BUZZER, pwm_level);
            flag_desliga_alarme = false;
        }


        // Enviando dados para uma nova fila
        xQueueSend(queue_dados_angulo, &phi, portMAX_DELAY); // Coloca o estado do botão capturado em uma fila

        // Processamento do GPS
         if(xQueueReceive(queue_dados_GPS, &dados_recebidos_GPS, 0)==pdPASS)
        {
            // Convertendo a latitude
            float valor = atof(dados_recebidos_GPS.latitude);
            int graus = (int) (valor/100);
            float minutos = valor - (graus*100);
            dados_recebidos_GPS.latitude_num = graus + (minutos/60);

            // Convertendo a longitude
            valor = atof(dados_recebidos_GPS.longitude);
            graus = (int) (valor/100);
            minutos = valor - (graus*100);
            dados_recebidos_GPS.longitude_num = graus + (minutos/60);

            // Enviando dados processados para o OLED
            xQueueSend(queue_dados_GPS_OLED, &dados_recebidos_GPS, portMAX_DELAY);
        }

    }
}


// Task 5: Interface com o usuario
void interface_usuario_task(void *pvParameters)
{
    // Declarando variável utilizada para definir o display OLED
    ssd1306_t disp;
    disp.external_vcc=false;
    ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
    ssd1306_clear(&disp);
    
    // Variáveis que recebem dados processados das filas
    float angulo;
    dados_GPS_t gps;
    dados_PPG_t ppg;
    absolute_time_t ultimo_gps, tempo_ox;


    // Laço inifnito da task
    for(;;)
    {

        // Delay 0 -> Disparado por evento
        xQueueReceive(queue_dados_angulo, &angulo, 0); // Dados do ângulo vindos da fila
        xQueueReceive(queue_dados_PPG,&ppg,0); // Dados do PPG vindos da fila

        if(xQueueReceive(queue_dados_GPS_OLED, &gps, 0)==pdPASS)
        {
            // Se o GPS é detectado, envia os dados
            if(gps.estado == true)
            {

                ultimo_gps = get_absolute_time();
                sprintf(info2,"Latitude: %.2f %s",gps.latitude_num, gps.hem_latitude);
                sprintf(info3,"Longitude: %.2f %s",gps.longitude_num, gps.hem_longitude);
            }
            else if(absolute_time_diff_us(ultimo_gps, get_absolute_time())>5000000) // Se perder o sinal por 5 segundos, mostra a mensagem
            {
                // Se o GPS não estiver conectado, aguarda
                strcpy(info2,"GPS nao detectado!");
                strcpy(info3,"Aguarde...");

            }

        }

        // Imprime os valores de ângulo 
        sprintf(info,"%.2f graus", angulo);

        // Caso haja um dedo detecdado, imprime os dados
        if(ppg.finger_on==true)
        {
            sprintf(info4,"%.2f bpm",ppg.bpm);
            sprintf(info5, "%.2f%% (SpO2)", ppg.spo2);
            tempo_ox = get_absolute_time();
        }
        else if (absolute_time_diff_us(tempo_ox,get_absolute_time())>TEMPO_SEM_DEDO)

        // Caso o dedo não tenha sido detectado, imprime as strings a seguir
        {
            sprintf(info4,"Sensor sem leitura");
            sprintf(info5, " "); 
        }


        // Escrevendo a mensagem desejada no display OLED:
        ssd1306_draw_string(&disp, 27, 0, 1, words[0]);
        ssd1306_draw_string(&disp, 0, 12, 1, words[1]);
        ssd1306_draw_string(&disp, 0, 22, 1, words[2]);
        ssd1306_draw_string(&disp, 0, 32, 1, info4);
        ssd1306_draw_string(&disp, 0, 42, 1, info5);
        ssd1306_draw_string(&disp, 0, 52, 1, info);

        //ssd1306_draw_string(&disp, 0, 42, 1, info2);
        //ssd1306_draw_string(&disp, 0, 52, 1, info3);

        ssd1306_show(&disp);
        ssd1306_clear(&disp);  

        // delay de 10 ms para a task 
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }

}

// Task 6: Conectividade (Wi-Fi e MQTT)
void conectividade_MQTT_task(void *pvParameters)
{
    // Tenta se conectar à rede wi-fi por 1 minuto
    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 60000))
    {
        printf("Falha ao conectar à rede Wi-Fi\n");
        vTaskDelay(pdMS_TO_TICKS(100));
        continue;
    }
    printf("Conectado ao Wi-Fi\n");

    MQTT_CLIENT_T *state = mqtt_client_init();
    run_dns_lookup(state);  
    state->mqtt_client = mqtt_client_new();

    for(;;)
    {
            if(mqtt_client_is_connected(state->mqtt_client) && mqtt_conectado)
            {
                // Realizar a publicação desejada no tópico anteriormente definido
                mqtt_publicar(state);
                vTaskDelay(pdMS_TO_TICKS(50));
            }
            else if (!mqtt_conectando)
            {
                DEBUG_printf("Reconectando...\n");
                mqtt_conectando = true;
                cyw43_arch_gpio_put(LED_PIN, 0);

                // Tenta reconectar
                mqtt_conectar(state);
                vTaskDelay(pdMS_TO_TICKS(250));
            }
            else
            {
                DEBUG_printf("Aguarde...\n");
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


// Task para inicialização do módulo Wi-Fi
void wifi_init_task(void *p) 
{ 
    // Tenta inicializar o módulo Wi-Fi
    while (cyw43_arch_init()) 
    { 
        printf("Falha ao inicializar Wi-Fi\n"); 
        vTaskDelay(pdMS_TO_TICKS(100)); 
    } 
    
    // Colocando o pi pico W em modo station (vai se conectar ao roteador wi-fi) 
    cyw43_arch_enable_sta_mode(); 
    printf("Wi-Fi inicializado!\n"); 
    vTaskDelete(NULL); 
}


    
/***************************************************************** Funções auxiliares ************************************************************/

// Configuração do PWM para acionamento do buzzer
void setup_pwm()
{

  // Variáveis de slice
  uint slice_buzzer;

  // Setando pinos de PWM
  gpio_set_function(BUZZER, GPIO_FUNC_PWM);


  // Obtendo valor dos slices
  slice_buzzer = pwm_gpio_to_slice_num(BUZZER);

  // Definindo divisor de clock do PWM
  pwm_set_clkdiv(slice_buzzer, DIVIDER_PWM);
  
  // Valor do contador do PWM (período)
  pwm_set_wrap(slice_buzzer, PERIOD_BUZZER);

  // Valor inicial do pino PWM
  pwm_set_gpio_level(BUZZER, 0);

  // Habilitando o PWM no slice correspondente
  pwm_set_enabled(slice_buzzer, true);


}


// Função para configuração inicial do sistema (setup)
void setup()
{
 /******************************************Configuração do I2C*********************************************/
        
    // Inicializando as entradas e saídas padrões do sistema
    stdio_init_all();

    // Definindo a função dos pinos para a comunicação I2C
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicialização da comunicação I2C com uma velocidade de 100 kHz
    i2c_init(I2C_PORT, 100*1000);

    // Inicializando o acelerômetro/giroscópio
    inicializar_mpu6050();

    // Inicializando o GPS
    inicializar_GPS();
    sleep_ms(500);

    /********************************************Configuração do MAX30102 ***************************************/

    max_init(LED_PA);

        
        
    /******************************************Configuração do OLED***********************************************/

    //Comunicação I2C do OLED
    i2c_init(i2c1, 400000);
    gpio_set_function(14, GPIO_FUNC_I2C);
    gpio_set_function(15, GPIO_FUNC_I2C);
    gpio_pull_up(14);
    gpio_pull_up(15);

    // Inicializa o display para desligá-lo (caso estivesse ligado anteriormente)
    ssd1306_t disp;
    disp.external_vcc=false;
    ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
    ssd1306_poweroff(&disp);

/****************************************** Configuração dos alarmes ***********************************************/

    // Configuração do LED
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);  

    // Configuração do buzzer 
    setup_pwm(); 
    

}

/******************************************************************* FUNÇÃO PRINCIPAL *************************************************************/
int main()
{
    // Configurando o sistema
    setup();

    // Criando o mutex
    i2c_sensores_mutex = xSemaphoreCreateMutex();

    // Verificando se foi crido com sucesso
    configASSERT(i2c_sensores_mutex != NULL); 

    // Criando as filas de dados de tamanho arbitrário 5 (armazena os dados do acelerômetro)
    queue_dados_acelerometro = xQueueCreate(5, sizeof(dados_MPU6050_t));
    queue_dados_angulo = xQueueCreate(1, sizeof(float));
    queue_dados_GPS = xQueueCreate(1, sizeof(dados_GPS_t));
    queue_dados_PPG = xQueueCreate(1, sizeof(dados_PPG_t));
    queue_dados_GPS_OLED = xQueueCreate(1, sizeof(dados_GPS_t));
    
    // Criando as tasks (em caso de sucesso de criação da fila de dados)
    if(queue_dados_acelerometro != NULL)
    {
        xTaskCreate(aquisicao_dados_MPU_task,"tarefa_leitura_dados_MPU", 1024, NULL, 4, NULL); // Task com prioridade 4 (muito alta)
        xTaskCreate(aquisicao_dados_GPS_task,"tarefa_leitura_dados_GPS", 1024, NULL, 2, NULL); // Task com prioridade 2 (média)
        xTaskCreate(aquisicao_dados_MAX30102_task,"tarefa_leitura_dados_MAX30102", 2048, NULL, 4, NULL); // Task com prioridade 4 (muito alta)
        xTaskCreate(interface_usuario_task,"tarefa_interface_usuario", 2048, NULL, 1, NULL); // Task com prioridade 1 (baixa)
        xTaskCreate(processa_dados_task,"tarefa_processar_dados", 1024, NULL, 3, NULL); // Task com prioridade 3 (alta)
        xTaskCreate(conectividade_MQTT_task,"tarefa_conectar_MQTT", 8192, NULL, 3, NULL); // Task com prioridade 3 (alta)
        xTaskCreate(wifi_init_task, "wifi_init", 8192, NULL, CYW43_TASK_PRIORITY, NULL); // Prioridade máxima (inicialização do hardware, task morre após execução)

    
        // Iniciando o escalonador de tarefas
        vTaskStartScheduler();
    }

    // Loop infinito para ser acessado no caso de algum tipo de falha na inicialização do escalonador de tarefas
    while (true) 
    {
        tight_loop_contents(); // Função que otimiza o loop vazio para evitar consumo excessivo de CPU.     
    }
}
