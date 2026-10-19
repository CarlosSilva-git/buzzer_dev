#include "buzzer.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>
#include <stdint.h>

#define BUZZER_COUNT 3
#define BUZZER_SAMPLE_RATE_HZ 20000UL
#define BUZZER_MAX_FREQUENCY_HZ (BUZZER_SAMPLE_RATE_HZ / 2UL - 1UL)

typedef struct Buzzer Buzzer;
typedef void (*BuzzerPlayMethod)(Buzzer *self, uint16_t frequency_hz,
                                 uint16_t duration_seconds);
typedef void (*BuzzerStopMethod)(Buzzer *self);
typedef void (*BuzzerTickMethod)(Buzzer *self);

struct Buzzer {
  volatile uint8_t *port;
  volatile uint8_t *direction;
  uint8_t pin_mask;
  volatile uint16_t frequency_hz;
  volatile uint16_t phase;
  volatile uint32_t samples_remaining;
  BuzzerPlayMethod play;
  BuzzerStopMethod stop;
  BuzzerTickMethod tick;
};

static void buzzer_play(Buzzer *self, uint16_t frequency_hz,
                        uint16_t duration_seconds);
static void buzzer_stop(Buzzer *self);
static void buzzer_tick(Buzzer *self);

// Uno D8, D9, and D10 (PB0, PB1, and PB2) are used for the three outputs.
static Buzzer buzzers[BUZZER_COUNT] = {
  {.port = &PORTB, .direction = &DDRB, .pin_mask = (1U << PB0)},
  {.port = &PORTB, .direction = &DDRB, .pin_mask = (1U << PB1)},
  {.port = &PORTB, .direction = &DDRB, .pin_mask = (1U << PB2)},
};

// Parse a decimal number from the string at *cursor, advancing the cursor
static uint8_t parse_uint16(const char **cursor, uint16_t *result) {
  uint32_t value = 0;
  const char *position = *cursor;

  if (*position < '0' || *position > '9') {
    return 0;
  }

  do {
    const uint8_t digit = (uint8_t)(*position - '0');
    value = value * 10UL + digit;
    if (value > 65535UL) {
      return 0;
    }
    position++;
  } while (*position >= '0' && *position <= '9');

  *cursor = position;
  *result = (uint16_t)value;
  return 1;
}

static void buzzer_play(Buzzer *self, uint16_t frequency_hz,
                        uint16_t duration_seconds) {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    *self->port &= (uint8_t)~self->pin_mask;
    self->frequency_hz = frequency_hz;
    self->phase = 0;
    self->samples_remaining =
        (uint32_t)duration_seconds * BUZZER_SAMPLE_RATE_HZ;
  }
}

static void buzzer_stop(Buzzer *self) {
  self->samples_remaining = 0;
  self->frequency_hz = 0;
  self->phase = 0;
  *self->port &= (uint8_t)~self->pin_mask;
}

static void buzzer_tick(Buzzer *self) {
  if (self->samples_remaining == 0) {
    return;
  }

  if (--self->samples_remaining == 0) {
    self->stop(self);
    return;
  }

  // Phase accumulation permits non-integer timer periods, such as 300 Hz.
  uint16_t phase = self->phase + (uint16_t)(2U * self->frequency_hz);
  if (phase >= BUZZER_SAMPLE_RATE_HZ) {
    phase -= BUZZER_SAMPLE_RATE_HZ;
    *self->port ^= self->pin_mask;
  }
  self->phase = phase;
}

ISR(TIMER0_COMPA_vect) {
  for (uint8_t index = 0; index < BUZZER_COUNT; index++) {
    buzzers[index].tick(&buzzers[index]);
  }
}

void buzzer_setup(void) {
  for (uint8_t index = 0; index < BUZZER_COUNT; index++) {
    Buzzer *self = &buzzers[index];
    *self->direction |= self->pin_mask;
    *self->port &= (uint8_t)~self->pin_mask;
    self->frequency_hz = 0;
    self->phase = 0;
    self->samples_remaining = 0;
    self->play = buzzer_play;
    self->stop = buzzer_stop;
    self->tick = buzzer_tick;
  }

  // Timer0 CTC: 16 MHz / 8 / (99 + 1) = a 20 kHz buzzer sample tick.
  // Disable compare outputs and clear waveform-generation/prescaler bits.
  TCCR0A &= (uint8_t)~((1U << COM0A1) | (1U << COM0A0) | (1U << COM0B1) |
                       (1U << COM0B0) | (1U << WGM01) | (1U << WGM00));
  TCCR0B &= (uint8_t)~((1U << WGM02) | (1U << CS02) | (1U << CS01) |
                       (1U << CS00));
  TCNT0 = 0;
  OCR0A = 99;
  TIFR0 = (1U << OCF0A);
  TIMSK0 &= (uint8_t)~(1U << OCIE0A);
  TIMSK0 |= (1U << OCIE0A);

  // WGM01 selects CTC mode; CS01 selects the 8 prescaler.
  TCCR0A |= (1U << WGM01);
  TCCR0B |= (1U << CS01);
}

BuzzerCommandResult buzzer_handle_command(const char *command,
                                          BuzzerCommandDetails *details) {
  if (command == 0 || command[0] != 'B' || command[1] < '1' ||
      command[1] > '3' || command[2] != '_') {
    return BUZZER_COMMAND_INVALID_FORMAT;
  }

  const uint8_t buzzer_index = (uint8_t)(command[1] - '1');
  const char *cursor = command + 3;
  uint16_t frequency_hz;
  uint16_t duration_seconds;

  if (!parse_uint16(&cursor, &frequency_hz) || *cursor++ != '_' ||
      !parse_uint16(&cursor, &duration_seconds) || *cursor != '\0') {
    return BUZZER_COMMAND_INVALID_FORMAT;
  }

  if (frequency_hz == 0 || frequency_hz > BUZZER_MAX_FREQUENCY_HZ ||
      duration_seconds == 0) {
    return BUZZER_COMMAND_OUT_OF_RANGE;
  }

  if (details != 0) {
    details->buzzer_number = buzzer_index + 1;
    details->frequency_hz = frequency_hz;
    details->duration_seconds = duration_seconds;
  }

  buzzers[buzzer_index].play(&buzzers[buzzer_index], frequency_hz,
                             duration_seconds);
  return BUZZER_COMMAND_ACCEPTED;
}