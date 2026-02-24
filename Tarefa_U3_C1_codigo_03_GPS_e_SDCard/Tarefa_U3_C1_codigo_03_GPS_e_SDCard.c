#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"


// Configurando a comunicação com o GPS
#define UART_ID uart1 // Or uart1
#define BAUD_RATE 9600 // Default baud rate for NEO-6M
#define UART_TX_PIN 8 // Or other appropriate GPIO
#define UART_RX_PIN 9 // Or other appropriate GPIO




// Processando string do GPS
void dados_GPS(char* sentenca)
{

    // Verificando se a palavra que chegou é a no formato desejado (GPRMC)

    if(strncmp(sentenca,"$GPRMC",6) !=0)
    {
        return;
    }

    // Copiando a string de interesse para o buffer
    char buffer[128]; // Essa variável é necessária?
    strcpy(buffer,sentenca);

    // Separando as partes da string (AVALIAR SE ESTÁ CORRETO!!!!!)
    char *identificador = strtok(buffer,",");
    char *hora_utc = strtok(NULL, ",");
    char *estado = strtok(NULL, ",");
    char *latitude = strtok(NULL, ",");
    char *hemisferio_latitude = strtok(NULL, ",");
    char *longitude = strtok(NULL, ",");
    char *hemisferio_longitude = strtok(NULL, ",");
    char *velocidade_knots = strtok(NULL, ",");
    char *curso = strtok(NULL, ",");
    char *data = strtok(NULL, ",");
    char *variacao_magnetica = strtok(NULL, ",");
    char *direcao_variacao = strtok(NULL, ",");
    char *checksum = strtok(NULL, ",");

    if(strcmp(estado,"A")==0)
    {
        printf("Latitude: %s %s\nLongitude: %s %s\n\n",latitude,hemisferio_latitude,longitude,hemisferio_longitude);

    }
    else
    {
        printf("Sistema GPS fora do ar\n\n");
    }

}

void setup()
{

    // Iniciando entrada/saída 
    stdio_init_all();

    /*gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    */

    // Iniciando a comunicação com a UART (GPS)
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);



}

int main()
{

    setup();
    char linha[128];
    int elemento_linha = 0;


    while (true) 
    {

        if (uart_is_readable(UART_ID)) 
        {
            char c = uart_getc(UART_ID);
            if(c=='\n')
            {
                linha[elemento_linha]='\0';
                dados_GPS(linha);
                elemento_linha = 0;
            }
            else{
                if(elemento_linha<sizeof(linha)-1)
                {
                    linha[elemento_linha++]=c;
                }
            }
            //putchar(c);  
        }



    }
}
    

