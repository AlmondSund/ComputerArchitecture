.syntax unified
.cpu cortex-m4
.thumb

#ifndef STM32L476xx
#error "src/main.S targets STM32L476xx register map (NUCLEO-L476RG)."
#endif

.equ RCC_BASE,       0x40021000
.equ RCC_CR,         (RCC_BASE + 0x00)
.equ RCC_CFGR,       (RCC_BASE + 0x08)
.equ RCC_PLLCFGR,    (RCC_BASE + 0x0C)
.equ RCC_CSR,        (RCC_BASE + 0x94)

.equ RCC_AHB2ENR,    (RCC_BASE + 0x4C)
.equ RCC_APB1ENR1,   (RCC_BASE + 0x58)

.equ HSE_VALUE_HZ,   8000000
.equ HSI16_VALUE_HZ, 16000000

.equ GPIOA_BASE,     0x48000000
.equ GPIOA_MODER,    (GPIOA_BASE + 0x00)
.equ GPIOA_BSRR,     (GPIOA_BASE + 0x18)
.equ GPIOB_BASE,     0x48000400
.equ GPIOB_MODER,    (GPIOB_BASE + 0x00)
.equ GPIOB_BSRR,     (GPIOB_BASE + 0x18)

.equ TIM2_BASE,      0x40000000
.equ TIM3_BASE,      0x40000400
.equ TIM4_BASE,      0x40000800
.equ TIM5_BASE,      0x40000C00
.equ TIM_CR1_OFF,    0x00
.equ TIM_SR_OFF,     0x10
.equ TIM_EGR_OFF,    0x14
.equ TIM_PSC_OFF,    0x28
.equ TIM_ARR_OFF,    0x2C

/* Multi Function Shield LED mapping on NUCLEO-L476RG Arduino header */
.equ D1_PIN,         5   /* D1 -> Arduino D13 -> PA5 */
.equ D2_PIN,         6   /* D2 -> Arduino D12 -> PA6 */
.equ D3_PIN,         7   /* D3 -> Arduino D11 -> PA7 */
.equ D4_PIN,         6   /* D4 -> Arduino D10 -> PB6 */

/* TIM2..TIM5 clock enable bits in RCC_APB1ENR1 */
.equ RCC_TIM2EN_BIT,  0
.equ RCC_TIM3EN_BIT,  1
.equ RCC_TIM4EN_BIT,  2
.equ RCC_TIM5EN_BIT,  3

/*
 * Queremos un tick ~32 kHz, pero el clock real puede ser 4/16/80 MHz, etc.
 * PSC se calcula en runtime: PSC = (TIMCLK / 32000) - 1
 */
.equ TIM_TICK_TARGET_HZ, 32000

/* Con tick 32 kHz, estos ARR dan los toggles deseados */
.equ TIM_D1_TOGGLE_ARR, 15999  /* 32 kHz / 16000 = 2 Hz toggle -> 1 blink/s */
.equ TIM_D2_TOGGLE_ARR,  7999  /* 32 kHz /  8000 = 4 Hz toggle -> 2 blinks/s */
.equ TIM_D3_TOGGLE_ARR,  5332  /* 32 kHz /  5333 = 6.0004 Hz toggle ~ 3 blinks/s */
.equ TIM_D4_TOGGLE_ARR,  3999  /* 32 kHz /  4000 = 8 Hz toggle -> 4 blinks/s */

/* Shield LEDs are active-low (pin LOW = LED ON) */
.equ GPIOA_LED_SET_MASK,          ((1 << D1_PIN) | (1 << D2_PIN) | (1 << D3_PIN))
.equ GPIOA_LED_RESET_MASK,        ((1 << (D1_PIN + 16)) | (1 << (D2_PIN + 16)) | (1 << (D3_PIN + 16)))
.equ GPIOA_LED_MODER_CLEAR_MASK,  ((3 << (D1_PIN * 2)) | (3 << (D2_PIN * 2)) | (3 << (D3_PIN * 2)))
.equ GPIOA_LED_MODER_OUTPUT_MASK, ((1 << (D1_PIN * 2)) | (1 << (D2_PIN * 2)) | (1 << (D3_PIN * 2)))
.equ GPIOB_LED_SET_MASK,          (1 << D4_PIN)
.equ GPIOB_LED_RESET_MASK,        (1 << (D4_PIN + 16))
.equ GPIOB_LED_MODER_CLEAR_MASK,  (3 << (D4_PIN * 2))
.equ GPIOB_LED_MODER_OUTPUT_MASK, (1 << (D4_PIN * 2))

