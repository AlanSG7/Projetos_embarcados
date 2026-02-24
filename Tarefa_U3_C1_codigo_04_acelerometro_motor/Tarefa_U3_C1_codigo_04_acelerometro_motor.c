/*
Embarcatech - Trilha de Software Embarcado - Tarefa da Unidade 3 Capítulo 1 
Questão 04 (sensor MPU 6050)
Aluno: Alan Sovano Gomes
Matrícula: 202420110940138
Mentora: Gabriela Teixeira
*/
    // Bibliotecas utilizadas
    #include <stdio.h>
    #include "pico/stdlib.h"
    #include "hardware/i2c.h"
    #include "hardware/pwm.h"
    #include <math.h>

    // Biblioteca utilizada para trabalhar com o display OLED
    #include "ssd1306.h"


    // Definindo os pinos da comunicação I2C (utilizando a I2C1 pelos pinos GP2 e GP3)
    #define I2C_PORT i2c0
    #define I2C_SDA 0
    #define I2C_SCL 1
    #define MPU6050_ADDR 0x68 // Endereço do dispositivo I2C (MPU 6050)

    //Defines para o PWM (servomotor) 
    #define SERVO 28 // Pino ao conectar o servo no canto superior direito do conector preto da placa
    
    // Variáveis de configuração do PWM.
    // f_PWM = 125*(10^6)/((DIVIDER)*(PERIOD+1))
    // Se PERIOD = 24999 e DIVIDER = 100 -> f_PWM = 50 Hz
    const uint16_t PERIOD_PWM =24999;
    const float DIVIDER_PWM = 100;
    float normalizacao_servo = 0.0;
    float rotacao_servo = 0.75*(PERIOD_PWM+1);
    float print_velocidade = 0;


    // Variáveis globlais
    struct repeating_timer temporizador_aquisicao;
    bool flag_printf = 0;
    float leitura_ac_x = 0.0, leitura_ac_y = 0.0, leitura_ac_z = 0.0;
    float leitura_gyro_x = 0.0, leitura_gyro_y = 0.0, leitura_gyro_z = 0.0;
    float calc_phi = 0.0;
    const char *words[]= {"ANGULO DE INCLINACAO:", "ANGULO MAIOR QUE 90!"}; // Strings utilizadas no display OLED
   
    // Guardam os dados que serão mostrados no display OLED
    char info [20]; 


    /************************Funções para operar o sensor*******************/

    void inicializar_mpu6050()
    {
        // Resetando o dispositivo
        uint8_t buf[2] = {0x6B, 0x80};
        
        for(int i =0; i<1; i++)
        {
            i2c_write_blocking(I2C_PORT, MPU6050_ADDR, &buf[i], 1, false); 
        }
        sleep_ms(100); // Espera o dispositivo ser estabilizado após o reset


        // Tirando o dispositivo do modo "sleep"
        buf[1] = 0x00;
        
        for(int i =0; i<1; i++)
        {
            i2c_write_blocking(I2C_PORT, MPU6050_ADDR, &buf[i], 1, false); 
        }
        sleep_ms(10); // Espera a tensão dos pinos do dispositivo estabilizar 
  
    }

   
    // Função para ler e converter os dados que chegam da MPU 6050 via I2C
    void capta_medicao_mpu6050(float *ac_x, float *ac_y, float *ac_z, float *gyro_x, float *gyro_y, float *gyro_z, float *phi)
    {

        // Buffer de dados (acelerômetro e giroscopio)
        uint8_t buf_ac[6];
        uint8_t buf_gyro[6];

        // Endereços dos registradores iniciais (ocorre um autoincremento após a leitura)
        uint8_t reg_ac_end = 0x3B;
        uint8_t reg_gyro_end = 0x43;

        // Capturando a informação do acelerômetro (16 bits em complemento de 2)
        i2c_write_blocking(I2C_PORT, MPU6050_ADDR, &reg_ac_end, 1, true);
        i2c_read_blocking(I2C_PORT, MPU6050_ADDR, buf_ac, 6, false);

        // Capturando a informação do giroscópio
        i2c_write_blocking(I2C_PORT, MPU6050_ADDR, &reg_gyro_end, 1, true);
        i2c_read_blocking(I2C_PORT, MPU6050_ADDR, buf_gyro, 6, false);

        // Conversão dos dados do acelerômetro para múltiplos de "g" (aceleração da gravidade)
        // O fator de sensibilidade é 16384 (valor padrão)
        *ac_x = (float) ( (int16_t) (buf_ac[0]<<8 | buf_ac[1]) )/16384; // buf_ac[0] e buf_ac[1]
        *ac_y = (float) ( (int16_t) (buf_ac[2]<<8 | buf_ac[3]) )/16384; // buf_ac[2] e buf_ac[3]
        *ac_z = (float) ( (int16_t) (buf_ac[4]<<8 | buf_ac[5]) )/16384; // buf_ac[4] e buf_ac[5]

        // Conversão dos dados do giroscópio para múltiplos de º/s (graus por segundo)
        // O fator de sensibilidade é 131 (valor padrão)       
        *gyro_x = (float) ( (int16_t) (buf_gyro[0]<<8 | buf_gyro[1]) )/131; // buf_gyro[0] e buf_gyro[1]
        *gyro_y = (float) ( (int16_t) (buf_gyro[2]<<8 | buf_gyro[3]) )/131; // buf_gyro[2] e buf_gyro[3]
        *gyro_z = (float) ( (int16_t) (buf_gyro[4]<<8 | buf_gyro[5]) )/131; // buf_gyro[4] e buf_gyro[5]

        // Calcula o ângulo de inclinação, convertendo de radianos para segundos (phi é a incliniação a partir do eixo z)
        *phi = 57.29*acos(*ac_z/sqrt(pow(*ac_x,2) + pow(*ac_y,2) + pow(*ac_z,2)));


    }

    // Função a ser chamada de forma repetitiva (aquisição periódica dos dados)
    bool aquisicao_temporizador_callback(struct repeating_timer *t)
    {   
        // Realiza a leitura e faz a convrsão para float de acordo com as orientações do datasheet
        capta_medicao_mpu6050(&leitura_ac_x, &leitura_ac_y, &leitura_ac_z, &leitura_gyro_x, &leitura_gyro_y, &leitura_gyro_z, &calc_phi);

        // Após a leitura, seta o flag para enviar os dados para a serial
        flag_printf = 1;

        // Converte o valor do sensor para um valor entre 0 e 1
        normalizacao_servo = calc_phi/180;

        // O valor passado para a função "pwm_set_gpio_level()" deve estar entre 1000 ms e 2000 ms
        // 1000 ms -> Faz o servo girar ao máximo para um lado (5% de duty cycle para 50 Hz)
        // 2000 ms -> gira o máximo para o outro lado (10% de duty cycle para 50 Hz)

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

        // Inicialização da comunicação I2C com uma velocidade de 100 kHz
        i2c_init(I2C_PORT, 100*1000);


        // Criando o temporizador para realizar a coleta de dados a cada 50 ms (20 Hz)
        add_repeating_timer_ms(50, aquisicao_temporizador_callback, NULL, &temporizador_aquisicao); 

        // Inicializando o acelerômetro/giroscópio
        inicializar_mpu6050();

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


/******************************************Configuração do OLED***********************************************/

        //Comunicação I2C do OLED
        i2c_init(i2c1, 100*1000);
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
                print_velocidade = -100 + 200*normalizacao_servo ;
                printf("Leitura acelerometro (x,y,z): %.2f g, %.2f g, %.2f g\nLeitura giroscopio (x,y,z): %.2f º/s, %.2f º/s, %.2f º/s\nAngulo phi = %.2fº\nVelocidade do servo: %.2f%%\n\n", leitura_ac_x, leitura_ac_y, leitura_ac_z, leitura_gyro_x, leitura_gyro_y, leitura_gyro_z, calc_phi, print_velocidade);

                // Escrevendo a mensagem desejada no display OLED:
                sprintf(info, "%.2f graus", calc_phi);
                ssd1306_draw_string(&disp, 0, 10, 1, words[0]);
                ssd1306_draw_string(&disp, 25, 25, 1, info);

                // Checando se o ângulo está acima de um limiar crítico
                if(calc_phi>90)
                {
                    ssd1306_draw_string(&disp, 0, 40, 1, words[1]);
                }

                ssd1306_show(&disp);
                ssd1306_clear(&disp);

                flag_printf = 0;
            }
            
        }

        return 0;
    }
