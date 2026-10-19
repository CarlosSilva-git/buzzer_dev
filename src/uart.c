#include "uart.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdarg.h>
#include <stdio.h>

#define UART_RX_BUFFER_SIZE 64
#define UART_RX_BUFFER_MASK (UART_RX_BUFFER_SIZE - 1)
#define UART_TX_BUFFER_SIZE 96

// The power-of-two size allows wrapping with a mask. One slot stays unused to
// distinguish a full ring from an empty one, so the usable capacity is 63 bytes.
// These indices are shared by the RX interrupt and foreground code.
static volatile uint8_t rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint8_t rx_head;
static volatile uint8_t rx_tail;

static const char startup_help[] PROGMEM =
  "\r\nAVR UART Buzzer Controller\r\n"
  "UART: 9600 baud, 8-N-1\r\n"
  "Command: B<buzzer>_<frequency Hz>_<duration seconds>\r\n"
  "Example: B1_300_3 plays buzzer 1 at 300 Hz for 3 seconds.\r\n"
  "Buzzer numbers: 1-3 (Uno pins D8, D9, D10).\r\n"
  "Frequency range: 1-9999 Hz; duration range: 1-65535 seconds.\r\n"
  "End each command with |, CR, or LF. Pipes can chain commands:\r\n"
  "B1_300_3|B2_440_3|B3_600_3|\r\n"
  "A new command replaces the active tone on that buzzer.\r\n"
  "Tones stop automatically at the requested duration.\r\n"
  "Responses report accepted, invalid, out-of-range, or oversized commands.\r\n"
  "Uptime heartbeat is sent once per minute as HH:MM:SS.\r\n"
  "The onboard LED blinks as a health signal.\r\n\r\n";

ISR(USART_RX_vect) {
  const uint8_t received = UDR0;
  const uint8_t next_head = (rx_head + 1) & UART_RX_BUFFER_MASK;

  // Leave the oldest unread bytes intact if the consumer has not caught up.
  if (next_head != rx_tail) {
    rx_buffer[rx_head] = received;
    rx_head = next_head;
  }
}

void uart_setup(uint32_t baudrate) {
  // Keep the current UART configuration if the requested rate is invalid or
  // cannot be represented by the ATmega328P's 12-bit baud divisor.
  if (baudrate == 0 || baudrate > F_CPU / 16UL) {
    return;
  }

  const uint32_t divisor = F_CPU / (16UL * baudrate) - 1UL;
  if (divisor > 4095) {
    return;
  }

  // Clear double-speed and multiprocessor modes for normal-speed operation.
  UCSR0A &= (uint8_t)~((1U << U2X0) | (1U << MPCM0));

  UBRR0H = (uint8_t)(divisor >> 8);
  UBRR0L = (uint8_t)divisor;

  // Select asynchronous mode, no parity, one stop bit, and 8 data bits.
  UCSR0C &= (uint8_t)~((1U << UMSEL01) | (1U << UMSEL00) | (1U << UPM01) |
                       (1U << UPM00) | (1U << USBS0) | (1U << UCSZ01) |
                       (1U << UCSZ00));
  UCSR0B &= (uint8_t)~((1U << RXEN0) | (1U << TXEN0) | (1U << RXCIE0) |
                       (1U << UCSZ02));
  UCSR0C |= (1U << UCSZ01) | (1U << UCSZ00);

  // Enable the receiver, transmitter, and receive-complete interrupt.
  UCSR0B |= (1U << RXEN0) | (1U << TXEN0) | (1U << RXCIE0);
  // The RX interrupt needs the global interrupt flag as well as RXCIE0.
  sei();
}

void uart_send_char(char value) {
  // Wait until the transmit data register can accept the next byte.
  while (!(UCSR0A & _BV(UDRE0))) {
  }
  UDR0 = (uint8_t)value;
}

static void uart_send_flash_string(const char *text) {
  char value;
  while ((value = (char)pgm_read_byte(text++)) != '\0') {
    uart_send_char(value);
  }
}

void uart_send_help(void) {
  uart_send_flash_string(startup_help);
}

void uart_sendf(const char *format, ...) {
  char buffer[UART_TX_BUFFER_SIZE];
  va_list args;
  va_start(args, format);
  const int length = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (length <= 0) {
    return;
  }

  // vsnprintf reports the untruncated length; send only bytes that fit here.
  const uint8_t bytes_to_send =
      length < (int)sizeof(buffer) ? (uint8_t)length : sizeof(buffer) - 1;

  for (uint8_t index = 0; index < bytes_to_send; index++) {
    uart_send_char(buffer[index]);
  }
}

int16_t uart_read(void) {
  // Return -1 when no byte is queued; otherwise consume the oldest byte.
  if (rx_head == rx_tail) {
    return -1;
  }

  const uint8_t value = rx_buffer[rx_tail];
  rx_tail = (rx_tail + 1) & UART_RX_BUFFER_MASK;
  return value;
}