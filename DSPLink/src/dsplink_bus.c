#include "dsplink_bus.h"

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx.h"

enum {
    I2C_STANDARD_HZ = 100000U,
    I2C_TIMEOUT_SPINS = 100000U,
    I2C1_PCLK1_HZ = 16000000U,
    I2C1_PCLK1_MHZ = I2C1_PCLK1_HZ / 1000000U
};

static void i2c_gpio_alt_open_drain(GPIO_TypeDef *port, uint8_t pin_no, uint8_t af)
{
    const uint32_t mode_shift = (uint32_t)pin_no * 2U;
    const uint32_t mode_mask = 0x3UL << mode_shift;
    const uint32_t afr_shift = (uint32_t)(pin_no % 8U) * 4U;
    const uint32_t afr_mask = 0xFUL << afr_shift;
    volatile uint32_t *afr = &port->AFR[pin_no / 8U];

    port->MODER = (port->MODER & ~mode_mask) | (0x2UL << mode_shift);
    port->OTYPER |= 1UL << pin_no;
    port->OSPEEDR = (port->OSPEEDR & ~mode_mask) | (0x2UL << mode_shift);
    port->PUPDR = (port->PUPDR & ~mode_mask) | (0x1UL << mode_shift);
    *afr = (*afr & ~afr_mask) | ((uint32_t)af << afr_shift);
}

static bool i2c_wait_set(volatile uint32_t *reg, uint32_t mask)
{
    uint32_t spins = 0U;

    while (((*reg & mask) == 0U) && (spins < I2C_TIMEOUT_SPINS)) {
        spins++;
    }

    return spins < I2C_TIMEOUT_SPINS;
}

static bool i2c_wait_clear(volatile uint32_t *reg, uint32_t mask)
{
    uint32_t spins = 0U;

    while (((*reg & mask) != 0U) && (spins < I2C_TIMEOUT_SPINS)) {
        spins++;
    }

    return spins < I2C_TIMEOUT_SPINS;
}

void dsplink_bus_i2c1_init(void)
{
    const uint32_t ccr = I2C1_PCLK1_HZ / (2U * I2C_STANDARD_HZ);

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    (void)RCC->AHB1ENR;
    (void)RCC->APB1ENR;

    i2c_gpio_alt_open_drain(GPIOB, 8U, 4U); /* CN12 pin 3 / PB8 / I2C1_SCL */
    i2c_gpio_alt_open_drain(GPIOB, 9U, 4U); /* CN12 pin 5 / PB9 / I2C1_SDA */

    I2C1->CR1 = I2C_CR1_SWRST;
    I2C1->CR1 = 0U;
    I2C1->CR2 = I2C1_PCLK1_MHZ;
    I2C1->CCR = ccr;
    I2C1->TRISE = I2C1_PCLK1_MHZ + 1U;
    I2C1->CR1 = I2C_CR1_PE;
}

static dsplink_bus_status_t i2c_send_register_subaddress(
        uint8_t address_7bit,
        uint16_t register_address)
{
    if ((I2C1->SR2 & I2C_SR2_BUSY) != 0U) {
        return DSPLINK_BUS_ERROR_BUSY;
    }

    I2C1->CR1 |= I2C_CR1_START;
    if (!i2c_wait_set(&I2C1->SR1, I2C_SR1_SB)) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return DSPLINK_BUS_ERROR_TIMEOUT;
    }

    I2C1->DR = (uint8_t)(address_7bit << 1U);
    if (!i2c_wait_set(&I2C1->SR1, I2C_SR1_ADDR | I2C_SR1_AF)) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return DSPLINK_BUS_ERROR_TIMEOUT;
    }

    if ((I2C1->SR1 & I2C_SR1_AF) != 0U) {
        I2C1->SR1 &= ~I2C_SR1_AF;
        I2C1->CR1 |= I2C_CR1_STOP;
        (void)i2c_wait_clear(&I2C1->SR2, I2C_SR2_BUSY);
        return DSPLINK_BUS_ERROR_TIMEOUT;
    }

    (void)I2C1->SR1;
    (void)I2C1->SR2;

    if (!i2c_wait_set(&I2C1->SR1, I2C_SR1_TXE)) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return DSPLINK_BUS_ERROR_TIMEOUT;
    }
    I2C1->DR = (uint8_t)(register_address >> 8U);

    if (!i2c_wait_set(&I2C1->SR1, I2C_SR1_TXE)) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return DSPLINK_BUS_ERROR_TIMEOUT;
    }
    I2C1->DR = (uint8_t)register_address;

    if (!i2c_wait_set(&I2C1->SR1, I2C_SR1_BTF)) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return DSPLINK_BUS_ERROR_TIMEOUT;
    }

    return DSPLINK_BUS_OK;
}

