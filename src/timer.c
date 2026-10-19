#include "timer.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>

static volatile uint32_t elapsed_seconds;
static uint8_t blink_millisecond_count;

ISR(TIMER1_COMPA_vect) {
  elapsed_seconds++;
}

ISR(TIMER2_COMPA_vect) {
  if (++blink_millisecond_count >= 167) {
    blink_millisecond_count = 0;
    PORTB ^= (1U << PORTB5);
  }
}

static void timer1_setup(void) {
  // Disconnect compare outputs and clear the waveform-generation bits.
  TCCR1A &= (uint8_t)~((1U << COM1A1) | (1U << COM1A0) | (1U << COM1B1) |
                       (1U << COM1B0) | (1U << WGM11) | (1U << WGM10));
  TCCR1B &= (uint8_t)~((1U << WGM13) | (1U << WGM12) | (1U << CS12) |
                       (1U << CS11) | (1U << CS10));
  TCNT1 = 0;
  OCR1A = (uint16_t)(F_CPU / 1024UL - 1UL);
  TIFR1 = (1U << OCF1A);
  TIMSK1 &= (uint8_t)~(1U << OCIE1A);
  TIMSK1 |= (1U << OCIE1A);

  // WGM12 selects CTC mode; CS12 and CS10 select the 1024 prescaler.
  TCCR1B |= (1U << WGM12) | (1U << CS12) | (1U << CS10);
}

static void timer2_setup(void) {
  // Disconnect compare outputs and clear waveform-generation settings.
  TCCR2A &= (uint8_t)~((1U << COM2A1) | (1U << COM2A0) | (1U << COM2B1) |
                       (1U << COM2B0) | (1U << WGM21) | (1U << WGM20));
  TCCR2B &= (uint8_t)~((1U << WGM22) | (1U << CS22) | (1U << CS21) |
                       (1U << CS20));
  TCNT2 = 0;
  OCR2A = 249;
  TIFR2 = (1U << OCF2A);
  TIMSK2 &= (uint8_t)~(1U << OCIE2A);
  TIMSK2 |= (1U << OCIE2A);

  // WGM21 selects CTC mode; CS22 selects the 64 prescaler.
  TCCR2A |= (1U << WGM21);
  TCCR2B |= (1U << CS22);
}

void timer_setup(void) {
  // PB5 is the Uno's onboard LED; initialize it off before enabling its timer.
  DDRB |= (1U << DDB5);
  PORTB &= (uint8_t)~(1U << PORTB5);

  timer1_setup();
  timer2_setup();
}

uint32_t timer_get_elapsed_seconds(void) {
  uint32_t seconds;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    seconds = elapsed_seconds;
  }
  return seconds;
}