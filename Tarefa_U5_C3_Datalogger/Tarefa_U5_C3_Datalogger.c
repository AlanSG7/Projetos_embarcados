/*
Embarcatech - Trilha de Software Embarcado - Tarefa da Unidade 5 Capítulo 2  (Datalogger)
Aluno: Alan Sovano Gomes
Matrícula: 202420110940138
Mentora: Gabriela Teixeira
*/

/* 
OBS: O sistema está funcional, mas apresenta um pequeno BUG: quando a fonte de alimentação muda de BATERIA para
USB, o sistema trava. Além disso, quando muda de USB para BATERIA, ocorrem eventuais travamentos.

NOTA: Aparentemente, o travamento ocorre devido ao uso de energia do SDCard. Com a mudança de fonte,
o SDCard fica instável e, consequentemente, a operação do sistema trava. Ao retirar a leitura do SDCard,
o sistema funciona normalmente.
*/

// Bibliotecas utilizadas
#include <stdio.h>
#include "pico/stdlib.h"
#include"hardware/rtc.h"
#include "hw_config.h"
#include "f_util.h"
#include "ff.h"
#include "ssd1306.h"

// Definindo os pinos da comunicação I2C (utilizando a I2C0 pelos pinos GP0 e GP1)
#define I2C_PORT i2c0
#define I2C_SDA 0
#define I2C_SCL 1
#define AHT10_ADDR 0x38 // Endereço do dispositivo I2C (temperatura e umidade)
#define BQ25622_ADDR 0x6B // Endereço I2C do gerenciador de energia
#define PUSH_BUTTON_A 5 // Botão para mudar a tela do OLED

// Protótipo das funções do SDCard e do sensor
void inicializar_SDCard();
void abrir_arquivo_SDCard(char *filename);
void encerrar_arquivo_SDCard(FIL* fil);
void ler_dados_csv_cartao_SD(char *filename);
void inicializar_aht10();
void capta_medicao_aht10(float *temperatura, float *umidade);
bool aquisicao_temporizador_callback(struct repeating_timer *t);
void inicializar_bq25622();
void capta_medicao_bq25622(float *vbat, float *ibat);
void mudar_tela_callback(unsigned gpio, uint32_t events);


// Variáveis para trabalhar com o SDCard
FATFS fs;
FIL file_write, file_read;
FRESULT fr;
int contador = 0;
char dados_txt[50];

 // Variáveis globlais
struct repeating_timer temporizador_aquisicao;
float leitura_temp = 0.0;
float leitura_umidade = 0.0;
float leitura_vbat = 0.0;
float leitura_ibat = 0.0;
bool flag_printf = 0;
const uint8_t comando_sensor[3] = {0xAC, 0x33, 0x00}; // Sequência de inicialização da leitura
const char *words[]= {"EMBARCASENSE", "Data e hora:","Dados Medidos: "}; // Strings utilizadas no display OLED
bool botao = 0; // Botão para mudança de tela
volatile absolute_time_t ultima_pressao_A = 0;
volatile bool usb_presente = false;
   
// Guardam os dados que serão mostrados no display OLED
char info [25]; 
char info2 [25];
char info3 [25];
char info4 [25];
char info5 [25];

// Variável com a definição inicial do RTC (ano, mês, dia, dia da semana, hora, minuto e segundo)
datetime_t  config_RTC= {2025,12,17,3,12,30,0 };

// Variável ponteiro que recebe o endereço da struct do RTC
datetime_t *valor_RTC = &config_RTC;



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


    // Criando o temporizador para realizar a coleta de dados a cada 1000 ms
    add_repeating_timer_ms(1000, aquisicao_temporizador_callback, NULL, &temporizador_aquisicao); 

    // Inicializando os sensores
    inicializar_aht10();
    inicializar_bq25622();

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


/******************************************Inicializando o SDCard***********************************************/

    inicializar_SDCard();

/******************************************Inicializando o RTC***********************************************/

    rtc_init();
    rtc_set_datetime(valor_RTC);

/******************************************Botão de mudança de tela***********************************************/


    // Inicializando o pino do botão A
    gpio_init(PUSH_BUTTON_A);
    gpio_set_dir(PUSH_BUTTON_A, GPIO_IN);
    gpio_pull_up(PUSH_BUTTON_A);

    // Configurando interrupção através do botão A
    gpio_set_irq_enabled_with_callback(PUSH_BUTTON_A, GPIO_IRQ_EDGE_FALL, true, mudar_tela_callback);

}


