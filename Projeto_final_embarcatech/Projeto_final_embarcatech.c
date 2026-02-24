/*PROJETO FINAL - EMBARCATECH
Aluno: Alan Sovano Gomes
Mentora: Luana Stefany Moura dos Santos
Tema: Malha Embarcada -- uma plataforma para o estudo de sistemas de controle */


// Bibliotecas padrão do raspberry pi pico SDK
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"

// Bibliotecas utilizadas para trabalhar com o display OLED
#include "ssd1306.h"
#include "BMSPA_font.h"

// Defines para os botões, pino PWM e conversor AD
#define PUSH_BUTTON_A 5
#define PUSH_BUTTON_B 6
#define PUSH_BUTTON_X 22
#define PWM_PIN 17
#define ADC_LEITURA 2

/**********************************************Declaração de variáveis**************************************************/
// Variáveis de configuração do PWM.
// f_PWM = 125*(10^6)/((5)*(PERIOD+1))
// Se PERIOD = 2499 -> f_PWM = 10 kHZ; Se PERIOD = 24999 -> f_PWM = 1 kHz
const uint16_t PERIOD_PWM =24999;
const float DIVIDER_PWM = 5;
volatile uint16_t pwm_level = (PERIOD_PWM+1);

// Fonte sendo utilizada
const uint8_t *fonts= {BMSPA_font};

// Variáveis para controlar o modo de oeração do sistema e mostrar no display OLED
volatile absolute_time_t ultima_pressao_A = 0, ultima_pressao_B = 0, ultima_pressao_X = 0; // Armazena o tempo da última pressão dos botões.
const char *words_2[]= {"MODO SELECIONADO:","Malha aberta", "Malha fechada", "(PID digital)", "(Sem controlador)"};
volatile bool degrau = false;
volatile bool malha_fechada = false;

// Variável para armazenar a referência atual
volatile float yr = 0.0;

 // Parâmetros do controlador contínuo (PID)
const float Kp = 0.001;
const float Ki = 4.0;
const float Kd = 0.0001;

// Passando o controlador contínuo para o domínio discreto através de uma aproximação backward (gerando um PID digital)
const float Ts = 0.004;
const float s0 = Kp + Ki*Ts + Kd/Ts;
const float s1 = -Kp - 2*Kd/Ts;
const float s2 = Kd/Ts;


// Condições iniciais do sistema de controle
volatile float y = 0;
volatile float u[2] = {0.0, 0.0};
volatile float e[3] = {0.0, 0.0, 0.0};
volatile float u_pwm =0.0;

// Criando variável para configuração do temporizador repetitivo
struct repeating_timer temporizador_controle;


/**********************************************declaração de funções **************************************************/

// Função de inicialização do PWM
void setup_pwm()
{

  // Variáveis de slice
  uint slice_pwm;

  // Setando pinos de PWM
  gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);

  // Obtendo valor dos slices
  slice_pwm = pwm_gpio_to_slice_num(PWM_PIN);

  // Definindo divisor de clock do PWM
  pwm_set_clkdiv(slice_pwm, DIVIDER_PWM);
  
  // Valor do contador do PWM (período)
  pwm_set_wrap(slice_pwm, PERIOD_PWM);

  // Habilitando o PWM no slice correspondente
  pwm_set_enabled(slice_pwm, true);

  // Enviando valor inicial (0) para a porta pwm
  pwm_set_gpio_level(PWM_PIN, 0*pwm_level);


}


