#ifndef UART_DEBUG_H
#define UART_DEBUG_H

#include <stdint.h>

void uart_debug_init(uint32_t baud);
void uart_debug_putc(char c);
void uart_debug_write(const char *text);
void uart_debug_write_u32(uint32_t value);

#endif
