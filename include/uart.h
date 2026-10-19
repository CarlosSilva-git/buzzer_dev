#ifndef UART_H
#define UART_H

#include <stdint.h>

// Configure USART0 for normal-speed, 8-N-1 operation at baudrate.
void uart_setup(uint32_t baudrate);

// Print the startup command and hardware help text.
void uart_send_help(void);

// Send one byte, or a printf-formatted string (truncated to the internal buffer).
void uart_send_char(char value);
void uart_sendf(const char *format, ...);

// Read one buffered byte, or return -1 when the RX ring buffer is empty.
int16_t uart_read(void);

#endif