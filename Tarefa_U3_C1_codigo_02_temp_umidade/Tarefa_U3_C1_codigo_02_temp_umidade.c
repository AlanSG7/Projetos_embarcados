/*
Embarcatech - Trilha de Software Embarcado - Tarefa da Unidade 3 Capítulo 1 
Questão 02 (Sensor de temperatura e umidade)
Aluno: Alan Sovano Gomes
Matrícula: 202420110940138
Mentora: Gabriela Teixeira
*/
    // Bibliotecas utilizadas
    #include <stdio.h>
    #include "pico/stdlib.h"
    #include "hardware/i2c.h"
    #include "hardware/pwm.h"

    // Biblioteca utilizada para trabalhar com o display OLED
    #include "ssd1306.h"

    // Definindo os pinos da comunicação I2C (utilizando a I2C0 pelos pinos GP0 e GP1)
    #define I2C_PORT i2c0
    #define I2C_SDA 0
    #define I2C_SCL 1
    #define AHT10_ADDR 0x38 // Endereço do dispositivo I2C (temperatura e umidade)

    // Limites para avisos no display OLED (MUDAR AQUI PARA DEPURAÇÃO E TESTES DA INTERFACE!)
    #define LIMIAR_TEMP 20
    #define LIMIAR_UMIDADE 70

    // Variáveis globlais
    struct repeating_timer temporizador_aquisicao;
    float leitura_temp = 0.0;
    float leitura_umidade = 0.0;
    bool flag_printf = 0;
    const uint8_t comando_sensor[3] = {0xAC, 0x33, 0x00}; // Sequência de inicialização da leitura
    const char *words[]= {"TEMPERATURA BAIXA!", "UMIDADE ALTA!","Dados recebidos: "}; // Strings utilizadas no display OLED
   
    // Guardam os dados que serão mostrados no display OLED
    char info [20]; 
    char info2 [25];


    /************************Funções para operar o sensor*******************/

    void inicializar_aht10()
    {
        // 0xE1 -> Inicializa o sensor; 
        // 0x08 -> Comando que envolve a calibração do sensor;
        // 0x00 -> Byte extra contendo zeros (padding)
        uint8_t buf[3] = {0xE1, 0x08, 0x00};

        // Enviando os bytes pelo barramento I2C
        i2c_write_blocking(I2C_PORT, AHT10_ADDR, buf, 3, false); 
  
    }

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

    // Função a ser chamada de forma repetitiva (aquisição periódica dos dados)
    bool aquisicao_temporizador_callback(struct repeating_timer *t)
    {   
        // Realiza a leitura e armazena nas variáveis globais
        capta_medicao_aht10(&leitura_temp, &leitura_umidade);

        // Após a leitura, seta o flag para enviar os dados para a serial
        flag_printf = 1;

        return true;

    }


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

        // Inicialização da comunicação I2C com uma velocidade de 400 kHz
        i2c_init(I2C_PORT, 400*1000);


        // Criando o temporizador para realizar a coleta de dados a cada 1000 ms
        add_repeating_timer_ms(1000, aquisicao_temporizador_callback, NULL, &temporizador_aquisicao); 

        // Inicializando o sensor de luminosidade
        inicializar_aht10();

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

    }


    int main()
    {
        // Realizando as configurações iniciais
        setup();

        // Arquivo para salvar as informações
        char* dados = "dados_datalogger_embarcasense.txt";

        // Declarando variável utilizada para definir o display OLED
        ssd1306_t disp;
        disp.external_vcc=false;
        ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
        ssd1306_clear(&disp);


        // Verificação periódica para a realização do printf
        while (true) 
        {
            // Enviando o valor lido para o monitor serial
            if(flag_printf==1)
            {

                // Mostrando no monitor serial e guardando as infos em strings
                printf("Temperatura: %.2f C\nUmidade relativa: %.2f%%\n\n", leitura_temp, leitura_umidade);
                sprintf(info, "Umidade: %.2f%%", leitura_umidade);
                sprintf(info2, "Temperatura: %.2f C", leitura_temp);


                // Escrevendo a mensagem desejada no display OLED:
                ssd1306_draw_string(&disp, 0, 5, 1, words[2]);
                ssd1306_draw_string(&disp, 0, 15, 1, info);
                ssd1306_draw_string(&disp, 0, 25, 1, info2);

                // Checando se a temperatura está abaixo do limiar crítico
                if(leitura_temp<LIMIAR_TEMP)
                {
                    ssd1306_draw_string(&disp, 0, 45, 1, words[0]);
                }

                // Checando se a temperatura está acima do limiar crítico
                if(leitura_umidade>LIMIAR_UMIDADE)
                {
                    ssd1306_draw_string(&disp, 0, 55, 1, words[1]);
                }

                ssd1306_show(&disp);
                ssd1306_clear(&disp);   

                flag_printf = 0;
            }
            
        }

        return 0;
    }
