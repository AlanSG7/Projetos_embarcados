/*
Embarcatech - Trilha de Software Embarcado - Tarefa da Unidade 2 Capítulo 3
Aluno: Alan Sovano Gomes
Matrícula: 202420110940138
Mentora: Gabriela Teixeira
*/

// Definição das bibliotecas
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"

// Biblioteca para geração de uma pilha de protocolos TCP/IP leve
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h" 

// Configurações da rede wi-fi que será utilizada (ALTERAR AQUI SE MUDAR A REDE!!!) 
#define WIFI_SSID "A54 de Alan"
#define WIFI_PASSWORD "testehoje"


// Definindo os pinos do microcontrolador para o botão e o LED
#define BOTAO 5 // Botão A da placa BitDogLab
#define LED 11 // Pino referente à cor verde do LED RGB da placa BitDogLab
#define LED_PIN CYW43_WL_GPIO_LED_PIN // Pino referente ao LED do módulo Wi-Fi

// Variáveis globais
bool estado_botao = 1; // Variável lógica do botão
uint16_t raw_value; // Variável que irá receber os dados do sensor de temperatura
const float conversion_factor = 3.3f / (1 << 12); // Fator de conversão para o valor de temperatura lido
float temperature; // Variável que armazena a temperatura convertida pra graus Celsius
int joyx, joyy; // Variáveis para armazenar os valores x e y do joystick
char rosa_dos_ventos[20] ="Centro"; // String que armazena a direção da rosa dos ventos
char estado_botao_string[20] = "não pressionado"; // String que armazena o estado lógico do botão 



// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    // Verifica se tem algo no buffer para ser processado
    if (!p)
    {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    // Alocação de memória 
    char *request = (char *)malloc(p->len + 1);

 // Se a alocação de memória falhar, limpa o buffer e retorna um erro  
if (!request) {
    pbuf_free(p);
    return ERR_MEM;
};

    // Copia os dados da requisição para a memória recém alocada
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';


    // Leitura do botão
    estado_botao = gpio_get(BOTAO);
    gpio_put(LED, !estado_botao);

    // Leitura da temperatura interna
    adc_select_input(4);
    raw_value = adc_read();
    temperature = 27.0f - ((raw_value * conversion_factor) - 0.706f) / 0.001721f;

    // Leitura do joystick
    adc_select_input(0);
    joyy = adc_read();
      adc_select_input(1);
    joyx = adc_read();

    // Definindo uma string para descrever o estado do botão
    if(estado_botao==1)
    {
        strcpy(estado_botao_string,"não pressionado");
    }
    else
    {
        strcpy(estado_botao_string,"pressionado");
    }

    // Definindo a posição na rosa dos ventos
    /* 
    OBS: Após uma avaliação empírica, os intervalos [0-1259], [1260-2259] e [2260-4095] foram selecionados 
    para definir a direção da rosa dos ventos a partir da posição do joystick
    */
    if(joyy<1260)
    {
        if(joyx<1260)
        {
            strcpy(rosa_dos_ventos,"Sudoeste (SW)");
        }
        else
        {
            if(joyx>2060)
            {
                strcpy(rosa_dos_ventos,"Sudeste (SE)");
            }
            else
            {
                strcpy(rosa_dos_ventos,"Sul (S)");
            };

        };

    }
    else
    {
        if(joyy>2260)
        {
            if(joyx<1260)
            {
                strcpy(rosa_dos_ventos,"Noroeste (NW)");
            }
            else
            {
                if(joyx>2260)
                {
                    strcpy(rosa_dos_ventos,"Nordeste (NE)");
                }
                else
                {
                    strcpy(rosa_dos_ventos,"Norte (N)");
                };
            };

        }
        else
        {
            if(joyx<1260)
            {
                strcpy(rosa_dos_ventos,"Oeste (W)");
            }
            else
            {
                if(joyx>2260)
                {
                    strcpy(rosa_dos_ventos,"Leste (E)");
                }
                else
                {
                    strcpy(rosa_dos_ventos,"Centro ");
                };
            };


        };

    };
    
    
    // Criando a resposta HTML
    char html[8192];
    
    snprintf(html, sizeof(html),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head>\n"
             "<meta charset=\"utf-8\">\n"
            "<title>Tarefa U2 C3 Alan</title>\n"

             // Estrutura geral da página
            "<style>\n"
            "body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }\n"
            "h1 { font-size: 64px; margin-bottom: 30px; }\n"
            "button { font-size: 36px; margin: 10px; padding: 20px 40px; border-radius: 10px; }\n"
            ".temperature { font-size: 48px; margin-top: 30px; color: #dc2c2c; }\n"
            ".botao{ font-size: 48px; margin-top: 30px; color: #322cdc; }\n"
            ".rosaventos{ font-size: 48px; margin-top: 30px; color: #008011; }\n"
            "</style>\n"
            "</head>\n"
            "<body>\n"
            "<h1>Leitura do botão <br> e sensor de temperatura</h1>\n"
            "<p class=\"botao\">Estado lógico do botão: %s </p>\n"
            "<p class=\"temperature\">Temperatura Interna: %.2f &deg;C</p>\n"
            "<br> <br>\n"
            "<h1>Rosa dos ventos <br> (joystick) </h1>\n"
            "<p class=\"rosaventos\">Coordenada x: %d </p>\n"
            "<p class=\"rosaventos\">Coordenada y: %d </p>\n"
            "<p class=\"rosaventos\">Direção: %s </p>\n"


            // Atualização da página inteira a cada 1 segundo (javascript)
            "<script>\n"
            "setInterval(function() { location.reload(); }, 1000);\n" 
            "</script>\n"
            

             // Desenho da rosa dos ventos
            "<div>\n"
            "<svg width=\"800\" height=\"600\" xmlns=\"http://www.w3.org/2000/svg\">\n"
            "<g>\n"
            "<title>Layer 1</title>\n"
            "<path stroke=\"#000\" id=\"svg_2\" d=\"m393.90006,381.86675c-94.1989,0 -170.5,-74.51105 -170.5,-166.5c0,-91.98895 76.30111,-166.5 170.5,-166.5c94.1989,0 170.5,74.51105 170.5,166.5c0,91.98895 -76.30111,166.5 -170.5,166.5z\" opacity=\"undefined\" fill=\"none\"/>\n"
            "<line stroke-linecap=\"undefined\" stroke-linejoin=\"undefined\" id=\"svg_3\" y2=\"381.86675\" x2=\"396.40005\" y1=\"47.86675\" x1=\"394.40005\" stroke=\"#000\" fill=\"none\"/>\n"
            "<path id=\"svg_4\" d=\"m222.40006,214.86675l341.15388,0\" opacity=\"undefined\" stroke-linecap=\"undefined\" stroke-linejoin=\"undefined\" stroke=\"#000\" fill=\"none\"/>\n"
            "<path id=\"svg_5\" d=\"m279.15005,92.61675l232,244\" opacity=\"undefined\" stroke-linecap=\"undefined\" stroke-linejoin=\"undefined\" stroke=\"#000\" fill=\"none\"/>\n"
            "<path id=\"svg_6\" d=\"m510.40005,94.86675l-233.49581,241.99162\" opacity=\"undefined\" stroke-linecap=\"undefined\" stroke-linejoin=\"undefined\" stroke=\"#000\" fill=\"none\"/>\n"
            "<text xml:space=\"preserve\" text-anchor=\"start\" font-family=\"Noto Sans JP\" font-size=\"24\" stroke-width=\"0\" id=\"svg_7\" y=\"33.86675\" x=\"387.40005\" stroke=\"#000\" fill=\"#000000\">N</text>\n"
            "<text xml:space=\"preserve\" text-anchor=\"start\" font-family=\"Noto Sans JP\" font-size=\"24\" stroke-width=\"0\" id=\"svg_8\" y=\"408.86675\" x=\"387.40005\" stroke=\"#000\" fill=\"#000000\">S</text>\n"
            "<text xml:space=\"preserve\" text-anchor=\"start\" font-family=\"Noto Sans JP\" font-size=\"24\" stroke-width=\"0\" id=\"svg_9\" y=\"33.86675\" x=\"387.40005\" stroke=\"#000\" fill=\"#000000\">N</text>\n"
            "<text xml:space=\"preserve\" text-anchor=\"start\" font-family=\"Noto Sans JP\" font-size=\"24\" id=\"svg_10\" y=\"220.86675\" x=\"188.40006\" stroke-width=\"0\" stroke=\"#000\" fill=\"#000000\">W</text>\n"
            "<text xml:space=\"preserve\" text-anchor=\"start\" font-family=\"Noto Sans JP\" font-size=\"24\" id=\"svg_11\" y=\"221.86675\" x=\"578.40005\" stroke-width=\"0\" stroke=\"#000\" fill=\"#000000\">E</text>\n"
            "<text xml:space=\"preserve\" text-anchor=\"start\" font-family=\"Noto Sans JP\" font-size=\"24\" id=\"svg_12\" y=\"76.26676\" x=\"236.00005\" stroke-width=\"0\" stroke=\"#000\" fill=\"#000000\">NW</text>\n"
            "<text xml:space=\"preserve\" text-anchor=\"start\" font-family=\"Noto Sans JP\" font-size=\"24\" id=\"svg_14\" y=\"75.26676\" x=\"513.00005\" stroke-width=\"0\" stroke=\"#000\" fill=\"#000000\">NE</text>\n"
            "<text style=\"cursor: move;\" xml:space=\"preserve\" text-anchor=\"start\" font-family=\"Noto Sans JP\" font-size=\"24\" id=\"svg_17\" y=\"365.86675\" x=\"519.40005\" stroke-width=\"0\" stroke=\"#000\" fill=\"#000000\">SE</text>\n"
            "<text xml:space=\"preserve\" text-anchor=\"start\" font-family=\"Noto Sans JP\" font-size=\"24\" id=\"svg_18\" y=\"366.86675\" x=\"237.40005\" stroke-width=\"0\" stroke=\"#000\" fill=\"#000000\">SW</text>\n"
            "<line id=\"svg_19\" y2=\"61.26676\" x2=\"384.00005\" y1=\"48.26676\" x1=\"394.00005\" stroke=\"#000\" fill=\"none\"/>\n"
            "<line id=\"svg_20\" y2=\"58.83016\" x2=\"404.04925\" y1=\"48.27468\" x1=\"394.04933\" stroke=\"#000\" fill=\"none\"/>\n"
            "<line id=\"svg_21\" y2=\"373.29136\" x2=\"406.81214\" y1=\"382.04136\" x1=\"396.49965\" stroke=\"#000\" fill=\"none\"/>\n"
            "<line id=\"svg_22\" y2=\"372.97886\" x2=\"385.24965\" y1=\"382.04136\" x1=\"396.49965\" stroke=\"#000\" fill=\"none\"/>\n"
            "<line id=\"svg_23\" y2=\"206.41618\" x2=\"231.8718\" y1=\"214.72381\" x1=\"222.94879\" stroke=\"#000\" fill=\"none\"/>\n"
            "<line id=\"svg_24\" y2=\"222.72375\" x2=\"231.25642\" y1=\"214.41612\" x1=\"222.6411\" stroke=\"#000\" fill=\"none\"/>\n"
            "<line id=\"svg_25\" y2=\"206.41618\" x2=\"555.1925\" y1=\"215.0315\" x1=\"563.80782\" stroke=\"#000\" fill=\"none\"/>\n"
            "<line id=\"svg_26\" y2=\"223.03144\" x2=\"556.11557\" y1=\"215.33919\" x1=\"563.80782\" stroke=\"#000\" fill=\"none\"/>\n"
            "<line id=\"svg_27\" y2=\"101.46657\" x2=\"279.19979\" y1=\"92.21658\" x1=\"278.44979\" stroke=\"#000\" fill=\"none\"/>\n"
            "<line id=\"svg_28\" y2=\"93.96658\" x2=\"286.94978\" y1=\"91.96658\" x1=\"278.44979\" stroke=\"#000\" fill=\"none\"/>\n"
            "<line id=\"svg_29\" y2=\"94.71658\" x2=\"501.99969\" y1=\"94.46658\" x1=\"510.99968\" stroke=\"#000\" fill=\"none\"/>\n"
            "<line id=\"svg_30\" y2=\"103.46657\" x2=\"510.74968\" y1=\"94.71658\" x1=\"509.99968\" stroke=\"#000\" fill=\"none\"/>\n"
            "<line stroke-linecap=\"undefined\" stroke-linejoin=\"undefined\" id=\"svg_31\" y2=\"328.5165\" x2=\"509.79967\" y1=\"336.0165\" x1=\"511.29967\" stroke=\"#000\" fill=\"none\"/>\n"
            "<line stroke-linecap=\"undefined\" stroke-linejoin=\"undefined\" id=\"svg_32\" y2=\"336.0165\" x2=\"501.79967\" y1=\"336.0165\" x1=\"510.54967\" stroke=\"#000\" fill=\"none\"/>\n"
            "<line stroke-linecap=\"undefined\" stroke-linejoin=\"undefined\" id=\"svg_33\" y2=\"328.15953\" x2=\"277.32419\" y1=\"336.56265\" x1=\"277.15613\" stroke=\"#000\" fill=\"none\"/>\n"
            "<line stroke-linecap=\"undefined\" stroke-linejoin=\"undefined\" id=\"svg_34\" y2=\"336.05847\" x2=\"284.04669\" y1=\"336.56265\" x1=\"276.98806\" stroke=\"#000\" fill=\"none\"/>\n"
            "</g>\n"
            "</svg>\n"
            "</div>\n"

            "</body>\n"
            "</html>\n",
            estado_botao_string,temperature,joyx,joyy, rosa_dos_ventos);


    // Verificando a página enviada (depuração):
    // printf("HTML gerado:\n%s\n", html);

    // Enviando o HTML 
    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    free(request);
    pbuf_free(p);
     return ERR_OK;
}

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}


