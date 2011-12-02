/*
 * Flickering LED Candle
 * (C) 2011 Akafugu Corporation
 *
 * This program is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 */

/*
 * Pinout:
 * PB0 (pin 5) - LED
 * PB1 (pin 6) - LED
 * PB2 (pin 7) - LED
 * PB3 (pin 2) - LED
 * PB4 (pin 3) - NC
 * PB5 (pin 1) - Reset
 *
 */

#define LED_PORT PORTB
#define LED_DDR  DDRB
#define LED_BIT PB0

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>

#define DELAY 100

// for counting seconds
volatile uint8_t int_counter;

// for tracking which pattern to show
volatile uint8_t pattern;
#define MAX_PATTERN 6

void (*function_list[MAX_PATTERN+1])(void);

// random number seed (will give same flicker sequence each time)
//volatile uint32_t lfsr = 0xbeefcace;
volatile uint32_t lfsr = 0xbeefdead;

uint32_t rand(void)
{
	// http://en.wikipedia.org/wiki/Linear_feedback_shift_register
	// Galois LFSR: taps: 32 31 29 1; characteristic polynomial: x^32 + x^31 + x^29 + x + 1 */
  	lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xD0000001u);
	return lfsr;
}


///
/// HELPER FUNCTIONS
///

// turn a specific led off
void led_off(uint16_t i)
{
	uint8_t idx = i % 4;
	
	if (idx == 0)
		PORTB &= ~(1 << PORTB1);
	else if (idx == 1)
		PORTB &= ~(1 << PORTB0);
	else if (idx == 2)
		PORTB &= ~(1 << PORTB2);
	else
		PORTB &= ~(1 << PORTB3);
}

// turn a specific led on
void led_on(uint16_t i)
{
	uint8_t idx = i % 4;
	
	if (idx == 0)
		PORTB |= (1 << PORTB1);
	else if (idx == 1)
		PORTB |= (1 << PORTB0);
	else if (idx == 2)
		PORTB |= (1 << PORTB2);
	else
		PORTB |= (1 << PORTB3);
}

// rotate leds clockwise
void led_rotate_cw(uint8_t i, uint8_t reset, uint8_t inverse)
{
	uint8_t port = i % 4;

	if (reset) PORTB = 0;
	if (inverse) led_off(port);
	else led_on(port);
}

// rotate leds counterclockwise
void led_rotate_ccw(uint8_t i, uint8_t reset, uint8_t inverse)
{
	uint8_t port = i % 4;

	if (reset) PORTB = 0;
	if (inverse) led_off(port);
	else led_on(port);
}

///
/// PATTERNS
///

// blink all with shrinking delay
void blink_all(void)
{
	static uint8_t direction = 0;
	static uint8_t delays = 255;
	
	if (delays % 2 == 0)
		PORTB = 0b1111;
	else
		PORTB = 0;
	
	for (uint8_t i = 0; i < delays; i++)
		_delay_ms(2);
	
	if (direction) {
		delays+=5;
		if (delays >= 250) direction = 0;
	}
	else {
		delays-=5;
		if (delays <= 5) direction = 1;
	}
}

void rotate_cw(void)
{
	static uint16_t counter = 0;

	led_rotate_cw(counter++, true, false);
	_delay_ms(DELAY);
}

void rotate_ccw(void)
{
	static uint16_t counter = 0xFFFF;

	led_rotate_ccw(counter--, true, false);
	_delay_ms(DELAY);
}

void chase(void)
{
	PORTB = 0;
	_delay_ms(DELAY);
	
	for (uint8_t i = 0; i < 4; i++) {
		led_rotate_cw(i, false, false);
		_delay_ms(DELAY);
	}
	
	_delay_ms(DELAY);

	for (uint8_t i = 4; i > 0; i--) {
		led_rotate_ccw(i-1, false, true);
		_delay_ms(DELAY);
	}
}

void chase_reverse(void)
{
	PORTB = 0;
	_delay_ms(DELAY);
	
	for (uint8_t i = 4; i > 0; i--) {
		led_rotate_ccw(i-1, false, false);
		_delay_ms(DELAY);
	}
	
	_delay_ms(DELAY);

	for (uint8_t i = 0; i < 4; i++) {
		led_rotate_ccw(i, false, true);
		_delay_ms(DELAY);
	}
}

void blink_staggered(void)
{
	static uint8_t i = 0;
	
	if (i == 0) {
		led_off(0);
		led_on(1);
		led_off(2);
		led_on(3);
		i = 1;
	}
	else {
		led_on(0);
		led_off(1);
		led_on(2);
		led_off(3);
		i = 0;
	}
	
	_delay_ms(DELAY);
}

ISR(TIM0_OVF_vect)
{
	if (++int_counter == 0xFF) {
		pattern = (rand()>>24) % MAX_PATTERN;
	}
}

void main(void) __attribute__ ((noreturn));

void main(void)
{
	// Inititalize timer
	TCCR0B = (1<<CS01); // Set Prescaler to clk/8 : 1 click = aprox 2us (using 4.6MHz internal clock)
	TIMSK0 |= (1<<TOIE0); // Enable Overflow Interrupt Enable
	TCNT0 = 0; // Initialize counter

	DDRB |= (1 << 0)|(1 << 1)|(1 << 2)|(1 << 3);

	PORTB = 0;

	function_list[0] = &rotate_cw;
	function_list[1] = &rotate_ccw;
	function_list[2] = &chase;
	function_list[3] = &chase_reverse;
	function_list[4] = &blink_staggered;
	function_list[5] = &blink_all;

	sei();

	while(1) {
		function_list[pattern]();
	}
}


