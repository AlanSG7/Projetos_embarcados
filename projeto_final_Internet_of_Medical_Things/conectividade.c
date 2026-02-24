/*
Embarcatech - Trilha de Software Embarcado - Projeto Final
conectividade (definião das funções)
Aluno: Alan Sovano Gomes
Matrícula: 202420110940138
Mentora: Gabriela Teixeira
*/

// Definição das bibliotecas
#include "conectividade.h"
#include "FreeRTOS.h"
#include "task.h"

// Certificado do broker
#include "ca_cert.h"

// Variáveis globais
static uint32_t counter = 0; // Contador de mensagens
static uint32_t last_time = 0; // Variável auxiliar para o tempo de publish do MQTT
char estado_botao_string[20] = "não pressionado"; // String que armazena o estado lógico do botão 
bool mqtt_conectado = false; // Verifica se a conexão MQTT foi realizada com sucesso
bool mqtt_conectando = false; // Verifica se a conexão MQTT está em processo de conexão 

// Flags para envio de mensagens e interface com "U7_projeto_final.c"
extern bool flag_queda;
extern bool flag_localizacao;
extern bool flag_desliga_alarme;
extern bool flag_liga_alarme;

// Dados GPS
extern char info2[50];
extern char info3[25];

// Dados do MAX30102
extern char info4 [25]; //BPM
extern char info5 [25]; // SpO2




/***************************************************Funções para processar as mensagens recebidas via MQTT***********************************************/


// Função de callback para tratar as mensagens recebidas em um tópico MQTT no qual a Pi Pico está inscrita
static void mqtt_pub_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) 
{
    // Criação de um buffer para receber os dados   
    static char buffer[BUFFER_SIZE]; 
    static uint16_t index = 0; 

    // Proteção contra overflow
    if (index + len >= BUFFER_SIZE) {
        index = 0;
        return;
    }

    memcpy(&buffer[index], data, len);
    index += len;

    // Se ainda não terminou a mensagem, retorna
    if (!(flags & MQTT_DATA_FLAG_LAST)) 
    {
        return;
    }

    // Mensagem completa
    buffer[index] = '\0';
    index = 0;


    // Tomada de decisão a partir da mensagem (ligar ou desligar o LED)

    if (strcmp(buffer, "desliga alarme") == 0) 
    {
        flag_desliga_alarme = true;
    }
    else if (strcmp(buffer, "liga alarme") == 0) 
    {
        flag_liga_alarme = true;
    }
    else if (strcmp(buffer, "localizacao") == 0) 
    {
        flag_localizacao = true;
    }

       /*   
    } 
    else 
    {
        // Caso a mensagem seja muito grande, mostra uma mensagem de erro
        DEBUG_printf("Mensagem muito longa, descartando.\n");
    }

*/

}

// Debug da requisição de assinatura
void mqtt_sub_request_cb(void *arg, err_t err) 
{
    DEBUG_printf("Estado da requisição de assinatura: %d\n", err);
}

// Debug da inicialização da comunicação
static void mqtt_pub_start_cb(void *arg, const char *topic, u32_t tot_len) 
{
    DEBUG_printf("Incoming message on topic: %s\n", topic);
}



/***********************************************************Funções para lidar com a inicialização do MQTT***********************************************/

// Função de inicialização para alocar memória para o cliente MQTT (retorna um ponteiro para a variável do tipo MQTT_CLIENT_T criada anteriormente)
MQTT_CLIENT_T* mqtt_client_init(void) 
{
    // Aloca a  memória para a variável struct que define o cliente MQTT
    MQTT_CLIENT_T *state = calloc(1, sizeof(MQTT_CLIENT_T)); 
    
    // Se a alocação falha, avisa e retorna um ponteiro nulo
    if (!state) 
    {
        DEBUG_printf("Falha ao alocar variável 'state'\n");
        return NULL;
    }

    // Se a alocação de memória é um sucesso, cria um ponteiro para a estrutura
    return state;
}


// Função de callback para quando o IP é encontrado via DNS
void dns_found(const char *name, const ip_addr_t *ipaddr, void *callback_arg) 
{
    // Recupera o ponteiro para o estado do cliente MQTT
    MQTT_CLIENT_T *state = (MQTT_CLIENT_T*)callback_arg;
    if (ipaddr) 
    {
        // Salva o endereço de IP na estrutura do cliente MQTT
        state->remote_addr = *ipaddr; 
        DEBUG_printf("DNS resolvido: %s\n", ip4addr_ntoa(ipaddr));
    } 
    else 
    {
        DEBUG_printf("Falha na resolução do DNS.\n");
    }
}