// Configurações iniciais do microcontrolador (inicialização do botão, ADC e LED utilizado para debug)
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

        // Inicializa o ADC
        adc_init();
        adc_set_temp_sensor_enabled(true);

}

// Função principal do código
int main()
{
    // Configurações iniciais
    setup();

    //Inicializa o chip wi-fi do rapberry pi pico W
     while (cyw43_arch_init())
    {
        printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    // Colocando o pi pico W em modo station (vai se conectar ao roteador wi-fi)
    cyw43_arch_enable_sta_mode();

    // Tenta se conectar À rede wi-fi
    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000))
    {
        printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }
    printf("Conectado ao Wi-Fi\n");

    // Mostrando o IP de acesso na rede
    if (netif_default)
    {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
        
        // Acende o led do módulo wi-fi se a conexão funcionar
        cyw43_arch_gpio_put(LED_PIN, 1);
    }

    // Configuração  do servidor TCP
    struct tcp_pcb *server = tcp_new();
    if (!server)
    {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    server = tcp_listen(server);
    tcp_accept(server, tcp_server_accept);

    printf("Servidor ouvindo na porta 80\n");

    // Laço infinito para manter e tratar a conexão
    while (true)
    {
        cyw43_arch_poll();
    }

    cyw43_arch_deinit();
    return 0;

}
