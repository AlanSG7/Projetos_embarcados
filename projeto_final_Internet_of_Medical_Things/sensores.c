/*
Embarcatech - Trilha de Software Embarcado - Projeto Final
sensores (definição das funções)
Aluno: Alan Sovano Gomes
Matrícula: 202420110940138
Mentora: Gabriela Teixeira
*/

/* Biblioteca para a utilizacao de sensores 
no projeto final.

Inclui funcoes para:

- MPU6050
- MAX30100
- GPS GY-NEO6MV2

*/

#include "sensores.h"

/******************************************************* Utilização da MPU ********************************************************/

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


void capta_medicao_mpu6050(float *ac_x, float *ac_y, float *ac_z, float *gyro_x, float *gyro_y, float *gyro_z)
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


}


/************************************************* Utilização do GPS ************************************************************************/
void inicializar_GPS()
{
    // Iniciando a comunicação com a UART (GPS)
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
}

// Processando string do GPS
bool dados_GPS(char* sentenca, char* resultado_lat, char* resultado_lon, char* hem_lat, char* hem_lon)
{

    // Verificando se a palavra que chegou é a no formato desejado (GPRMC)

    if(strncmp(sentenca,"$GPRMC",6) !=0)
    {
        return false;
    }

    // Copiando a string de interesse para o buffer
    char buffer[128]; 
    strcpy(buffer,sentenca);

    // Separando as partes da string 
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

    strcpy(resultado_lat,latitude);
    strcpy(resultado_lon,longitude);
    strcpy(hem_lat,hemisferio_latitude);
    strcpy(hem_lon,hemisferio_longitude);

    if(strcmp(estado,"A")==0)
    {
        return true;
    }
    else
    {
        return false;
    }

}


/******************************************************* Utilização do sensor do fotopletismógrafo (PPG) ******************************************/

//----------------- Funções auxiliares I2C -----------------

/*
 * Escreve 1 byte (val) no registrador (reg) do MAX30102.
 * Retorna true se a escrita foi bem-sucedida.
 */
static bool wr8(uint8_t reg, uint8_t val){
    uint8_t b[2] = {reg, val};
    return i2c_write_blocking(I2C_PORT, MAX_ADDR, b, 2, false) == 2;
}

/*
 * Lê 1 byte do registrador (reg) do MAX30102 para *val.
 * Retorna true se leu com sucesso.
 */
static bool rd8(uint8_t reg, uint8_t *val){
    if (i2c_write_blocking(I2C_PORT, MAX_ADDR, &reg, 1, true) != 1) return false;
    return i2c_read_blocking(I2C_PORT, MAX_ADDR, val, 1, false) == 1;
}

/*
 * Lê N bytes começando em (reg) para o buffer buf.
 * Retorna true se a leitura foi bem-sucedida.
 */
static bool rdbuf(uint8_t reg, uint8_t *buf, size_t n){
    if (i2c_write_blocking(I2C_PORT, MAX_ADDR, &reg, 1, true) != 1) return false;
    return i2c_read_blocking(I2C_PORT, MAX_ADDR, buf, n, false) == (int)n;
}

// ----------------- Inicialização / reset do MAX30102 -----------------

/*
 * Reseta o MAX30102:
 *  - Escreve 0x40 em MODE_CFG para reset.
 *  - Espera 10 ms.
 *  - Escreve 0x00 para acordar.
 */
static void max_reset(void){
    wr8(REG_MODE_CFG, 0x40); sleep_ms(10);
    wr8(REG_MODE_CFG, 0x00); sleep_ms(10);
}

/*
 * Configuração inicial:
 *  - desabilita interrupções,
 *  - zera ponteiros da FIFO (evita lixo),
 *  - define média de 4 amostras na FIFO + rollover,
 *  - configura SPO2 (range + 100Hz + 18 bits),
 *  - acerta correntes dos LEDs,
 *  - coloca em modo SpO2 (RED+IR).
 */
void max_init(uint8_t led_pa){
    max_reset();

    // Interrupções desabilitadas (usamos polling simples)
    wr8(REG_INT_EN1, 0x00);
    wr8(REG_INT_EN2, 0x00);

    // Limpa FIFO (ponteiros e contador de overflow)
    wr8(REG_FIFO_WR_PTR, 0x00);
    wr8(REG_OVF_CNT,     0x00);
    wr8(REG_FIFO_RD_PTR, 0x00);

    // FIFO_CFG:
    //  bits 7:5 = 0b000 => sem média
    //  bit 4   = 1      => rollover habilitado (circular)
    wr8(REG_FIFO_CFG, (0<<5) | (1<<4));

    // SPO2_CFG: range ADC + SR=100 Hz + PW=18 bits
    wr8(REG_SPO2_CFG, SPO2_CFG_RANGE_SR_PW);

    // Correntes de LED (mesmo valor para RED e IR; ajuste se quiser)
    //wr8(REG_LED1_PA, led_pa);   // RED
    //wr8(REG_LED2_PA, led_pa);   // IR

    wr8(REG_LED1_PA, 0x45); // RED  ≈ 9,6 mA
    wr8(REG_LED2_PA, 0x45); // IR   ≈ 13,8 mA

    // MODE_CFG: 0x03 => modo SpO2 (LEDs RED+IR alternados)
    wr8(REG_MODE_CFG, 0x03);

    // Aguardar estabilização
    sleep_ms(100);
}

