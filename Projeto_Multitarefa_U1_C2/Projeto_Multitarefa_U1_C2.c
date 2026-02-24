/*
Embarcatech - Trilha de Software Embarcado - Tarefa da Unidade 1 Capítulo 2
Aluno: Alan Sovano Gomes
Matrícula: 202420110940138
Mentora: Gabriela Teixeira
*/


/********************************** Declaração de bibliotecas, macros e variáveis globais ****************************************/

// Bibliotecas a serem utilizadas
#include <stdio.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Definindo os pinos do microcontrolador para o botão e o LED
#define BOTAO 5 // Botão A da placa BitDogLab
#define LED 11 // Pino referente à cor verde do LED RGB da placa BitDogLab

// Variáveis de estado do botão e do LED
bool estado_botao = 1;
bool estado_botao_anterior = 1;
bool valor_botao_detectado = 1;
bool estado_led = 0;

// Identificador da task que irá controlar o estado do LED
TaskHandle_t controle_LED_task_handle;

// Declarando a fila a ser utilizada para a sincronização entre tarfas
QueueHandle_t queue_estado_botao;

/********************************************* Declarando as tasks do problema ***************************************************/

// Task 1: Leitura do botão
void leitura_botao_task(void *pvParameters)
{
    // Laço inifnito da task
    for(;;)
    {
        //printf("Task 1 em execução!\n"); // Printf utilizado para depuração do código

        valor_botao_detectado = gpio_get(BOTAO); // Captura o estado do botão
        xQueueSend(queue_estado_botao, &valor_botao_detectado, portMAX_DELAY); // Coloca o estado do botão capturado em uma fila
        vTaskDelay(pdMS_TO_TICKS(100)); // delay de 100 ms para a task
    }

}

// Task 2: Processamento e decisão
void processamento_botao_task(void *pvParameters)
{
    for(;;)
    {
        //printf("Task 2 em execução!\n"); // Printf utilizado para depuração do código 

        // Captura o estado do botão de uma fila
        xQueueReceive(queue_estado_botao, &estado_botao, portMAX_DELAY);

        // Acessa a condição quando houver uma borda de descida do botão (pressionar do botão)
        // OBS: o botão está com resistor de pull-up (ou seja, está em nível lógico alto quando não pressionado).
        if(estado_botao_anterior ==1 && estado_botao==0)
        {
            // Envia uma notificação para a tarefa controle_LED_task()
            xTaskNotifyGive(controle_LED_task_handle);
        }

        // atualizando valor das variáveis
        estado_botao_anterior = estado_botao;

        // Delay para a task (para evitar que o processador fique sobrecarregado com ela)
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

// Task 3: Controle do LED
void controle_LED_task(void *pvParameters)
{
    for(;;)
    {
        //printf("Task 3 em execução!\n"); // Printf utilizado para depuração do código

        // Aguarda indefinidamente a notificação da tarefa "processamento_botao_task()"
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Alternando o estado do LED com o pressionar do botão
        estado_led = !estado_led;

        // Alterando o estado do LED
        gpio_put(LED, estado_led);

    }
}


/*********************************************** Funções principais do código *****************************************************/

// Função para configuração inicial do sistema (setup)
void setup()
{
        // Inicialização da comunicação padrão do Pi Pico
        stdio_init_all();

        // Inicializando o pino do botão
        gpio_init(BOTAO);
        gpio_set_dir(BOTAO, GPIO_IN);
        gpio_pull_up(BOTAO);
    
        // Inicializando o pino do LED
        gpio_init(LED);
        gpio_set_dir(LED, GPIO_OUT);    

}

// Declaração da função principal (main)
int main()
{
    // Configurando o sistema
    setup();

    // Criando a fila de dados booleanos de tamanho arbitrário 5 (armazena os estados do botão)
    queue_estado_botao = xQueueCreate(5, sizeof(bool));
    
    // Criando as tasks (em caso de sucesso de criação da fila de dados)
    if(queue_estado_botao != NULL)
    {
        xTaskCreate(leitura_botao_task,"tarefa leitura botao", 128, NULL, 1, NULL); // Task com prioridade 1 (baixa)
        xTaskCreate(processamento_botao_task, "tarefa processamento botao", 128, NULL, 2, NULL); // Task com prioridade 2 (média)
        xTaskCreate(controle_LED_task, "tarefa controle LED", 128, NULL, 3, &controle_LED_task_handle); // Task com prioridade 3 (alta)
    
        // Iniciando o escalonador de tarefas
        vTaskStartScheduler();
    }

    // Loop infinito para ser acessado no caso de algum tipo de falha na inicialização do escalonador de tarefas
    while (true) 
    {
        tight_loop_contents(); // Função que otimiza o loop vazio para evitar consumo excessivo de CPU.     
    }
}