// Inicializando pinos de entrada e saída
void setup_gpios() {

  //Comunicação I2C
    i2c_init(i2c1, 400000);
    gpio_set_function(14, GPIO_FUNC_I2C);
    gpio_set_function(15, GPIO_FUNC_I2C);
    gpio_pull_up(14);
    gpio_pull_up(15);

  // Inicializando o pino do botão A
  gpio_init(PUSH_BUTTON_A);
  gpio_set_dir(PUSH_BUTTON_A, GPIO_IN);
  gpio_pull_up(PUSH_BUTTON_A);

   // Inicializando o pino do botão B
  gpio_init(PUSH_BUTTON_B);
  gpio_set_dir(PUSH_BUTTON_B, GPIO_IN);
  gpio_pull_up(PUSH_BUTTON_B);

   // Inicializando o pino do botão X
  gpio_init(PUSH_BUTTON_X);
  gpio_set_dir(PUSH_BUTTON_X, GPIO_IN);
  gpio_pull_up(PUSH_BUTTON_X);
  

}


// Animação inicial do programa
void animacao_inicial(void) {

  // Vetor com as strings iniciais que serão mostradas
  const char *words[]= {"MALHA EMBARCADA", "UM PROJETO" , "EMBARCATECH", "SELECIONE O MODO:", "Malha Aberta (A)", "Malha fechada (B)"};

  // Declarando variável utilizada para definir o display OLED
  ssd1306_t disp;
  disp.external_vcc=false;
  ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
  ssd1306_clear(&disp);

  // Primeira tela
  ssd1306_draw_string_with_font(&disp, 5, 30, 1, fonts, words[0]);
  ssd1306_show(&disp);
  sleep_ms(2500);
  ssd1306_clear(&disp);

  // Segunda tela
  ssd1306_draw_string_with_font(&disp, 18, 20, 1, fonts, words[1]);
  ssd1306_draw_string_with_font(&disp, 15, 35, 1, fonts, words[2]);
  ssd1306_show(&disp);
  sleep_ms(2500);
  ssd1306_clear(&disp);
        
  // Terceira tela
  ssd1306_draw_string(&disp, 0, 20, 1, words[3]);
  ssd1306_draw_string(&disp, 0, 35, 1, words[4]);
  ssd1306_draw_string(&disp, 0, 45, 1, words[5]);
  ssd1306_show(&disp);
  ssd1306_clear(&disp);
    
}

// Função para conversão do valor lido pelo conversor A/D
float adc_to_voltage(uint16_t adc_value) 
{
  // Constantes fornecidas no datasheet do RP2040
  const float conversion_factor = 3.3f / (1 << 12);  // Conversão de 12 bits (0-4095) para 0-3.3V
  float voltage = adc_value * conversion_factor;     // Converte o valor ADC para tensão
  return voltage;
}



// Função para tratar a interrupção dos botões
void mudar_operacao_callback(int gpio, uint32_t events)
{
  
  //Declarando uma variável struct que irá auxiliar na mudança dos dizeres presentes no display OLED
  ssd1306_t disp;
  disp.external_vcc=false;
  ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
  ssd1306_clear(&disp);

 
  // Verifica se a interrupção foi disparada pelo botão A (modo malha aberta)
  if(gpio==PUSH_BUTTON_A && events == GPIO_IRQ_EDGE_FALL && absolute_time_diff_us(ultima_pressao_A, get_absolute_time()) > 500000)
  {
    ultima_pressao_A = get_absolute_time();
    ssd1306_draw_string(&disp, 15, 20, 1, words_2[0]);
    ssd1306_draw_string(&disp, 15, 35, 1, words_2[1]);
    ssd1306_draw_string(&disp, 15, 50, 1, words_2[4]);
    ssd1306_show(&disp);
    ssd1306_clear(&disp);
    malha_fechada = false;

  }

  // Verifica se a interrupção foi disparada pelo botão B (modo malha fechada)
  else if(gpio==PUSH_BUTTON_B && events == GPIO_IRQ_EDGE_FALL && absolute_time_diff_us(ultima_pressao_B, get_absolute_time()) > 500000)
  {
    ultima_pressao_B = get_absolute_time();
    ssd1306_draw_string(&disp, 15, 20, 1, words_2[0]);
    ssd1306_draw_string(&disp, 15, 35, 1, words_2[2]);
    ssd1306_draw_string(&disp, 15, 50, 1, words_2[3]);
    ssd1306_show(&disp);
    ssd1306_clear(&disp);
    malha_fechada = true;
    
  }

  // Verifica qual o valor de referência para o degrau (0 ou 1)
  else if(gpio==PUSH_BUTTON_X && events == GPIO_IRQ_EDGE_FALL && absolute_time_diff_us(ultima_pressao_X, get_absolute_time()) > 500000)
  {
    ultima_pressao_X = get_absolute_time();
    degrau = !degrau;

    if(degrau==false)
    {
      yr = 0.0;
    }
    else
    {
      yr = 1.0;
    }
    
  }

}