.section .text
.align  2
.global main
.type   main, %function

.thumb_func
main:
    /* Enable GPIOA and GPIOB clocks */
    ldr r0, =RCC_AHB2ENR
    ldr r1, [r0]
    orr r1, r1, #(1 << 0)  /* GPIOAEN */
    orr r1, r1, #(1 << 1)  /* GPIOBEN */
    str r1, [r0]
    ldr r1, [r0]           /* Ensure clock gate write has completed */
    dsb
    isb

    /* Force all shield LEDs OFF before switching mode to avoid startup glitches */
    ldr r0, =GPIOA_BSRR
    ldr r1, =GPIOA_LED_SET_MASK
    str r1, [r0]
    ldr r0, =GPIOB_BSRR
    ldr r1, =GPIOB_LED_SET_MASK
    str r1, [r0]

    /* Configure PA5/PA6/PA7 as outputs */
    ldr r0, =GPIOA_MODER
    ldr r1, [r0]
    ldr r2, =GPIOA_LED_MODER_CLEAR_MASK
    bic r1, r1, r2
    ldr r2, =GPIOA_LED_MODER_OUTPUT_MASK
    orr r1, r1, r2
    str r1, [r0]

    /* Configure PB6 as output */
    ldr r0, =GPIOB_MODER
    ldr r1, [r0]
    ldr r2, =GPIOB_LED_MODER_CLEAR_MASK
    bic r1, r1, r2
    ldr r2, =GPIOB_LED_MODER_OUTPUT_MASK
    orr r1, r1, r2
    str r1, [r0]

    bl leds_all_off
    bl timers_init

    /* Cache timer base addresses and LED software states (0=OFF, 1=ON) */
    ldr r4, =TIM2_BASE  /* D1 */
    ldr r5, =TIM3_BASE  /* D2 */
    ldr r6, =TIM4_BASE  /* D3 */
    ldr r7, =TIM5_BASE  /* D4 */
    mov r8, #0
    mov r9, #0
    mov r10, #0
    mov r11, #0

.loop:
    /* D1: timer-based 1 blink/s */
    mov r0, r4
    bl timer_update_elapsed
    cmp r0, #0
    beq 1f
    mov r0, r8
    eor r0, r0, #1
    mov r8, r0
    cmp r0, #0
    beq 2f
    bl led_d1_on
    b 1f
2:
    bl led_d1_off
1:

    /* D2: timer-based 2 blinks/s */
    mov r0, r5
    bl timer_update_elapsed
    cmp r0, #0
    beq 3f
    mov r0, r9
    eor r0, r0, #1
    mov r9, r0
    cmp r0, #0
    beq 4f
    bl led_d2_on
    b 3f
4:
    bl led_d2_off
3:

    /* D3: timer-based 3 blinks/s */
    mov r0, r6
    bl timer_update_elapsed
    cmp r0, #0
    beq 5f
    mov r0, r10
    eor r0, r0, #1
    mov r10, r0
    cmp r0, #0
    beq 6f
    bl led_d3_on
    b 5f
6:
    bl led_d3_off
5:

    /* D4: timer-based 4 blinks/s */
    mov r0, r7
    bl timer_update_elapsed
    cmp r0, #0
    beq 7f
    mov r0, r11
    eor r0, r0, #1
    mov r11, r0
    cmp r0, #0
    beq 8f
    bl led_d4_on
    b 7f
8:
    bl led_d4_off
7:

    b .loop

.align 2
.type leds_all_on, %function
.thumb_func
leds_all_on:
    ldr r0, =GPIOA_BSRR
    ldr r1, =GPIOA_LED_RESET_MASK
    str r1, [r0]
    ldr r0, =GPIOB_BSRR
    ldr r1, =GPIOB_LED_RESET_MASK
    str r1, [r0]
    bx lr

.type leds_all_off, %function
.thumb_func
leds_all_off:
    ldr r0, =GPIOA_BSRR
    ldr r1, =GPIOA_LED_SET_MASK
    str r1, [r0]
    ldr r0, =GPIOB_BSRR
    ldr r1, =GPIOB_LED_SET_MASK
    str r1, [r0]
    bx lr

.type led_d1_on, %function
.thumb_func
led_d1_on:
    ldr r0, =GPIOA_BSRR
    ldr r1, =(1 << (D1_PIN + 16))
    str r1, [r0]
    bx lr

.type led_d1_off, %function
.thumb_func
led_d1_off:
    ldr r0, =GPIOA_BSRR
    ldr r1, =(1 << D1_PIN)
    str r1, [r0]
    bx lr