dsplink_bus_status_t dsplink_bus_i2c1_probe(uint8_t address_7bit)
{
    volatile uint32_t clear;
    uint32_t spins = 0U;

    if ((I2C1->SR2 & I2C_SR2_BUSY) != 0U) {
        return DSPLINK_BUS_ERROR_BUSY;
    }

    I2C1->CR1 |= I2C_CR1_START;
    if (!i2c_wait_set(&I2C1->SR1, I2C_SR1_SB)) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return DSPLINK_BUS_ERROR_TIMEOUT;
    }

    I2C1->DR = (uint8_t)(address_7bit << 1U);

    while ((I2C1->SR1 & (I2C_SR1_ADDR | I2C_SR1_AF)) == 0U) {
        spins++;
        if (spins >= I2C_TIMEOUT_SPINS) {
            I2C1->CR1 |= I2C_CR1_STOP;
            return DSPLINK_BUS_ERROR_TIMEOUT;
        }
    }

    if ((I2C1->SR1 & I2C_SR1_ADDR) != 0U) {
        clear = I2C1->SR1;
        clear = I2C1->SR2;
        (void)clear;
        I2C1->CR1 |= I2C_CR1_STOP;
        (void)i2c_wait_clear(&I2C1->SR2, I2C_SR2_BUSY);
        return DSPLINK_BUS_OK;
    }

    I2C1->SR1 &= ~I2C_SR1_AF;
    I2C1->CR1 |= I2C_CR1_STOP;
    (void)i2c_wait_clear(&I2C1->SR2, I2C_SR2_BUSY);
    return DSPLINK_BUS_ERROR_TIMEOUT;
}

dsplink_bus_status_t dsplink_bus_adau1701_read_u16(
        uint8_t address_7bit,
        uint16_t register_address,
        uint16_t *value_out)
{
    volatile uint32_t clear;
    dsplink_bus_status_t status;
    uint8_t high;
    uint8_t low;

    if (value_out == 0) {
        return DSPLINK_BUS_ERROR_TIMEOUT;
    }

    status = i2c_send_register_subaddress(address_7bit, register_address);
    if (status != DSPLINK_BUS_OK) {
        return status;
    }

    I2C1->CR1 |= I2C_CR1_ACK | I2C_CR1_POS | I2C_CR1_START;
    if (!i2c_wait_set(&I2C1->SR1, I2C_SR1_SB)) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return DSPLINK_BUS_ERROR_TIMEOUT;
    }

    I2C1->DR = (uint8_t)((address_7bit << 1U) | 1U);
    if (!i2c_wait_set(&I2C1->SR1, I2C_SR1_ADDR | I2C_SR1_AF)) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return DSPLINK_BUS_ERROR_TIMEOUT;
    }

    if ((I2C1->SR1 & I2C_SR1_AF) != 0U) {
        I2C1->SR1 &= ~I2C_SR1_AF;
        I2C1->CR1 |= I2C_CR1_STOP;
        (void)i2c_wait_clear(&I2C1->SR2, I2C_SR2_BUSY);
        return DSPLINK_BUS_ERROR_TIMEOUT;
    }

    I2C1->CR1 &= ~I2C_CR1_ACK;
    clear = I2C1->SR1;
    clear = I2C1->SR2;
    (void)clear;

    if (!i2c_wait_set(&I2C1->SR1, I2C_SR1_BTF)) {
        I2C1->CR1 |= I2C_CR1_STOP;
        I2C1->CR1 &= ~I2C_CR1_POS;
        I2C1->CR1 |= I2C_CR1_ACK;
        return DSPLINK_BUS_ERROR_TIMEOUT;
    }

    I2C1->CR1 |= I2C_CR1_STOP;
    high = (uint8_t)I2C1->DR;
    low = (uint8_t)I2C1->DR;

    I2C1->CR1 &= ~I2C_CR1_POS;
    I2C1->CR1 |= I2C_CR1_ACK;
    (void)i2c_wait_clear(&I2C1->SR2, I2C_SR2_BUSY);

    *value_out = (uint16_t)(((uint16_t)high << 8U) | low);
    return DSPLINK_BUS_OK;
}

