#define STM32H7A3xxQ

#include "stm32h7xx.h" // ATENÇÃO: Verifique se este é o cabeçalho exato da sua placa NUCLEO-144 (ex: stm32h7xx.h, stm32f4xx.h)
#include <stdint.h>


// =========================================================================
// DEFINIÇÃO DOS PINOS, PORTAS E MÁSCARAS (HARDWARE MAPPING)
// =========================================================================

// Configurações do LED RGB (Porta D)
#define LED_PORT        GPIOD
#define LED_RED_PIN     12
#define LED_GREEN_PIN   14
#define LED_BLUE_PIN    15

// Configurações do Botão Azul (Porta C)
#define BOT_PORT        GPIOC
#define BOT_PIN         13

// Máscaras úteis para manipular os registradores de forma mais legível
#define LED_RED_MASK    (1U << LED_RED_PIN)
#define LED_GREEN_MASK  (1U << LED_GREEN_PIN)
#define LED_BLUE_MASK   (1U << LED_BLUE_PIN)
#define LED_ALL_MASK    (LED_RED_MASK | LED_GREEN_MASK | LED_BLUE_MASK)

// =========================================================================
// ESTRUTURAS E VARIÁVEIS GLOBAIS
// =========================================================================

// Definição da Estrutura Otimizada (52 bytes, sem padding)
typedef struct {
  char identificador[50];
  uint8_t estado_botao;
  uint8_t estado_leds;
  uint32_t contador;
  enum COR {PRETO, VERMELHO, VERDE, AZUL, AMARELO, CIANO, MAGENTA, BRANCO} cor_led;
} Perifericos_t;

// Instanciação da Variável Global
volatile Perifericos_t LEDInterativo = {
    .identificador = "Usuario_Embarcados",
    .estado_botao = 0,
    .estado_leds = 0b00000001, // Começa configurado para o estado "Apagado"
    .contador = 0,
    .cor_led = PRETO
};

// Protótipos das Funções
void Hardware_Init(void);

// =========================================================================
// FUNÇÃO PRINCIPAL
// =========================================================================
int main(void) {

    // Inicializa o hardware (Clocks, GPIOs, Interrupções)
    Hardware_Init();

    while (1) {
        // Verifica se o botão foi pressionado (flag alterada pela interrupção)
        if (LEDInterativo.estado_botao == 1) {

            // 1. APAGAR TODOS OS CANAIS DO LED antes de definir a nova cor
            // Escrevemos nos bits de RESET do registrador BSRR (que ficam nos bits 16 ao 31)
            LED_PORT->BSRR = (LED_ALL_MASK << 16);

            // 2. ACENDER OS LEDs E ATUALIZAR O ESTADO DA COR
            switch(LEDInterativo.estado_leds) {
                case 0b00000001: // Apagado
                    LEDInterativo.cor_led = PRETO;
                    break;
                case 0b00000010: // Vermelho
                    LED_PORT->BSRR = LED_RED_MASK;
                    LEDInterativo.cor_led = VERMELHO;
                    break;
                case 0b00000100: // Verde
                    LED_PORT->BSRR = LED_GREEN_MASK;
                    LEDInterativo.cor_led = VERDE;
                    break;
                case 0b00001000: // Azul
                    LED_PORT->BSRR = LED_BLUE_MASK;
                    LEDInterativo.cor_led = AZUL;
                    break;
                case 0b00010000: // Amarelo (Vermelho + Verde)
                    LED_PORT->BSRR = (LED_RED_MASK | LED_GREEN_MASK);
                    LEDInterativo.cor_led = AMARELO;
                    break;
                case 0b00100000: // Ciano (Verde + Azul)
                    LED_PORT->BSRR = (LED_GREEN_MASK | LED_BLUE_MASK);
                    LEDInterativo.cor_led = CIANO;
                    break;
                case 0b01000000: // Magenta (Azul + Vermelho)
                    LED_PORT->BSRR = (LED_RED_MASK | LED_BLUE_MASK);
                    LEDInterativo.cor_led = MAGENTA;
                    break;
                case 0b10000000: // Branco (Todos os canais)
                    LED_PORT->BSRR = LED_ALL_MASK;
                    LEDInterativo.cor_led = BRANCO;
                    break;
                default:
                    LEDInterativo.cor_led = PRETO;
                    break;
            }

            // 3. RESETAR FLAG do botão após processar a ação
            LEDInterativo.estado_botao = 0;
        }
    }
}

