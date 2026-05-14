#include "temp_sensor.h"

#include <stddef.h>
#include <stdint.h>

#include "stm32l476xx.h"

enum {
    ADC_SEQUENCE_CHANNELS = 2U,
    ADC_SEQUENCE_COUNT = 16U,
    ADC_SAMPLE_COUNT = ADC_SEQUENCE_CHANNELS * ADC_SEQUENCE_COUNT,
    ADC_TEMP_INDEX = 0U,
    ADC_VREF_INDEX = 1U,
    ADC_CHANNEL_VREFINT = 0U,
    ADC_CHANNEL_TEMPSENSOR = 17U,
    ADC_TIMEOUT_ITERATIONS = 1000000U,
    VREFINT_CAL_MV = 3000U,
    TS_CAL1_TEMP_C = 30,
    TS_CAL2_TEMP_C = 110,
    TS_CAL1_ADDR = 0x1FFF75A8UL,
    TS_CAL2_ADDR = 0x1FFF75CAUL,
    VREFINT_CAL_ADDR = 0x1FFF75AAUL,
};

static volatile uint16_t g_adc_samples[ADC_SAMPLE_COUNT];
static bool g_adc_fault = false;
static bool g_dma_fault = false;

static void delay_cycles(volatile uint32_t cycles)
{
    while (cycles > 0U) {
        --cycles;
    }
}

static uint16_t read_calibration_halfword(uint32_t address)
{
    return *(const uint16_t *)address;
}

static bool wait_while_set(volatile uint32_t *reg, uint32_t mask)
{
    uint32_t timeout = ADC_TIMEOUT_ITERATIONS;

    while (((*reg & mask) != 0U) && (timeout > 0U)) {
        --timeout;
    }

    return timeout > 0U;
}

static bool wait_until_set(volatile uint32_t *reg, uint32_t mask)
{
    uint32_t timeout = ADC_TIMEOUT_ITERATIONS;

    while (((*reg & mask) == 0U) && (timeout > 0U)) {
        --timeout;
    }

    return timeout > 0U;
}

bool temp_sensor_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;
    (void)RCC->AHB1ENR;
    (void)RCC->AHB2ENR;

    ADC1->CR &= ~ADC_CR_ADEN;
    ADC1->CR &= ~ADC_CR_DEEPPWD;
    ADC1->CR |= ADC_CR_ADVREGEN;
    delay_cycles(SystemCoreClock / 10000U);

    ADC123_COMMON->CCR = (ADC123_COMMON->CCR & ~(ADC_CCR_CKMODE | ADC_CCR_PRESC))
                         | ADC_CCR_CKMODE_0
                         | ADC_CCR_CKMODE_1
                         | ADC_CCR_TSEN
                         | ADC_CCR_VREFEN;

    ADC1->DIFSEL = 0U;
    ADC1->CR |= ADC_CR_ADCAL;
    if (!wait_while_set(&ADC1->CR, ADC_CR_ADCAL)) {
        g_adc_fault = true;
        return false;
    }

    DMA1_Channel1->CCR &= ~DMA_CCR_EN;
    DMA1->IFCR = DMA_IFCR_CGIF1;
    DMA1_CSELR->CSELR &= ~DMA_CSELR_C1S; /* Request 0: ADC1 on STM32L476. */
    DMA1_Channel1->CPAR = (uint32_t)&ADC1->DR;
    DMA1_Channel1->CMAR = (uint32_t)g_adc_samples;
    DMA1_Channel1->CNDTR = ADC_SAMPLE_COUNT;
    DMA1_Channel1->CCR = DMA_CCR_MINC
                         | DMA_CCR_CIRC
                         | DMA_CCR_PSIZE_0
                         | DMA_CCR_MSIZE_0
                         | DMA_CCR_PL_1;

    ADC1->SMPR1 = (ADC1->SMPR1 & ~ADC_SMPR1_SMP0) | ADC_SMPR1_SMP0_Msk;
    ADC1->SMPR2 = (ADC1->SMPR2 & ~ADC_SMPR2_SMP17) | ADC_SMPR2_SMP17_Msk;
    ADC1->SQR1 = (1UL << ADC_SQR1_L_Pos)
                 | (ADC_CHANNEL_TEMPSENSOR << ADC_SQR1_SQ1_Pos)
                 | (ADC_CHANNEL_VREFINT << ADC_SQR1_SQ2_Pos);
    ADC1->CFGR = ADC_CFGR_DMAEN
                 | ADC_CFGR_DMACFG
                 | ADC_CFGR_CONT
                 | ADC_CFGR_OVRMOD;
    ADC1->IER = ADC_IER_OVRIE;
    ADC1->ISR = ADC_ISR_ADRDY | ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR;

    NVIC_SetPriority(ADC1_2_IRQn, 1U);
    NVIC_EnableIRQ(ADC1_2_IRQn);

    DMA1_Channel1->CCR |= DMA_CCR_EN;
    ADC1->CR |= ADC_CR_ADEN;
    if (!wait_until_set(&ADC1->ISR, ADC_ISR_ADRDY)) {
        g_adc_fault = true;
        return false;
    }

    ADC1->CR |= ADC_CR_ADSTART;
    return true;
}

