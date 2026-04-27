#include "service/perceptual_brightness.h"

static const uint16_t gamma22_duty_permille[PERCEPTUAL_BRIGHTNESS_MAX_LEVEL + 1U] = {
    0U,   0U,   0U,   1U,   2U,   4U,   5U,   8U,
    10U,  13U,  17U,  21U,  25U,  30U,  35U,  41U,
    47U,  54U,  61U,  69U,  77U,  86U,  95U,  105U,
    116U, 126U, 138U, 150U, 162U, 175U, 189U, 203U,
    218U, 233U, 249U, 265U, 282U, 300U, 318U, 336U,
    356U, 375U, 396U, 417U, 439U, 461U, 484U, 507U,
    531U, 556U, 581U, 607U, 633U, 660U, 688U, 716U,
    745U, 775U, 805U, 836U, 868U, 900U, 933U, 966U,
    1000U,
};

uint16_t PerceptualBrightness_ToDutyPermille(uint8_t visual_level)
{
    if (visual_level > PERCEPTUAL_BRIGHTNESS_MAX_LEVEL)
    {
        visual_level = PERCEPTUAL_BRIGHTNESS_MAX_LEVEL;
    }

    return gamma22_duty_permille[visual_level];
}