int main()
{
    // Realizando as configurações iniciais
    setup();

    // Declarando variável utilizada para definir o display OLED
    ssd1306_t disp;
    disp.external_vcc=false;
    ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
    ssd1306_clear(&disp);

     // Arquivo para salvar as informações do datalogger
    char* datalog = "dados_salvos.txt";

    // Adicionando o cabeçalho do arquivo
    abrir_arquivo_SDCard(datalog);
    sprintf(dados_txt,"Umidade (%%%%), Temperatura (C), Tensao (V), Corrente (mA), Hora/Data\n");
    // Escrevendo dados no arquivo 
    if (f_printf(&file_write, dados_txt) < 0) 
    {
        printf("f_printf failed\n");
    }
    encerrar_arquivo_SDCard(&file_write);


    // Verificação periódica para a realização do printf
    while (true) 
    {
        // Enviando o valor lido para o monitor serial
        if(flag_printf==1)
         {

            // Mostrando no monitor serial e guardando as infos em strings
            printf("Temperatura: %.2f C\nUmidade relativa: %.2f%%\n", leitura_temp, leitura_umidade);
            sprintf(info, "Umidade: %.2f%%", leitura_umidade);
            sprintf(info2, "Temperatura: %.2f C", leitura_temp);
            sprintf(info4, "Tensao: %.3f V", leitura_vbat);
            sprintf(info5, "Corrente: %.1f mA", leitura_ibat);

            // Dados sobre a bateria
            printf("Tensão da Bateria: %.3f V\nCorrente da bateria: %.1f mA\n", leitura_vbat, leitura_ibat);

            // Verificando a data e a hora
            rtc_get_datetime(valor_RTC);

            // Variável com a definição do RTC (ano, mês, dia, dia da semana, hora, minuto e segundo)
            if(config_RTC.sec<10)
                sprintf(info3, "%i:%i:0%i (%i/%i/%i)", config_RTC.hour, config_RTC.min, config_RTC.sec, config_RTC.day, config_RTC.month, config_RTC.year);
            else
                sprintf(info3, "%i:%i:%i (%i/%i/%i)", config_RTC.hour, config_RTC.min, config_RTC.sec, config_RTC.day, config_RTC.month, config_RTC.year);

            printf(info3);
            printf("\n\n");

            
            // Seleção e tela do dispositivo
            if(botao == 0) // Mostra umidade e temperatura
            {
                // Escrevendo a mensagem desejada no display OLED:
                ssd1306_draw_string(&disp, 27, 0, 1, words[0]);
                ssd1306_draw_string(&disp, 0, 12, 1, words[2]);
                ssd1306_draw_string(&disp, 0, 22, 1, info);
                ssd1306_draw_string(&disp, 0, 32, 1, info2);
                ssd1306_draw_string(&disp, 0, 45, 1, words[1]);
                ssd1306_draw_string(&disp, 0, 55, 1, info3);
            }
            else // Mostra tensão e corrente da bateria
            {
                // Escrevendo a mensagem desejada no display OLED:
                ssd1306_draw_string(&disp, 27, 0, 1, words[0]);
                ssd1306_draw_string(&disp, 0, 12, 1, words[2]);
                ssd1306_draw_string(&disp, 0, 22, 1, info4);
                ssd1306_draw_string(&disp, 0, 32, 1, info5);
                ssd1306_draw_string(&disp, 0, 45, 1, words[1]);
                ssd1306_draw_string(&disp, 0, 55, 1, info3);

            }


           // Salvando os dados no SDCard
            abrir_arquivo_SDCard(datalog);
            sprintf(dados_txt,"%f, %f, %f, %f, %s\n", leitura_umidade, leitura_temp, leitura_vbat, leitura_ibat, info3);

            // Escrevendo dados no arquivo 
            if (f_printf(&file_write, dados_txt) < 0) 
            {
                printf("f_printf failed\n");
            }
            encerrar_arquivo_SDCard(&file_write);



            ssd1306_show(&disp);
            ssd1306_clear(&disp);   

            flag_printf = 0;
        }
            
    }

    // Desmontando a unidade do SDCard
    f_unmount("");
    return 0;
}

/*************************************Funções para a utilização do SDCard*****************************************/


// Inicializando o SDCard
void inicializar_SDCard()
{
    
    fr  = f_mount(&fs, "", 1);
    if (FR_OK != fr) {
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }

}

// Abrindo/gerando o arquivo para ser manipulado no SDCard
void abrir_arquivo_SDCard(char *filename)
{
    // Open a file and write to it
    fr = f_open(&file_write, filename, FA_OPEN_APPEND | FA_WRITE);
    if (FR_OK != fr && FR_EXIST != fr) {
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    }

}

// Fechando o arquivo após salvar os dados
void encerrar_arquivo_SDCard(FIL* fil)
{
    // Close the file
    fr = f_close(fil);
    if (FR_OK != fr) {
        printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }
}




