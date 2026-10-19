#include "buzzer.h"
#include "timer.h"
#include "uart.h"

#define UART_COMMAND_BUFFER_SIZE 24

static char command_buffer[UART_COMMAND_BUFFER_SIZE];
static uint8_t command_length;
static uint8_t command_overflow;

static void process_uart_character(char character) {
  if (character == '\r' || character == '\n' || character == '|') {
    if (command_overflow) {
      uart_sendf("[BUZZER] Command too long\r\n");
    } else if (command_length > 0) {
      command_buffer[command_length] = '\0';
      BuzzerCommandDetails details;
      const BuzzerCommandResult result =
          buzzer_handle_command(command_buffer, &details);

      if (result == BUZZER_COMMAND_ACCEPTED) {
        uart_sendf("[BUZZER] B%u: %u Hz square wave for %u s started\r\n",
                   (unsigned int)details.buzzer_number,
                   (unsigned int)details.frequency_hz,
                   (unsigned int)details.duration_seconds);
      } else if (result == BUZZER_COMMAND_OUT_OF_RANGE) {
        uart_sendf("[BUZZER] Range: frequency 1-9999 Hz, duration 1-65535 s\r\n");
      } else {
        uart_sendf("[BUZZER] Invalid format; use B1_300_3\r\n");
      }
    }

    command_length = 0;
    command_overflow = 0;
    return;
  }

  if (!command_overflow) {
    if (command_length < sizeof(command_buffer) - 1) {
      command_buffer[command_length++] = character;
    } else {
      command_overflow = 1;
    }
  }
}

int main(void) {
  uart_setup(9600);
  uart_sendf("[STARTUP] UART ready: 9600 baud, 8-N-1\r\n");

  buzzer_setup();
  uart_sendf("[STARTUP] Buzzers ready: D8-D10 outputs; Timer0 tone tick at 20 kHz\r\n");

  timer_setup();
  uart_sendf("[STARTUP] Timers ready: 1 Hz uptime and independent LED blink\r\n");

  uart_send_help();

  uint32_t last_reported_minute = 0;
  while (1) {
    int16_t received;
    // The UART RX ISR buffers input; commands are parsed one line at a time.
    while ((received = uart_read()) >= 0) {
      process_uart_character((char)received);
    }

    const uint32_t seconds = timer_get_elapsed_seconds();

    const uint32_t elapsed_minutes = seconds / 60UL;
    if (elapsed_minutes != last_reported_minute) {
      last_reported_minute = elapsed_minutes;
      const unsigned int hours = (seconds / 3600UL) % 100UL;
      const unsigned int minutes = (seconds / 60UL) % 60UL;
      const unsigned int seconds_part = seconds % 60UL;
      uart_sendf("[HEARTBEAT] Running for: [HH:MM:SS] %02u:%02u:%02u\r\n", hours, minutes, seconds_part);
    }

  }
}