// ----------------- FIFO: disponibilidade e leitura de uma amostra -----------------

/*
 * Quantas amostras estão disponíveis na FIFO
 *  - Lê os ponteiros WR_PTR e RD_PTR e calcula diferença com wrap (5 bits).
 *  - A FIFO tem profundidade de 32 amostras (0..31).
 */
uint8_t fifo_available(void){
    uint8_t wr, rd;
    rd8(REG_FIFO_WR_PTR, &wr);
    rd8(REG_FIFO_RD_PTR, &rd);
    return (uint8_t)((wr - rd) & 0x1F);
}

/*
 * Lê uma amostra (um par RED/IR) da FIFO:
 *  - O MAX30102 entrega 6 bytes por amostra (3 por canal, 18 bits).
 *  - Montagem: (b0<<16 | b1<<8 | b2) & 0x3FFFF.
 */
bool read_sample(uint32_t *red, uint32_t *ir){
    uint8_t b[6];
    if (!rdbuf(REG_FIFO_DATA, b, 6)) return false;

    *red = (((uint32_t)b[0]<<16) | ((uint32_t)b[1]<<8) | b[2]) & 0x3FFFF;
    *ir  = (((uint32_t)b[3]<<16) | ((uint32_t)b[4]<<8) | b[5]) & 0x3FFFF;
    return true;
}


// ----------------- Estado e processamento (BPM / SpO2) -----------------

/*
 * Estrutura de estado:
 *  - dc_* e var_*: acumuladores para EMA (Exponential Moving Average - média móvel exponencial) da base (DC) e variância (≈RMS^2) do AC.
 *  - ma_*: média móvel de 5 pontos em AC do IR (suavização para detectar vales).
 *  - last_peak / rr_hist: memória para estimar BPM com média dos últimos RR.
 *  - finger_on: flag indicando dedo/pulso presente (DC/SNR dentro da faixa).
 *  - last_*: métricas de debug para imprimir (DC_IR, RMS_IR, SNR).
 *  - acs_prev*: histórico do AC suavizado para testar “mínimo local”.
 */


/* Inicializa a estrutura de estado com valores neutros. */
void ps_init(PulseState *s){
    s->dc_red = s->dc_ir = 0.0f;
    s->var_red = s->var_ir = 1.0f;  // evita zero ao tirar sqrt
    s->ma_sum = 0.0f; for(int i=0;i<2;i++) s->ma_buf[i]=0.0f; s->ma_idx=0;
    s->last_peak = -100000;  // bem no passado
    s->rr_len = 0;
    s->bpm = 0.0f;
    s->finger_on = false;
    s->last_rms_ir = 0.0f; s->last_snr = 0.0f; s->last_dc_ir = 0.0f;
    s->acs_prev1 = s->acs_prev2 = 0.0f;
}

/*
 * process(...)
 *  - Entrada: amostras brutas RED/IR e o índice da amostra (idx).
 *  - Saída: valor de SpO2 estimado (didático) ou NAN se sem dedo.
 *
 * Passos:
 *  1) EMA rápida para DC e variância do AC (resposta ~0,3–0,5 s).
 *  2) RMS da AC e SNR = RMS/DC.
 *  3) Testa dedo/pulso (DC em faixa + SNR mínimo).
 *  4) Suaviza AC do IR (média móvel de 5 pts).
 *  5) Detecta batimento por *vale* (mínimo local) + amplitude mínima + refratário.
 *  6) Calcula RR, rejeita outliers relativos à média, estima BPM (média de 5 RR).
 *  7) Estima SpO2 por ratio-of-ratios (didático).
 */
