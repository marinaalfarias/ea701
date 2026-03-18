

#include <stdint.h>
#include <string.h>

/* ================== REGISTRADORES (STM32H7 - NUCLEO-144) ================== */

// Clock
#define RCC_AHB4ENR   (*(volatile uint32_t*)0x58024540)

// GPIO D (LED RGB)
#define GPIOD_MODER   (*(volatile uint32_t*)0x58020C00)
#define GPIOD_ODR     (*(volatile uint32_t*)0x58020C14)

// GPIO C (Botão)
#define GPIOC_MODER   (*(volatile uint32_t*)0x58020800)

// SYSCFG e EXTI
#define RCC_APB4ENR   (*(volatile uint32_t*)0x580244E0)
#define SYSCFG_EXTICR4 (*(volatile uint32_t*)0x58000414)

#define EXTI_RTSR1    (*(volatile uint32_t*)0x58000008)
#define EXTI_IMR1     (*(volatile uint32_t*)0x58000000)
#define EXTI_PR1      (*(volatile uint32_t*)0x58000014)

// NVIC
#define NVIC_ISER1    (*(volatile uint32_t*)0xE000E104)

/* ================== DEFINIÇÕES ================== */

#define LED_VERMELHO (1 << 12)
#define LED_VERDE    (1 << 14)
#define LED_AZUL     (1 << 15)

/* ================== ESTRUTURA OTIMIZADA ================== */

typedef struct {
    uint32_t contador;       // 4 bytes
    char identificador[20];  // reduzido para economizar memória
    uint8_t estado_botao;    
    uint8_t estado_leds;
    uint8_t cor_led;
} Perifericos_t;

/* ================== VARIÁVEL GLOBAL ================== */

Perifericos_t LEDInterativo = {
    .identificador = "Aluno",
    .estado_botao = 0,
    .contador = 0,
    .cor_led = 0,
    .estado_leds = 0x01
};

// C* ================== FUNÇÃO PARA ATUALIZAR LED ================== */

void atualiza_led(uint8_t estado) {
    // Apaga todos os LEDs primeiro
    GPIOD_ODR &= ~(LED_VERMELHO | LED_VERDE | LED_AZUL);

    switch (estado) {
        case 0x01: // apagado
            LEDInterativo.cor_led = 0;
            break;

        case 0x02: // vermelho
            GPIOD_ODR |= LED_VERMELHO;
            LEDInterativo.cor_led = 1;
            break;

        case 0x04: // verde
            GPIOD_ODR |= LED_VERDE;
            LEDInterativo.cor_led = 2;
            break;

        case 0x08: // azul
            GPIOD_ODR |= LED_AZUL;
            LEDInterativo.cor_led = 3;
            break;

        case 0x10: // amarelo
            GPIOD_ODR |= (LED_VERMELHO | LED_VERDE);
            LEDInterativo.cor_led = 4;
            break;

        case 0x20: // ciano
            GPIOD_ODR |= (LED_VERDE | LED_AZUL);
            LEDInterativo.cor_led = 5;
            break;

        case 0x40: // magenta
            GPIOD_ODR |= (LED_VERMELHO | LED_AZUL);
            LEDInterativo.cor_led = 6;
            break;

        case 0x80: // branco
            GPIOD_ODR |= (LED_VERMELHO | LED_VERDE | LED_AZUL);
            LEDInterativo.cor_led = 7;
            break;
    }
}

/* ================== ISR DO BOTÃO ================== */

void EXTI15_10_IRQHandler(void) {
    if (EXTI_PR1 & (1 << 13)) {

        // Limpa flag de interrupção
        EXTI_PR1 |= (1 << 13);

        // Incrementa contador
        LEDInterativo.contador++;

        // Rotação cíclica dos bits
        uint8_t tmp = (LEDInterativo.estado_leds << 1) |
                      ((LEDInterativo.estado_leds >> 7) & 1);

        LEDInterativo.estado_leds = tmp;

        // Sinaliza evento
        LEDInterativo.estado_botao = 1;
    }
}


int main(void) {

    /* --- Habilita clocks --- */
    RCC_AHB4ENR |= (1 << 3); // GPIOD
    RCC_AHB4ENR |= (1 << 2); // GPIOC
    RCC_APB4ENR |= (1 << 1); // SYSCFG

    /* --- Configura LEDs (PD12,14,15 como saída) --- */
    GPIOD_MODER &= ~( (3 << (12*2)) | (3 << (14*2)) | (3 << (15*2)) );
    GPIOD_MODER |=  ( (1 << (12*2)) | (1 << (14*2)) | (1 << (15*2)) );

    /* --- Configura PC13 como entrada --- */
    GPIOC_MODER &= ~(3 << (13*2));

    /* --- Configura EXTI13 (botão) --- */
    SYSCFG_EXTICR4 &= ~(0xF << 4); // limpa bits
    SYSCFG_EXTICR4 |=  (0x2 << 4); // PC13

    EXTI_IMR1  |= (1 << 13); // habilita interrupção
    EXTI_RTSR1 |= (1 << 13); // borda de subida

    /* --- NVIC (IRQ EXTI15_10 = posição 40) --- */
    NVIC_ISER1 |= (1 << (40 - 32)); // habilita IRQ

    /* --- Estado inicial --- */
    atualiza_led(LEDInterativo.estado_leds);

    /* --- Loop principal --- */
    while (1) {

        if (LEDInterativo.estado_botao) {

            atualiza_led(LEDInterativo.estado_leds);

            // Reseta flag
            LEDInterativo.estado_botao = 0;
        }
    }
}