.type led_d2_on, %function
.thumb_func
led_d2_on:
    ldr r0, =GPIOA_BSRR
    ldr r1, =(1 << (D2_PIN + 16))
    str r1, [r0]
    bx lr

.type led_d2_off, %function
.thumb_func
led_d2_off:
    ldr r0, =GPIOA_BSRR
    ldr r1, =(1 << D2_PIN)
    str r1, [r0]
    bx lr

.type led_d3_on, %function
.thumb_func
led_d3_on:
    ldr r0, =GPIOA_BSRR
    ldr r1, =(1 << (D3_PIN + 16))
    str r1, [r0]
    bx lr

.type led_d3_off, %function
.thumb_func
led_d3_off:
    ldr r0, =GPIOA_BSRR
    ldr r1, =(1 << D3_PIN)
    str r1, [r0]
    bx lr

.type led_d4_on, %function
.thumb_func
led_d4_on:
    ldr r0, =GPIOB_BSRR
    ldr r1, =(1 << (D4_PIN + 16))
    str r1, [r0]
    bx lr

.type led_d4_off, %function
.thumb_func
led_d4_off:
    ldr r0, =GPIOB_BSRR
    ldr r1, =(1 << D4_PIN)
    str r1, [r0]
    bx lr

.type timers_init, %function
.thumb_func
timers_init:
    push {r4, lr}

    /* Enable TIM2/TIM3/TIM4/TIM5 clocks */
    ldr r0, =RCC_APB1ENR1
    ldr r1, [r0]
    orr r1, r1, #(1 << RCC_TIM2EN_BIT)
    orr r1, r1, #(1 << RCC_TIM3EN_BIT)
    orr r1, r1, #(1 << RCC_TIM4EN_BIT)
    orr r1, r1, #(1 << RCC_TIM5EN_BIT)
    str r1, [r0]
    ldr r1, [r0]
    dsb
    isb

    /* CALC PSC so timer tick ~32 kHz regardless of current clock tree */
    bl timers_calc_psc_32khz
    mov r4, r0    /* r4 = PSC */

    ldr r0, =TIM2_BASE
    ldr r1, =TIM_D1_TOGGLE_ARR
    mov r2, r4
    bl timer_start

    ldr r0, =TIM3_BASE
    ldr r1, =TIM_D2_TOGGLE_ARR
    mov r2, r4
    bl timer_start

    ldr r0, =TIM4_BASE
    ldr r1, =TIM_D3_TOGGLE_ARR
    mov r2, r4
    bl timer_start

    ldr r0, =TIM5_BASE
    ldr r1, =TIM_D4_TOGGLE_ARR
    mov r2, r4
    bl timer_start

    pop {r4, pc}

