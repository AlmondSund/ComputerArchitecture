#ifndef BSP_LED_PWM_H
#define BSP_LED_PWM_H

#include <stdint.h>

#define BSP_LED_PWM_DUTY_SCALE 1000U
#define BSP_LED_PWM_FREQUENCY_HZ 1000U

typedef enum
{
    BSP_LED_PWM_OK = 0,
    BSP_LED_PWM_ERROR
} BSP_LED_PWM_Status;

BSP_LED_PWM_Status BSP_LED_PWM_Init(void);
void BSP_LED_PWM_SetDutyPermille(uint16_t duty_permille);
uint16_t BSP_LED_PWM_GetDutyPermille(void);

#endif /* BSP_LED_PWM_H */