float process(PulseState *s, uint32_t red_raw, uint32_t ir_raw, int idx){
    // EMA mais rápida: a maior => resposta mais curta (menos atraso)
    const float a_dc  = 0.03f;  // ~0,33 s com FS=100
    const float a_var = 0.03f;  // janela semelhante para variância

    // Converte para float
    float red = (float)red_raw;
    float ir  = (float)ir_raw;

    // 1) Estima DC (EMA) e extrai AC = x - DC
    s->dc_red += a_dc * (red - s->dc_red);
    s->dc_ir  += a_dc * (ir  - s->dc_ir);
    float ac_red = red - s->dc_red;
    float ac_ir  = ir  - s->dc_ir;

    // 2) Estima variância do AC (EMA) e RMS = sqrt(variância)
    s->var_red += a_var * ((ac_red*ac_red) - s->var_red);
    s->var_ir  += a_var * ((ac_ir*ac_ir)   - s->var_ir);

    float dc_ir   = fmaxf(s->dc_ir,  1.0f); // evita dividir por 0
    float dc_redv = fmaxf(s->dc_red, 1.0f);
    float rms_ir  = sqrtf(fmaxf(s->var_ir,  1.0f));
    float rms_red = sqrtf(fmaxf(s->var_red, 1.0f));
    float snr     = rms_ir / dc_ir;        // fração (ex.: 0,01 = 1%)

    // Guarda métricas para impressão
    s->last_dc_ir  = dc_ir;
    s->last_rms_ir = rms_ir;
    s->last_snr    = snr;

    // 3) Dedo/pulso presente? (DC dentro da faixa + SNR mínimo)
    bool dc_ok  = (dc_ir  > FINGER_DC_MIN && dc_ir  < FINGER_DC_MAX &&
                   dc_redv > FINGER_DC_MIN && dc_redv < FINGER_DC_MAX);
    bool snr_ok = (snr >= FINGER_SNR_MIN);
    s->finger_on = dc_ok && snr_ok;

    // 4) Suavização do AC do IR via média móvel (5 amostras)
    s->ma_sum -= s->ma_buf[s->ma_idx];
    s->ma_buf[s->ma_idx] = ac_ir;
    s->ma_sum += s->ma_buf[s->ma_idx];
    s->ma_idx = (s->ma_idx + 1) % 2;
    float ac_s = s->ma_sum / 2.0f;  // AC do IR suavizado

    // 5) Detecção de batimento por *mínimos* (vales), com refratário/outliers
    if (s->finger_on) {
        // Amplitude mínima do vale (fração do RMS da AC)
        float thr_vale = 0.45f * rms_ir;
        // Refratário em amostras
        int refractory = (int)(REFRACTORY_S * FS_HZ);

        // “Mínimo local” no ponto anterior (acs_prev1)?
        bool is_min_local = (s->acs_prev1 < s->acs_prev2) && (s->acs_prev1 <= ac_s);
        bool amp_ok       = (-s->acs_prev1) > thr_vale;   // vale “fundo” suficiente

        // Se for um vale válido e respeita refratário, computa RR
        if (is_min_local && amp_ok && (idx - s->last_peak) > refractory) {
            int   diff = idx - s->last_peak;
            float rr   = diff / FS_HZ;      // intervalo em segundos
            s->last_peak = idx;

            // RR dentro de limites físicos plausíveis?
            if (rr > RR_MIN_S && rr < RR_MAX_S) {
                // Calcula média dos RR anteriores (aprox. de mediana)
                float mean = rr;
                if (s->rr_len) {
                    float soma = 0.0f;
                    for (int i=0; i<s->rr_len; i++) soma += s->rr_hist[i];
                    mean = soma / s->rr_len;
                }
                // Rejeita outliers relativos à média (evita “dobrar” batida)
                if (rr > RR_LOWER_FRAC*mean && rr < RR_UPPER_FRAC*mean) {
                    // Enfileira RR (buffer de até 5 amostras)
                    if (s->rr_len < 2) s->rr_hist[s->rr_len++] = rr;
                    else {
                        for (int i=1; i<2; i++) s->rr_hist[i-1] = s->rr_hist[i];
                        s->rr_hist[1] = rr;
                    }
                    // BPM pela média dos RR
                    float m = 0.0f; for (int i=0; i<s->rr_len; i++) m += s->rr_hist[i];
                    m /= s->rr_len;
                    s->bpm = 60.0f / m;
                }
            }
        }
    } else {
        // Sem dedo/pulso: zera estimativa de ritmo
        s->bpm = 0.0f; s->rr_len = 0;
    }

    // 6) Estimativa didática de SpO2 (ratio-of-ratios) somente se finger_on
    float spo2 = NAN;
    if (s->finger_on) {
        // R = (AC_red/DC_red) / (AC_ir/DC_ir)
        float R = (rms_red / dc_redv) / (rms_ir / dc_ir);
        // Conversão linear de exemplo (didática, não clínica)
        spo2 = 104.0f - 17.0f * R +7.0f*R*R;
        if (spo2 < 70.0f) spo2 = 70.0f;
        if (spo2 > 100.0f) spo2 = 100.0f;
    }

    // 7) Atualiza histórico para checar “mínimo local” na próxima iteração
    s->acs_prev2 = s->acs_prev1;
    s->acs_prev1 = ac_s;

    return spo2;
}