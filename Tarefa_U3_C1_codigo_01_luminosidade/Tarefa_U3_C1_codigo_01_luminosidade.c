/*
Embarcatech - Trilha de Software Embarcado - Tarefa da Unidade 3 Capítulo 1 
Questão 01 (Sensor de umidade)
Aluno: Alan Sovano Gomes
Matrícula: 202420110940138
Mentora: Gabriela Teixeira
*/

    // Bibliotecas utilizadas
    #include <stdio.h>
    #include "pico/stdlib.h"
    #include "hardware/i2c.h"
    #include "hardware/pwm.h"


    // Definindo os pinos da comunicação I2C (utilizando a I2C0 pelos pinos GP0 e GP1)
    #define I2C_PORT i2c0
    #define I2C_SDA 0
    #define I2C_SCL 1
    #define BH1750_ADDR 0x23 // Endereço do dispositivo I2C (sensor de luminosidade)

    //Defines para o PWM (servomotor) 
    #define SERVO 2 // Pino ao conectar o servo no encaixe I2C1 da placa
    
    // Variáveis de configuração do PWM.
    // f_PWM = 125*(10^6)/((DIVIDER)*(PERIOD+1))
    // Se PERIOD = 24999 e DIVIDER = 100 -> f_PWM = 50 Hz
    const uint16_t PERIOD_PWM =24999;
    const float DIVIDER_PWM = 100;

    // Variáveis auxiliares para o controle do servo motor
    float normalizacao_servo = 0.0; // VNormalização do valor de luminosidade para controle do servo
    float rotacao_servo = 0.75*(PERIOD_PWM+1); // Ponto médio da rotação (velocidade zero)
    float print_velocidade = 0;


    // Variáveis globlais
    struct repeating_timer temporizador_aquisicao;
    float leitura_luminosidade= 0.0;
    bool flag_printf = 0;

    /************************Funções para operar o sensor*******************/

    void inicializar_bh1750()
    {
        // 0x01 -> Liga o sensor; 
        // 0x07 -> Zerando o conteúdo do registrador de dados após a inicialização
        // 0x10 -> Modo de alta resolução 01 (1 lux de resolução com amostragem a cada 120 ms);
        uint8_t buf[3] = {0x01, 0x07, 0x10};

        for(int i =0; i<3; i++)
        {
            i2c_write_blocking(I2C_PORT, BH1750_ADDR, &buf[i], 1, false); 
        }
  
    }

    // Função para ler e converter os dados que chegam do sensor de luminosidade via I2C
    uint16_t capta_medicao_bh1750()
    {
        // Buffer de dados
        uint8_t buf[2];

        // Lendo a informação disponibilizada pelo sensor
        i2c_read_blocking(I2C_PORT, BH1750_ADDR, buf, 2, false);

        // Combinando os dois dados de 8 bits em um dado de 16 bits
        return (buf[0]<<8 | buf[1]);
    }

    // Função a ser chamada de forma repetitiva (aquisição periódica dos dados)
    bool aquisicao_temporizador_callback(struct repeating_timer *t)
    {   
        // Realiza a leitura e faz a convrsão para float de acordo com as orientações do datasheet
        leitura_luminosidade = (float) capta_medicao_bh1750()/1.2;

        // Após a leitura, seta o flag para enviar os dados para a serial
        flag_printf = 1;

        // Converte o valor do sensor para um valor entre 0 e 1 (54612.5 é valor máximo, em lux, que o sensor alcança)
        normalizacao_servo = (leitura_luminosidade/54612.5);

        // O valor passado para a função "pwm_set_gpio_level()" deve estar entre 1000 ms e 2000 ms
        // 1000 ms -> Faz o servo girar ao máximo para um lado (5% de duty cycle para 50 Hz)
        // 2000 -> gira o máximo para o outro lado (10% de duty cycle para 50 Hz)

        rotacao_servo = 0.05 + normalizacao_servo*0.05;

        // Usa o valor do duty_cycle no servomotor
        pwm_set_gpio_level(SERVO, rotacao_servo*(PERIOD_PWM+1));

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

        // Criando o temporizador para realizar a coleta de dados a cada 200 ms
        add_repeating_timer_ms(200, aquisicao_temporizador_callback, NULL, &temporizador_aquisicao); 

        // Inicializando o sensor de luminosidade
        inicializar_bh1750();

/******************************************Configuração do PWM***********************************************/

        // Variável de slice do PWM
        uint slice_servo;

        // Setando o pino de PWM
        gpio_set_function(SERVO, GPIO_FUNC_PWM);

        // Obtendo valor do slice
        slice_servo = pwm_gpio_to_slice_num(SERVO);

        // Definindo divisor de clock do PWM
        pwm_set_clkdiv(slice_servo, DIVIDER_PWM);
        
        // Valor do contador do PWM (período)
        pwm_set_wrap(slice_servo, PERIOD_PWM);

        // Valor inicial do pino PWM
        pwm_set_gpio_level(SERVO, 0.075*(PERIOD_PWM+1)); 

        // Habilitando o PWM no slice correspondente
        pwm_set_enabled(slice_servo, true);


    }



    int main()
    {
        // Realizando as configurações iniciais
        setup();

        // Verificação periódica para a realização do printf
        while (true) 
        {
            // Enviando o valor lido para o monitor serial
            if(flag_printf==1)
            {
                // Valor de -100% a 100% da velocidade (o sinal indica a rotação para um lado ou para o outro)
                print_velocidade = -100 + 200*normalizacao_servo ;

                printf("Leitura: %.2f lux\nVelocidade relativa: %.2f%%\n\n", leitura_luminosidade, print_velocidade);
                flag_printf = 0;
            }
            
        }

        return 0;
    }
