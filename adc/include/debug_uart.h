#ifndef DEBUG_UART_H
#define DEBUG_UART_H

#include <stdint.h>

void debug_uart_init(uint32_t baud);
void debug_uart_write(const char *text);
void debug_uart_write_u32(uint32_t value);
void debug_uart_write_i32(int32_t value);
void debug_uart_putc(char c);

#endif
