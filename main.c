#define STM32H7A3xxQ
#include <stdint.h>
#include "stm32h7xx.h"

// GPIOD (LED RGB)
#define LED_PORT        GPIOD
#define LED_RED_PIN     12
#define LED_GREEN_PIN   14
#define LED_BLUE_PIN    15

// GPIOC (Botão)
#define BOT_PORT        GPIOC
#define BOT_PIN         13

// Máscaras para cada pino
#define LED_RED_MASK    (1U << LED_RED_PIN)
#define LED_GREEN_MASK  (1U << LED_GREEN_PIN)
#define LED_BLUE_MASK   (1U << LED_BLUE_PIN)
#define LED_ALL_MASK    (LED_RED_MASK | LED_GREEN_MASK | LED_BLUE_MASK)

// ESTRUTURAS E VARIÁVEIS GLOBAIS

// Agrupamento das informações necessárias para alterar os estados do LED
typedef struct {
  char identificador[50];
  uint8_t estado_botao;
  uint8_t estado_leds;
  uint32_t contador;
  enum COR {APAGADO, VERMELHO, VERDE, AZUL, AMARELO, CIANO, MAGENTA, BRANCO} cor_led;
} Perifericos_t;

// Dados iniciais que serão alterados a cada clique do botão
volatile Perifericos_t LEDInterativo = {
    .identificador = "usuario",
    .estado_botao = 0,
    .estado_leds = 0b00000001, // Estado inicial - LED apagado
    .contador = 0,
    .cor_led = APAGADO
};

// Protótipos das Funções
void Hardware_Init(void);

///////////////////////////

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
                case 0x01: // Apagado
                    LEDInterativo.cor_led = APAGADO;
                    break;
                case 0x02: // Vermelho
                    LED_PORT->BSRR = LED_RED_MASK;
                    LEDInterativo.cor_led = VERMELHO;
                    break;
                case 0x04: // Verde
                    LED_PORT->BSRR = LED_GREEN_MASK;
                    LEDInterativo.cor_led = VERDE;
                    break;
                case 0x08: // Azul
                    LED_PORT->BSRR = LED_BLUE_MASK;
                    LEDInterativo.cor_led = AZUL;
                    break;
                case 0x10: // Amarelo (Vermelho + Verde)
                    LED_PORT->BSRR = (LED_RED_MASK | LED_GREEN_MASK);
                    LEDInterativo.cor_led = AMARELO;
                    break;
                case 0x20: // Ciano (Verde + Azul)
                    LED_PORT->BSRR = (LED_GREEN_MASK | LED_BLUE_MASK);
                    LEDInterativo.cor_led = CIANO;
                    break;
                case 0x40: // Magenta (Azul + Vermelho)
                    LED_PORT->BSRR = (LED_RED_MASK | LED_BLUE_MASK);
                    LEDInterativo.cor_led = MAGENTA;
                    break;
                case 0x80: // Branco (Todos os canais)
                    LED_PORT->BSRR = LED_ALL_MASK;
                    LEDInterativo.cor_led = BRANCO;
                    break;
                default:
                    LEDInterativo.cor_led = APAGADO;
                    break;
            }

            // Reset da FLAG do botão após processar a ação
            LEDInterativo.estado_botao = 0;
        }
    }
}

void Hardware_Init(void) {
    // Habilita o clock para o botão (GPIOC), LED (GPIOD), SYSCFG e EXTI
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOCEN | RCC_AHB4ENR_GPIODEN;
    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;

    // Configuração dos pinos do LED como saída (PD12,14,15)
    LED_PORT->MODER &= ~((3U << (LED_RED_PIN * 2)) | (3U << (LED_GREEN_PIN * 2)) | (3U << (LED_BLUE_PIN * 2)));
    LED_PORT->MODER |=  ((1U << (LED_RED_PIN * 2)) | (1U << (LED_GREEN_PIN * 2)) | (1U << (LED_BLUE_PIN * 2)));

    // Configuração do botão como entrada (PC13)
    BOT_PORT->MODER &= ~(3U << (BOT_PIN * 2));
    BOT_PORT->PUPDR &= ~(3U << (BOT_PIN * 2));

    // Configuração do EXTI13 (botão) - Interrupção
    SYSCFG->EXTICR[3] &= ~(0x000F << 4);     // Limpar bits correspondentes ao EXTI13
    SYSCFG->EXTICR[3] |=  (0x0002 << 4);     // PC13

    // Gatilho para borda de subida no EXTI13
    EXTI->RTSR1 |= (1U << BOT_PIN);
    EXTI->FTSR1 &= ~(1U << BOT_PIN);

    // Habilita a interrupção
    EXTI->IMR1 |= (1U << BOT_PIN);

    // Configuração do NVIC (Controlador de interrupções)
    // A linha EXTI13 é atendida através do pinos 10 a 15
    NVIC_SetPriority(EXTI15_10_IRQn, 6);     // Define a prioridade 
    NVIC_EnableIRQ(EXTI15_10_IRQn);          // Habilita a interrupção no controlador
}

// Rotina de serviço de interrupção
void EXTI15_10_IRQHandler(void) {
    // Verificar se a interrupção é gerada pelo pino do botão
    if ((EXTI->PR1 & (1U << BOT_PIN)) != 0) {

        // Limpa a flag de interrupção
        EXTI->PR1 = (1U << BOT_PIN); // Escreve "1" no bit 13

        // Incrementa o contador
        LEDInterativo.contador++;

        // Rotação cíclica dos bits (deslocamento à esquerda)
        // São 8 estados
        uint8_t tmp = (LEDInterativo.estado_leds << 1) | ((LEDInterativo.estado_leds >> 7) & 1);
        LEDInterativo.estado_leds = tmp;

        // Indica que o estado foi alterado (botão apertado)
        LEDInterativo.estado_botao = 1;
    }
}