// Procura converter o nome do host do broker MQTT para um endereço de IP válido
void run_dns_lookup(MQTT_CLIENT_T *state) 
{
    DEBUG_printf("Realizando a resolução do DNS %s...\n", MQTT_SERVER_HOST);

    // Utiliza a função "dns_gethostbyname()" (da biblioteca lwIP) para encontrar o IP do host do broker
    if (dns_gethostbyname(MQTT_SERVER_HOST, &(state->remote_addr), dns_found, state) == ERR_INPROGRESS) 
    {
        while (state->remote_addr.addr == 0) 
        {
            // Processa eventos da rede Wi-Fi enquanto espera a resolução do DNS
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}


// Função de callback para teste da conexão MQTT
static void mqtt_conexao_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) 
{
    // Iniciou o processo de conexão
    mqtt_conectando = false;

    // DEBUG (erro de conexao)
    DEBUG_printf("MQTT status = %d\n", status);  

    MQTT_CLIENT_T *state = (MQTT_CLIENT_T *)arg;  // <- pega o contexto passado

    // No caso de conexão com sucesso, acende o led verde do módulo WI-FI
    if (status == MQTT_CONNECT_ACCEPTED) 
    {
        mqtt_conectado = true;
       // cyw43_arch_gpio_put(LED_PIN, 1);
        DEBUG_printf("MQTT conectado.\n");

        // Se ocorre a conexão com o servidor MQTT, faz a inscrição nos tópicos descritos neste trecho
        mqtt_set_inpub_callback(state->mqtt_client, mqtt_pub_start_cb, mqtt_pub_data_cb, NULL);
        mqtt_sub_unsub(state->mqtt_client, "pico_w/Alan_Sovano/recv", 0, mqtt_sub_request_cb, NULL, 1);

    } 
    else
    {
         mqtt_conectado = false;
        //cyw43_arch_gpio_put(LED_PIN, 0);
        DEBUG_printf("A conexão com o MQTT falhou.\n");

    }


}

// Função para realizar a conexão MQTT
err_t mqtt_conectar(MQTT_CLIENT_T *state) 
{
    struct mqtt_connect_client_info_t ci = {0};
    ci.client_id = ID_DO_CLIENTE;

     /* PARA IMPLEMENTAÇÕES FUTURAS
    ci.client_user = MQTT_USER;
    ci.client_pass = MQTT_PASSWORD; 

     #if MQTT_TLS
    // ===== TLS CONFIGURAÇÃO =====
    struct altcp_tls_config *tls_config;

    tls_config = altcp_tls_create_config_client(ca_cert, ca_cert_len);
    if (!tls_config) {
        DEBUG_printf("Erro ao criar TLS config\n");
        return ERR_VAL;
    }

    ci.tls_config = tls_config;  
#endif


    DEBUG_printf("Conectando ao broker MQTT com TLS...\n");
*/

    return mqtt_client_connect(state->mqtt_client, &(state->remote_addr), MQTT_SERVER_PORT, mqtt_conexao_cb, state, &ci);
}



/****************************************************Funções para fazer a publicação das mensagens*******************************************************/

// Debug da requisição de publicação
void mqtt_pub_request_cb(void *arg, err_t err) 
{
    DEBUG_printf("Estado da requisição de publicação: %d\n", err);
}


// Função de publicação do MQTT
err_t mqtt_publicar(MQTT_CLIENT_T *state) 
{

    // Verifica se passou 1 segundo
    if(absolute_time_diff_us(last_time, get_absolute_time()) >= PUB_DELAY_US)
    {
        last_time = get_absolute_time();
        char buffer[BUFFER_SIZE];
        counter += 1;

        
        // Constrói a mensagem para enviar via MQTT
        if(flag_localizacao ==false)
        {
            if(flag_queda == true)
            {
                snprintf(buffer, BUFFER_SIZE, 
                "Sistema IoMT de monitoramento (EMBARCATECH) \n"
                "HOUVE QUEDA!!! \n"
                "Localizacao: \n"
                "%s \n"
                "%s \n\n"
                "MENSAGEM NÚMERO: %u",
                info2, info3, counter);
            }
            else if(flag_queda == false && flag_liga_alarme == true)
            {
                snprintf(buffer, BUFFER_SIZE, 
                "Sistema IoMT de monitoramento (EMBARCATECH) \n"
                "%s \n"
                "%s \n"
                "ALARME LIGADO REMOTAMENTE! \n\n"
                "MENSAGEM NÚMERO: %u",
                info4, info5, counter);
            }
            else
            {    
                snprintf(buffer, BUFFER_SIZE, 
                "Sistema IoMT de monitoramento (EMBARCATECH) \n"
                "%s \n"
                "%s \n"
                "ALARMES DESLIGADOS\n\n"
                "MENSAGEM NÚMERO: %u",
                info4, info5, counter);
            }
        }
        else
        {
            snprintf(buffer, BUFFER_SIZE, 
            "LOCALIZACAO SOLICITADA! \n"
            "Localizacao: \n"
            "%s \n"
            "%s \n\n"
            "MENSAGEM NÚMERO: %u",
            info2, info3, counter);
            flag_localizacao = false;
        }

        return mqtt_publish(state->mqtt_client, "pico_w/Alan_Sovano/pub", buffer, strlen(buffer), 0, 0, mqtt_pub_request_cb, state);
    }
    else
    {
        return 0;
    }
}

