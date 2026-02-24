#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// Configuração mínima para lwIP
#define NO_SYS 1
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0
#define LWIP_TCP 1
#define LWIP_UDP 1
#define MEM_ALIGNMENT 4
#define MEM_SIZE 16384
#define MEMP_NUM_PBUF 16
#define PBUF_POOL_SIZE 32               // Ajuste conforme necessário
#define MEMP_NUM_UDP_PCB 4
#define MEMP_NUM_TCP_PCB 4
#define MEMP_NUM_TCP_SEG 16
#define LWIP_IPV4 1
#define LWIP_ICMP 1
#define LWIP_RAW 1
#define LWIP_DHCP 1
#define LWIP_AUTOIP 1
#define LWIP_DNS 1
#define LWIP_HTTPD 1
#define LWIP_HTTPD_SSI              1  // Habilita SSI
#define LWIP_HTTPD_SUPPORT_POST     1  // Habilita suporte a POST, se necessário
#define LWIP_HTTPD_DYNAMIC_HEADERS 1
#define HTTPD_USE_CUSTOM_FSDATA 0
#define LWIP_HTTPD_CGI 0           // Desative CGI para economizar memória
#define LWIP_NETIF_HOSTNAME 1


// Aumenta o tamanho do buffer de envio TCP (default: 2048 ou menos)
#define TCP_SND_BUF 16384

// Aumenta a janela TCP (importante para controle de fluxo)
#define TCP_WND 16384

// Número de segmentos (pacotes) que podem estar enfileirados para envio
#define TCP_SND_QUEUELEN 63

// Tamanho máximo de segmento TCP (evita fragmentação)
#define TCP_MSS 1280

#define LWIP_DISABLE_TCP_SANITY_CHECKS 1

#endif /* LWIPOPTS_H */