.type timer_start, %function
.thumb_func
/* r0=TIMx base, r1=ARR, r2=PSC */
timer_start:
    str r2, [r0, #TIM_PSC_OFF]
    str r1, [r0, #TIM_ARR_OFF]
    movs r3, #1
    str r3, [r0, #TIM_EGR_OFF] /* Load PSC/ARR immediately */
    movs r3, #0
    str r3, [r0, #TIM_SR_OFF]  /* Clear update flag */
    movs r3, #1
    str r3, [r0, #TIM_CR1_OFF] /* Counter enable */
    bx lr

.type timer_update_elapsed, %function
.thumb_func
timer_update_elapsed:
    ldr r1, [r0, #TIM_SR_OFF]
    tst r1, #1
    beq 1f
    movs r1, #0
    str r1, [r0, #TIM_SR_OFF]
    movs r0, #1
    bx lr
1:
    movs r0, #0
    bx lr

/* -------------------- Clock helpers -------------------- */

.align 2
.type timers_calc_psc_32khz, %function
.thumb_func
timers_calc_psc_32khz:
    push {lr}
    bl rcc_get_timclk1_hz          /* r0 = TIMCLK1 Hz */
    ldr r1, =TIM_TICK_TARGET_HZ    /* 32000 */
    udiv r0, r0, r1                /* r0 = TIMCLK1/32000 */
    subs r0, r0, #1                /* PSC = div - 1 */
    pop {pc}

.align 2
.type rcc_get_timclk1_hz, %function
.thumb_func
rcc_get_timclk1_hz:
    push {r4-r6, lr}

    /* Read CFGR once */
    ldr r4, =RCC_CFGR
    ldr r4, [r4]

    /* SYSCLK from SWS[1:0] (bits 3:2) */
    lsrs r5, r4, #2
    ands r5, r5, #3
    cmp r5, #0
    beq 1f              /* MSI */
    cmp r5, #1
    beq 2f              /* HSI16 */
    cmp r5, #2
    beq 3f              /* HSE */
    /* PLL */
    bl rcc_get_pll_r_hz
    b 4f
1:
    bl rcc_get_msi_hz
    b 4f
2:
    ldr r0, =HSI16_VALUE_HZ
    b 4f
3:
    ldr r0, =HSE_VALUE_HZ
4:
    /* r0 = SYSCLK */

    /* Apply HPRE (bits 7:4) to get HCLK */
    lsrs r5, r4, #4
    ands r5, r5, #0x0F
    ldr r6, =hpre_div_table
    ldr r1, [r6, r5, lsl #2]   /* r1 = hpre_div */
    udiv r0, r0, r1            /* r0 = HCLK */

    /* Apply PPRE1 (bits 10:8) to get PCLK1 */
    lsrs r5, r4, #8
    ands r5, r5, #0x07
    ldr r6, =ppre_div_table
    ldr r1, [r6, r5, lsl #2]   /* r1 = ppre1_div */
    udiv r0, r0, r1            /* r0 = PCLK1 */

    /* TIMCLK1 = PCLK1 if PPRE1==/1 else 2*PCLK1 */
    cmp r5, #4
    blt 5f
    lsls r0, r0, #1
5:
    pop {r4-r6, pc}

.align 2
.type rcc_get_msi_hz, %function
.thumb_func
rcc_get_msi_hz:
    /* MSI range selection: MSIRGSEL decides if MSIRANGE from CR or MSISRANGE from CSR */
    ldr r0, =RCC_CR
    ldr r1, [r0]
    tst r1, #(1 << 3)           /* MSIRGSEL */
    beq 1f

    /* range = CR.MSIRANGE[7:4] */
    lsrs r2, r1, #4
    ands r2, r2, #0x0F
    b 2f
1:
    /* range = CSR.MSISRANGE[11:8] */
    ldr r0, =RCC_CSR
    ldr r1, [r0]
    lsrs r2, r1, #8
    ands r2, r2, #0x0F
2:
    ldr r0, =msi_freq_table
    ldr r0, [r0, r2, lsl #2]
    bx lr

.align 2
.type rcc_get_pll_r_hz, %function
.thumb_func
rcc_get_pll_r_hz:
    push {r4-r6, lr}

    ldr r4, =RCC_PLLCFGR
    ldr r5, [r4]

    /* PLLSRC[1:0] */
    ands r6, r5, #3
    cmp r6, #1
    beq .pll_src_msi
    cmp r6, #2
    beq .pll_src_hsi
    cmp r6, #3
    beq .pll_src_hse
    movs r0, #0
    b .pll_done

.pll_src_msi:
    bl rcc_get_msi_hz
    b .pll_have_src
.pll_src_hsi:
    ldr r0, =HSI16_VALUE_HZ
    b .pll_have_src
.pll_src_hse:
    ldr r0, =HSE_VALUE_HZ

.pll_have_src:
    /* PLLM (bits 6:4): 000=>1 ... 111=>8 */
    lsrs r1, r5, #4
    ands r1, r1, #7
    adds r1, r1, #1
    udiv r0, r0, r1         /* vco_in = src / PLLM */

    /* PLLN (bits 14:8) */
    lsrs r1, r5, #8
    ands r1, r1, #0x7F
    mul r0, r0, r1          /* vco = vco_in * PLLN */

    /* PLLR (bits 26:25): 00=>2,01=>4,10=>6,11=>8 => 2*(code+1) */
    lsrs r1, r5, #25
    ands r1, r1, #3
    adds r1, r1, #1
    lsls r1, r1, #1
    udiv r0, r0, r1         /* pll_r = vco / PLLR */

.pll_done:
    pop {r4-r6, pc}

/* -------------------- Tables -------------------- */
.section .rodata
.align 2

/* HPRE div table indexed by HPRE[3:0] */
hpre_div_table:
    .word 1,1,1,1,1,1,1,1, 2,4,8,16,64,128,256,512

/* PPRE1 div table indexed by PPRE1[2:0] */
ppre_div_table:
    .word 1,1,1,1,2,4,8,16

/* MSI freq table indexed by range (0..11 valid) */
msi_freq_table:
    .word 100000,200000,400000,800000,1000000,2000000,4000000,8000000
    .word 16000000,24000000,32000000,48000000,0,0,0,0
