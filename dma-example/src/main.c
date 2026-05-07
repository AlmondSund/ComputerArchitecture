#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32l476xx.h"

enum {
    DIGIT_COUNT = 4U,
    REFRESH_MS = 2U,
    MESSAGE_STEP_MS = 250U,
    FRAME_BYTES = 2U,
    DMA_TIMEOUT_ITERATIONS = 10000U,
};

typedef struct {
    GPIO_TypeDef *port;
    uint8_t pin_no;
    uint16_t pin_mask;
} pin_t;

typedef struct {
    uint8_t segment_bits;
    uint8_t digit_bits;
} display_frame_t;

static volatile uint32_t g_ms = 0U;
static bool g_dma_error = false;

static const pin_t k_latch_pin = {GPIOB, 5U, 1U << 5}; /* D4 */
static const pin_t k_clock_pin = {GPIOA, 8U, 1U << 8}; /* D7 */
static const pin_t k_data_pin = {GPIOA, 9U, 1U << 9};  /* D8 */

static const uint8_t k_digit_select[DIGIT_COUNT] = {
    0xF1U,
    0xF2U,
    0xF4U,
    0xF8U,
};

static const char k_message[] = " dMA    ";

static display_frame_t g_dma_source = {0xFFU, 0xFFU};
static display_frame_t g_dma_target = {0xFFU, 0xFFU};

void SysTick_Handler(void)
{
    ++g_ms;
}

static uint32_t now_ms(void)
{
    return g_ms;
}

static void pin_write(pin_t pin, bool high)
{
    pin.port->BSRR = high ? (uint32_t)pin.pin_mask
                          : ((uint32_t)pin.pin_mask << 16U);
}

static void gpio_output(pin_t pin)
{
    const uint32_t shift = (uint32_t)pin.pin_no * 2U;
    const uint32_t mask = 0x3UL << shift;

    pin.port->MODER = (pin.port->MODER & ~mask) | (0x1UL << shift);
    pin.port->OTYPER &= ~(0x1UL << pin.pin_no);
    pin.port->OSPEEDR &= ~mask;
    pin.port->PUPDR &= ~mask;
}

static void shift_bit(bool high)
{
    pin_write(k_data_pin, high);
    pin_write(k_clock_pin, true);
    pin_write(k_clock_pin, false);
}

static void shift_byte(uint8_t value)
{
    uint32_t bit_index;

    for (bit_index = 0U; bit_index < 8U; ++bit_index) {
        const uint8_t bit = (uint8_t)(1U << (7U - bit_index));

        shift_bit((value & bit) != 0U);
    }
}

static void write_display_frame(display_frame_t frame)
{
    pin_write(k_latch_pin, false);
    shift_byte(frame.segment_bits);
    shift_byte(frame.digit_bits);
    pin_write(k_latch_pin, true);
}

static uint8_t encode_char(char c)
{
    if ((c >= 'a') && (c <= 'z')) {
        c = (char)(c - ('a' - 'A'));
    }

    switch (c) {
    case '0':
    case 'O':
        return 0x3FU;
    case '1':
    case 'I':
        return 0x06U;
    case '2':
    case 'Z':
        return 0x5BU;
    case '3':
        return 0x4FU;
    case '4':
        return 0x66U;
    case '5':
    case 'S':
        return 0x6DU;
    case '6':
        return 0x7DU;
    case '7':
        return 0x07U;
    case '8':
    case 'B':
        return 0x7FU;
    case '9':
        return 0x6FU;
    case 'A':
        return 0x77U;
    case 'C':
        return 0x39U;
    case 'D':
        return 0x5EU;
    case 'E':
        return 0x79U;
    case 'F':
        return 0x71U;
    case 'H':
        return 0x76U;
    case 'L':
        return 0x38U;
    case 'M':
        return 0x37U;
    case 'N':
        return 0x54U;
    case 'P':
        return 0x73U;
    case 'R':
        return 0x50U;
    case 'T':
        return 0x78U;
    case 'U':
        return 0x3EU;
    case 'Y':
        return 0x6EU;
    case '-':
        return 0x40U;
    case '_':
        return 0x08U;
    case ' ':
    default:
        return 0x00U;
    }
}

