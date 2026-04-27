#ifndef SERVICE_PERCEPTUAL_BRIGHTNESS_H
#define SERVICE_PERCEPTUAL_BRIGHTNESS_H

#include <stdint.h>

#define PERCEPTUAL_BRIGHTNESS_MAX_LEVEL 64U

uint16_t PerceptualBrightness_ToDutyPermille(uint8_t visual_level);

#endif /* SERVICE_PERCEPTUAL_BRIGHTNESS_H */