// =========================================================================
// CONFIGURAÇÃO DOS PERIFÉRICOS (CMSIS)
// =========================================================================
void Hardware_Init(void) {
    // --- 1. CONFIGURAÇÃO DOS CLOCKS ---
    // Habilitar clock para as portas C (Botão) e D (LEDs)
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOCEN | RCC_AHB4ENR_GPIODEN;
    // Habilitar clock para o SYSCFG (Essencial para configurar o EXTI)
    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;

    // --- 2. CONFIGURAÇÃO DO LED RGB (SAÍDAS) ---
    // Limpar os bits de MODER (configurando como entrada temporariamente)
    LED_PORT->MODER &= ~((3U << (LED_RED_PIN * 2)) | (3U << (LED_GREEN_PIN * 2)) | (3U << (LED_BLUE_PIN * 2)));
    // Configurar como saída (01)
    LED_PORT->MODER |=  ((1U << (LED_RED_PIN * 2)) | (1U << (LED_GREEN_PIN * 2)) | (1U << (LED_BLUE_PIN * 2)));

    // --- 3. CONFIGURAÇÃO DO BOTÃO AZUL (ENTRADA) ---
    // Limpar bits de MODER para garantir modo de entrada (00)
    BOT_PORT->MODER &= ~(3U << (BOT_PIN * 2));
    // Como existe um circuito de pull-down externo no PC13, desabilitamos resistores internos (00)
    BOT_PORT->PUPDR &= ~(3U << (BOT_PIN * 2));

    // --- 4. CONFIGURAÇÃO DA INTERRUPÇÃO EXTERNA (EXTI) ---
    // Mapear o PC13 para a linha EXTI13 no SYSCFG
    SYSCFG->EXTICR[3] &= ~(0x000F << 4);     // Limpar bits correspondentes ao EXTI13
    SYSCFG->EXTICR[3] |=  (0x0002 << 4);     // Setar mapeamento para PORT C (0010 binário = 2)

    // Configurar gatilho para borda de subida (Rising Trigger) no EXTI13
    EXTI->RTSR1 |= (1U << BOT_PIN);
    EXTI->FTSR1 &= ~(1U << BOT_PIN);         // Desativar borda de descida para evitar disparos na soltura do botão

    // Desmascarar (habilitar) a interrupção 13
    EXTI->IMR1 |= (1U << BOT_PIN);

    // --- 5. CONFIGURAÇÃO DO NVIC (CONTROLADOR DE INTERRUPÇÕES) ---
    // A linha EXTI13 é atendida pela interrupção "EXTI15_10_IRQn" (pinos 10 a 15)
    NVIC_SetPriority(EXTI15_10_IRQn, 6);     // Define a prioridade requisitada (6)
    NVIC_EnableIRQ(EXTI15_10_IRQn);          // Habilita a interrupção no controlador
}

// =========================================================================
// ROTINA DE SERVIÇO DE INTERRUPÇÃO (ISR)
// =========================================================================
void EXTI15_10_IRQHandler(void) {
    // Verificar se a interrupção foi realmente gerada pelo pino do nosso botão (linha 13)
    if ((EXTI->PR1 & (1U << BOT_PIN)) != 0) {

        // Limpar a flag de interrupção escrevendo 1 nela
        EXTI->PR1 = (1U << BOT_PIN);

        // 1. Incrementar o contador de acionamentos
        LEDInterativo.contador++;

        // 2. Rotação Cíclica da variável estado_leds
        // O deslocamento << 1 move todos os bits para a esquerda
        // O >> 7 extrai o bit mais significativo antigo para jogar de volta na posição 0
        uint8_t tmp = (LEDInterativo.estado_leds << 1) | ((LEDInterativo.estado_leds >> 7) & 1);
        LEDInterativo.estado_leds = tmp;

        // 3. Sinaliza ao loop principal (main) que há uma alteração pendente
        LEDInterativo.estado_botao = 1;
    }
}