dsplink_bus_status_t dsplink_bus_adau1701_write_u16(
        uint8_t address_7bit,
        uint16_t register_address,
        uint16_t value)
{
    dsplink_bus_status_t status =
            i2c_send_register_subaddress(address_7bit, register_address);

    if (status != DSPLINK_BUS_OK) {
        return status;
    }

    if (!i2c_wait_set(&I2C1->SR1, I2C_SR1_TXE)) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return DSPLINK_BUS_ERROR_TIMEOUT;
    }
    I2C1->DR = (uint8_t)(value >> 8U);

    if (!i2c_wait_set(&I2C1->SR1, I2C_SR1_TXE)) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return DSPLINK_BUS_ERROR_TIMEOUT;
    }
    I2C1->DR = (uint8_t)value;

    if (!i2c_wait_set(&I2C1->SR1, I2C_SR1_BTF)) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return DSPLINK_BUS_ERROR_TIMEOUT;
    }

    I2C1->CR1 |= I2C_CR1_STOP;
    (void)i2c_wait_clear(&I2C1->SR2, I2C_SR2_BUSY);
    return DSPLINK_BUS_OK;
}

dsplink_bus_status_t dsplink_bus_adau1701_write_block(
        uint8_t address_7bit,
        uint16_t start_address,
        const uint8_t *data,
        uint16_t length)
{
    uint16_t index;
    dsplink_bus_status_t status;

    if ((data == 0) || (length == 0U)) {
        return DSPLINK_BUS_ERROR_TIMEOUT;
    }

    status = i2c_send_register_subaddress(address_7bit, start_address);
    if (status != DSPLINK_BUS_OK) {
        return status;
    }

    for (index = 0U; index < length; ++index) {
        if (!i2c_wait_set(&I2C1->SR1, I2C_SR1_TXE)) {
            I2C1->CR1 |= I2C_CR1_STOP;
            return DSPLINK_BUS_ERROR_TIMEOUT;
        }
        I2C1->DR = data[index];
    }

    if (!i2c_wait_set(&I2C1->SR1, I2C_SR1_BTF)) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return DSPLINK_BUS_ERROR_TIMEOUT;
    }

    I2C1->CR1 |= I2C_CR1_STOP;
    (void)i2c_wait_clear(&I2C1->SR2, I2C_SR2_BUSY);
    return DSPLINK_BUS_OK;
}

dsplink_bus_scan_t dsplink_bus_i2c1_scan(void)
{
    uint8_t address;
    dsplink_bus_scan_t scan = {false, false, false, 0U, 0U, 0U, 0U};

    for (address = DSPLINK_I2C_SCAN_FIRST_ADDRESS;
         address <= DSPLINK_I2C_SCAN_LAST_ADDRESS;
         address++) {
        const dsplink_bus_status_t status = dsplink_bus_i2c1_probe(address);

        if (status == DSPLINK_BUS_OK) {
            scan.found_any_device = true;
            if (scan.device_count < UINT8_MAX) {
                scan.device_count++;
            }
            if ((address >= DSPLINK_ADAU1701_ADDRESS_FIRST)
                    && (address <= DSPLINK_ADAU1701_ADDRESS_LAST)) {
                scan.found_adau1701 = true;
                scan.adau1701_address = address;
            }
        }
    }

    if (scan.found_adau1701) {
        const dsplink_bus_status_t core_status =
                dsplink_bus_adau1701_read_u16(
                    scan.adau1701_address,
                    DSPLINK_ADAU1701_CORE_CONTROL_REGISTER,
                    &scan.dsp_core_control);
        const dsplink_bus_status_t dac_status =
                dsplink_bus_adau1701_read_u16(
                    scan.adau1701_address,
                    DSPLINK_ADAU1701_DAC_SETUP_REGISTER,
                    &scan.dac_setup);

        scan.control_read_ok =
                (core_status == DSPLINK_BUS_OK) && (dac_status == DSPLINK_BUS_OK);
    }

    return scan;
}

const char *dsplink_bus_status_name(dsplink_bus_status_t status)
{
    switch (status) {
    case DSPLINK_BUS_OK:
        return "OK";
    case DSPLINK_BUS_ERROR_BUSY:
        return "BUSY";
    case DSPLINK_BUS_ERROR_TIMEOUT:
    default:
        return "TIMEOUT";
    }
}
