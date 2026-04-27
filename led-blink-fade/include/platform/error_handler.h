#ifndef PLATFORM_ERROR_HANDLER_H
#define PLATFORM_ERROR_HANDLER_H

#include <stdint.h>

void Error_Handler(void);

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line);
#endif

#endif /* PLATFORM_ERROR_HANDLER_H */