// Laço com o algoritmo de controle (o qual deve rodar no período de amostragem de 4 ms)
bool controle_temporizador_callback(struct repeating_timer *t)
{
 // Calculando o erro de seguimento de referência e o sinal de controle

  if(malha_fechada==true)
  {
    // Lendo sinal de saída do sistema
    uint16_t adc_value = adc_read();
    // Convertendo os valores lidos pelo ADC para tensão 
    y = adc_to_voltage(adc_value);

    // Calculando sinal de erro e sinal de controle
    e[0] = yr - y;
    u[0] = u[1] +s0*e[0] +s1*e[1] +s2*e[2];

    // Saturando o sinal de controle
    if(u[0]<0)
    {
      u[0]=0;
    }
            
    if(u[0]>3.3)
    {
      u[0]=3.3;
    }

    // Calculando nível do PWM referente ao sinal de controle
    u_pwm = 303*u[0]/1000;

    // Atualizando os erros e os sinais de controle
    e[2] = e[1];
    e[1] = e[0];
    u[1] = u[0];

    // Enviando o sinal de controle para o sistema a ser controlado
    pwm_set_gpio_level(PWM_PIN, u_pwm*pwm_level);

    // Enviando os dados pela porta USB
    printf("%.2f,%.2f\n",y,u[0]);

  }
  else
  {
    // Sinal de saída sendo lido e convertido para volts
    uint16_t adc_value = adc_read();
    y = adc_to_voltage(adc_value);

    // Enviando a referência para o sistema (malha aberta)
    u[0] = yr;

    // Atualizando o sinal de entrada
    u[1] = u[0];

    // Enviando o sinal de controle para o sistema
    u_pwm = 303*u[0]/1000;
    pwm_set_gpio_level(PWM_PIN, u_pwm*pwm_level);

    // Enviando o sinal de saída e de controle para a comunicação serial
    printf("%.2f,%.2f\n",y, u[0]);

  }                
        
}


/*****************************************************Função main*********************************************************************/
int main() 
{
  // Inicializando as entradas e saída padrões
  stdio_init_all();

  // Configurando pinos de entrada e saída para a comunicação I2C
  setup_gpios();

  // Animação inicial
  animacao_inicial();

  //inicializando PWM
  setup_pwm();

  // inicializando ADC
  adc_init();
  adc_gpio_init(28); // Garantindo que o pino 28 (ADC) está em alta impedância (sem pull_up ou pull_down)
  adc_select_input(ADC_LEITURA);

  // Configurando a interrupção gerada pelo botão A, B e X
  gpio_set_irq_enabled_with_callback(PUSH_BUTTON_A, GPIO_IRQ_EDGE_FALL, true, mudar_operacao_callback);
  gpio_set_irq_enabled_with_callback(PUSH_BUTTON_B, GPIO_IRQ_EDGE_FALL, true, mudar_operacao_callback);
  gpio_set_irq_enabled_with_callback(PUSH_BUTTON_X, GPIO_IRQ_EDGE_FALL, true, mudar_operacao_callback);

  // Cria o evento repetitivo. calcula e envia o sinal de controle a cada Ts períodos de tempo
  add_repeating_timer_ms(4, controle_temporizador_callback, NULL, &temporizador_controle);

  // Loop infinito
  while(true)
  {
    tight_loop_contents(); // Função que otimiza o loop vazio para evitar consumo excessivo de CPU.  
  }
    

  return 0;

}