static display_frame_t make_frame(uint8_t digit, uint8_t message_offset)
{
    const size_t message_span = sizeof(k_message) - 1U;
    const size_t index = ((size_t)message_offset + digit) % message_span;
    const char c = g_dma_error ? 'E' : k_message[index];

    return (display_frame_t){
        (uint8_t)~encode_char(c),
        k_digit_select[digit],
    };
}

static bool dma_copy_frame(display_frame_t frame, display_frame_t *out)
{
    uint32_t timeout = DMA_TIMEOUT_ITERATIONS;

    g_dma_source = frame;

    DMA1_Channel1->CCR &= ~DMA_CCR_EN;
    DMA1->IFCR = DMA_IFCR_CGIF1;
    /*
     * With DIR set, STM32 DMA reads from CMAR and writes to CPAR. MEM2MEM keeps
     * those source/destination roles, so the prepared frame must be CMAR.
     */
    DMA1_Channel1->CPAR = (uint32_t)&g_dma_target;
    DMA1_Channel1->CMAR = (uint32_t)&g_dma_source;
    DMA1_Channel1->CNDTR = FRAME_BYTES;
    DMA1_Channel1->CCR = DMA_CCR_MEM2MEM
                         | DMA_CCR_DIR
                         | DMA_CCR_PINC
                         | DMA_CCR_MINC
                         | DMA_CCR_TEIE
                         | DMA_CCR_PL_1;
    DMA1_Channel1->CCR |= DMA_CCR_EN;

    while (((DMA1->ISR & (DMA_ISR_TCIF1 | DMA_ISR_TEIF1)) == 0U)
            && (timeout > 0U)) {
        --timeout;
    }

    DMA1_Channel1->CCR &= ~DMA_CCR_EN;

    if (((DMA1->ISR & DMA_ISR_TEIF1) != 0U) || (timeout == 0U)) {
        DMA1->IFCR = DMA_IFCR_CGIF1;
        return false;
    }

    DMA1->IFCR = DMA_IFCR_CGIF1;
    out->segment_bits = g_dma_target.segment_bits;
    out->digit_bits = g_dma_target.digit_bits;
    return true;
}

static void configure_board(void)
{
    SystemCoreClockUpdate();

    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;
    (void)RCC->AHB1ENR;
    (void)RCC->AHB2ENR;

    gpio_output(k_latch_pin);
    gpio_output(k_clock_pin);
    gpio_output(k_data_pin);

    pin_write(k_latch_pin, true);
    pin_write(k_clock_pin, false);
    pin_write(k_data_pin, false);
    write_display_frame((display_frame_t){0xFFU, 0xFFU});

    SysTick_Config(SystemCoreClock / 1000U);
}

int main(void)
{
    uint8_t scan_digit = 0U;
    uint8_t message_offset = 0U;
    uint32_t last_refresh_ms = 0U;
    uint32_t last_message_ms = 0U;

    configure_board();

    while (true) {
        const uint32_t current_ms = now_ms();

        if ((uint32_t)(current_ms - last_refresh_ms) >= REFRESH_MS) {
            display_frame_t frame;
            display_frame_t copied_frame;

            last_refresh_ms = current_ms;
            frame = make_frame(scan_digit, message_offset);

            if (dma_copy_frame(frame, &copied_frame)) {
                write_display_frame(copied_frame);
            } else {
                g_dma_error = true;
                write_display_frame(frame);
            }

            scan_digit = (uint8_t)((scan_digit + 1U) % DIGIT_COUNT);
        }

        if ((uint32_t)(current_ms - last_message_ms) >= MESSAGE_STEP_MS) {
            const uint8_t message_span = (uint8_t)(sizeof(k_message) - 1U);

            last_message_ms = current_ms;
            message_offset = (uint8_t)((message_offset + 1U) % message_span);
        }

        __WFI();
    }
}