/************************************Funções para utilizar os sensores escolhidos****************************/



    void inicializar_aht10()
    {
        // 0xE1 -> Inicializa o sensor; 
        // 0x08 -> Comando que envolve a calibração do sensor;
        // 0x00 -> Byte extra contendo zeros (padding)
        uint8_t buf[3] = {0xE1, 0x08, 0x00};

        // Enviando os bytes pelo barramento I2C
        i2c_write_blocking(I2C_PORT, AHT10_ADDR, buf, 3, false); 
  
    }

    void inicializar_bq25622()
    {
        // Setando o conversor A/D do CI gerenciador de energia (A0 no reg 26 -> aumenta amostragem p 15 ms e resolução cai para 11 bits)
        uint8_t buffer_0[2] = {0x26, 0xA0}, buffer_1[2] = {0x27,0x00};
        i2c_write_blocking(i2c0, BQ25622_ADDR, buffer_0, 2, false);
        i2c_write_blocking(i2c0, BQ25622_ADDR, buffer_1, 2, false); 
    };


    // Função para ler e converter os dados que chegam do sensor de temperatura e umidade via I2C
    // Recebe os endereços de duas variáveis (facilita o envio de várias infos pela função)
    void capta_medicao_aht10(float *temperatura, float *umidade)
    {
        // Buffer de dados
        uint8_t buf[6];

        // Enviando o comando para leitura (true para manter o controle do barramento)
        i2c_write_blocking(I2C_PORT, AHT10_ADDR, comando_sensor, 3, true);

        // Lendo os dados (false para encerrar a comunicação com o barramento)]
        // OBS: diferente do sensor de luminosidade, esse não tem modo de amostragem automática!
        do
        {
           i2c_read_blocking(I2C_PORT, AHT10_ADDR, buf, 6, false);

        } while ( (buf[0]>>7) == 1 ); // Verificando se o bit de estado indica que a operação de envio de dados por parte do sensor acabou

        // Reconstruindo os dados de temperatura (infos do 2º, 3º e metade do 4º byte que foram recebidos)
        // OBS: os bytes estão sendo reescritos como infos de 32 bits, por isso o casting das variáveis (a info tem 20 bits)
        *umidade = 100.0*( (uint32_t) buf[1] <<12 | (uint32_t) buf[2] <<4 | (uint32_t) buf[3] >> 4 )/1048576;

        // Reconstruindo os dados de temperatura (infos do 6º, 5º e metade do 4º byte que foram recebidos)
        *temperatura = 200.0*(( ((uint32_t) buf[3] & 0x0F) <<16 ) | (uint32_t) buf[4] <<8 | (uint32_t) buf[5])/1048576 - 50;
   
    }
    
    // Função para ler e converter os dados que chegam do CI BQ25622
    void capta_medicao_bq25622(float *vbat, float *ibat)
    {

        // Variáveis para manipular os dados de tensão da bateria
        uint8_t reg_vbat= {0x30}, buffer_vbat[2]; 
        uint16_t dados_brutos_vbat;

        // Lendo os dados de tensão
        i2c_write_blocking(i2c0, BQ25622_ADDR, &reg_vbat, 1, true);
        i2c_read_blocking(i2c0, BQ25622_ADDR, buffer_vbat, 2, false);

        // Convertendo os dados em notação little endian
        dados_brutos_vbat = (buffer_vbat[1] << 8) | buffer_vbat[0];

        // PEgando os bits de 1 a 12 (que contém a medição de VBAT)
        uint16_t vbat_adc = (dados_brutos_vbat >> 1) & 0x0FFF;

        // Multiplicando o valor obtido pela resolução
        *vbat = vbat_adc * 1.99e-3f;

        
        // Variáveis para manipular os dados de corrente da bateria
        uint8_t reg_ibat= {0x2A}, buffer_ibat[2]; 
        uint16_t dados_brutos_ibat;

        // Lendo os dados de tensão
        i2c_write_blocking(i2c0, BQ25622_ADDR, &reg_ibat, 1, true);
        i2c_read_blocking(i2c0, BQ25622_ADDR, buffer_ibat, 2, false);

        // Convertendo os dados em notação little endian
        dados_brutos_ibat = (buffer_ibat[1] << 8) | buffer_ibat[0];

        // Pegando os bits de 2 a 15 (que contém a medição de IBAT)
        int16_t ibat_adc =(int16_t) dados_brutos_ibat >>2;

        // Multiplicando o valor obtido pela resolução
        *ibat = (float) ibat_adc * 4.0f;

    }



/******************************Funções de callback para as interrupções do sistema****************************/

    // Função a ser chamada de forma repetitiva (aquisição periódica dos dados)
    bool aquisicao_temporizador_callback(struct repeating_timer *t)
    {   
        // Realiza a leitura e armazena nas variáveis globais
        capta_medicao_aht10(&leitura_temp, &leitura_umidade);
        capta_medicao_bq25622(&leitura_vbat, &leitura_ibat);

        // Após a leitura, seta o flag para enviar os dados para a serial
        flag_printf = 1;

        return true;

    }


// Função para a configuração 
void mudar_tela_callback(unsigned gpio, uint32_t events)
{
  
  // Verifica se a interrupção foi disparada pelo botão A (modo malha aberta)
  if(events == GPIO_IRQ_EDGE_FALL && absolute_time_diff_us(ultima_pressao_A, get_absolute_time()) > 500000)
  {
    ultima_pressao_A = get_absolute_time();
    botao = !botao;

  }
}