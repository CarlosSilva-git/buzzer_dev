#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

typedef enum {
  BUZZER_COMMAND_ACCEPTED,
  BUZZER_COMMAND_INVALID_FORMAT,
  BUZZER_COMMAND_OUT_OF_RANGE
} BuzzerCommandResult;

typedef struct {
  uint8_t buzzer_number;
  uint16_t frequency_hz;
  uint16_t duration_seconds;
} BuzzerCommandDetails;

void buzzer_setup(void);
BuzzerCommandResult buzzer_handle_command(const char *command,
                                          BuzzerCommandDetails *details);

#endif