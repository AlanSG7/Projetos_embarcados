/*
Embarcatech - Trilha de Software Embarcado - Projeto Final
conectividade (header)
Aluno: Alan Sovano Gomes
Matrícula: 202420110940138
Mentora: Gabriela Teixeira
*/

// Definição das bibliotecas
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

// Biblioteca utilizada para trabalhar com o display OLED
#include "ssd1306.h"

// Biblioteca para geração de uma pilha de protocolos TCP/IP leve e uso do MQTT
#include "lwip/pbuf.h"      //
#include "lwip/tcp.h"       // Comunicação via protocolo TCP
#include "lwip/dns.h"       // Implementa DNS (transforma o nome do domínio em um IP)
#include "lwip/apps/mqtt.h" // API para MQTT (fornece as funções básicas para trabalhar com MQTT)
#include "lwip/altcp_tcp.h" // TCP alternativo (permite o uso de TCP com TSL/SSL sem mudanças no código da aplicação)
#include "lwip/altcp_tls.h" // Complemento da "ALTCP" (para oferecer suporte À TLS/SSL, permitindo conexões mais seguras)

// Macros para se trabalhar com o MQTT
#define DEBUG_printf  // Macro para ajudar a habilitar/desabilitar "printf" utilizados somente para debug
//#define MQTT_SERVER_HOST "broker.hivemq.com" // Endereço do servidor MQTT (HiveMQ)
#define MQTT_SERVER_HOST "broker.emqx.io" // Endereço do servidor MQTT (EMQX)
#define MQTT_SERVER_PORT 1883 // Porta de comunicação com o servidor MQTT
#define MQTT_TLS 0 // Ativação ou não do protocolo de segurança TLS
#define BUFFER_SIZE 1024 // Tamanho do buffer para enviar/receber a mensagem
#define ID_DO_CLIENTE "Pico_W_Alan_Sovano" // ID utilizado para comunicação MQTT
#define PUB_DELAY_US 1000000 // Tempo de espera (microssegundos) para o laço de publish do MQTT

// Usuário e senha para acessar o broker
//#define MQTT_USER        "IOMT_EMBARCATECH"
//#define MQTT_PASSWORD    "alansovano@nota10"

// Configurações da rede wi-fi que será utilizada (ALTERAR AQUI SE MUDAR A REDE!!!) 
#define WIFI_SSID "A54 de Alan"
#define WIFI_PASSWORD "testehoje"

// Macros para elementos de hardware
#define LED_PIN_B 12 // Pino do led RGB (cor azul)
#define LED_PIN_R 13 // Pino do led RGB (cor red)
#define BUZZER 21 // Pino do buzzer
#define BUTTON5_PIN 5 // Pino do botão A
#define LED_PIN CYW43_WL_GPIO_LED_PIN // Pino referente ao LED do módulo Wi-Fi


// Define uma variável do tipo struct para lidar com parâmetros de cliente MQTT
typedef struct MQTT_CLIENT_T_ 
{
    ip_addr_t remote_addr;
    mqtt_client_t *mqtt_client;
    u32_t received;
    u32_t counter;
    u32_t reconnect;
} MQTT_CLIENT_T;


MQTT_CLIENT_T* mqtt_client_init(void);
void run_dns_lookup(MQTT_CLIENT_T *state);
err_t mqtt_conectar(MQTT_CLIENT_T *state);
err_t mqtt_publicar(MQTT_CLIENT_T *state);