void ADC1_2_IRQHandler(void)
{
    if ((ADC1->ISR & ADC_ISR_OVR) != 0U) {
        g_adc_fault = true;
        ADC1->ISR = ADC_ISR_OVR;
    }
}

static bool adc_dma_has_samples(void)
{
    if ((DMA1->ISR & DMA_ISR_TEIF1) != 0U) {
        g_dma_fault = true;
        DMA1->IFCR = DMA_IFCR_CTEIF1;
        return false;
    }

    return ((DMA1->ISR & DMA_ISR_TCIF1) != 0U)
           || (DMA1_Channel1->CNDTR < ADC_SAMPLE_COUNT);
}

bool temp_sensor_read(temp_sensor_snapshot_t *snapshot)
{
    uint32_t temp_sum = 0U;
    uint32_t vref_sum = 0U;
    uint32_t index;
    uint32_t temp_raw;
    uint32_t vref_raw;
    uint32_t vdda_mv;
    uint32_t temp_corrected;
    const uint32_t ts_cal1 = read_calibration_halfword(TS_CAL1_ADDR);
    const uint32_t ts_cal2 = read_calibration_halfword(TS_CAL2_ADDR);
    const uint32_t vref_cal = read_calibration_halfword(VREFINT_CAL_ADDR);

    if (snapshot == NULL) {
        return false;
    }

    snapshot->adc_fault = g_adc_fault;
    snapshot->dma_fault = g_dma_fault;
    snapshot->valid = false;

    if (!adc_dma_has_samples()) {
        snapshot->adc_fault = g_adc_fault;
        snapshot->dma_fault = g_dma_fault;
        return false;
    }

    for (index = 0U; index < ADC_SAMPLE_COUNT; index += ADC_SEQUENCE_CHANNELS) {
        temp_sum += g_adc_samples[index + ADC_TEMP_INDEX];
        vref_sum += g_adc_samples[index + ADC_VREF_INDEX];
    }

    temp_raw = temp_sum / ADC_SEQUENCE_COUNT;
    vref_raw = vref_sum / ADC_SEQUENCE_COUNT;

    if ((vref_raw == 0U) || (ts_cal2 <= ts_cal1) || (vref_cal == 0U)) {
        return true;
    }

    vdda_mv = (VREFINT_CAL_MV * vref_cal) / vref_raw;
    temp_corrected = (temp_raw * vdda_mv) / VREFINT_CAL_MV;

    snapshot->temp_raw = (uint16_t)temp_raw;
    snapshot->vref_raw = (uint16_t)vref_raw;
    snapshot->vdda_mv = vdda_mv;
    snapshot->temp_centi_c =
            ((((int32_t)temp_corrected - (int32_t)ts_cal1)
              * ((TS_CAL2_TEMP_C - TS_CAL1_TEMP_C) * 100))
             / ((int32_t)ts_cal2 - (int32_t)ts_cal1))
            + (TS_CAL1_TEMP_C * 100);
    snapshot->adc_fault = g_adc_fault;
    snapshot->dma_fault = g_dma_fault;
    snapshot->valid = !g_adc_fault && !g_dma_fault;

    return true;
}
