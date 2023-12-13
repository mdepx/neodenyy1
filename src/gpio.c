/*-
 * Copyright (c) 2023 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <arm/stm/stm32f4_gpio.h>

#include "gpio.h"

static const struct gpio_pin neodenyy1_pins[] = {
	{ PORT_A, 9, MODE_ALT, 7, PULLUP }, /* USART1_TX */
	{ PORT_A, 10, MODE_ALT, 7, PULLUP }, /* USART1_RX */

	/* Placement Head. */
	{ PORT_E, 2, MODE_OUT, 0, PULLDOWN }, /* Air 1 */
	{ PORT_E, 1, MODE_OUT, 0, PULLDOWN }, /* Air 2 */
	{ PORT_E, 0, MODE_OUT, 0, PULLDOWN }, /* N */

	/* Needle Set */
	{ PORT_B, 5, MODE_INP, 0, PULLDOWN }, /* NS */

	/* At least one head is at Z. */
	{ PORT_B, 4, MODE_INP, 0, PULLDOWN }, /* SZ */

	/* Head 1 has component. */
	{ PORT_B, 3, MODE_INP, 0, PULLDOWN }, /* S1 */

	/* Head 2 has component. */
	{ PORT_D, 4, MODE_INP, 0, PULLDOWN }, /* S2 */

	{ PORT_A, 3, MODE_OUT, 0, PULLDOWN }, /* Vibrator */
	{ PORT_B, 11, MODE_OUT, 0, PULLDOWN }, /* PeelR */
	{ PORT_B, 12, MODE_OUT, 0, PULLDOWN }, /* PeelL */
	{ PORT_B, 13, MODE_OUT, 0, PULLDOWN }, /* Pump */

	/* Sensors are logic 1 (home) or 0. */
	{ PORT_C, 1, MODE_INP, 0, PULLDOWN }, /* SensorR */
	{ PORT_C, 6, MODE_INP, 0, PULLDOWN }, /* SensorL head */
	{ PORT_C, 7, MODE_INP, 0, PULLDOWN }, /* SensorL */

	/* TODO: Y Motor R */
	{ PORT_C,  0, MODE_OUT, 0, PULLDOWN }, /* ST */
	{ PORT_C, 13, MODE_OUT, 0, PULLDOWN }, /* FR */
	{ PORT_B,  7, MODE_ALT, 2, PULLDOWN }, /* STP TIM4_CH2 */

	/* TODO: Y Motor L */
	{ PORT_A, 8, MODE_OUT, 0, PULLDOWN }, /* ST */
	{ PORT_C, 9, MODE_OUT, 0, PULLDOWN }, /* FR */
	{ PORT_B, 6, MODE_ALT, 2, PULLDOWN }, /* STP TIM4_CH1 */

	/* X Motor */
	{ PORT_E,  6, MODE_OUT, 0, PULLDOWN }, /* ST */
	{ PORT_E,  5, MODE_OUT, 0, PULLDOWN }, /* FR */
	{ PORT_B,  8, MODE_ALT, 3, PULLDOWN }, /* STP TIM10_CH1 */
	{ PORT_D, 14, MODE_OUT, 0, PULLDOWN }, /* X Motor VREF */

	{ PORT_D, 13, MODE_OUT, 0, PULLDOWN }, /* Motor VREF */
	{ PORT_D, 15, MODE_OUT, 0, PULLDOWN }, /* Motor VREF */
	{ PORT_C, 6, MODE_OUT, 0, PULLDOWN }, /* Motor VREF */

	/* Z Motor */
	{ PORT_E, 4, MODE_OUT, 0, PULLDOWN }, /* ST */
	{ PORT_E, 3, MODE_OUT, 0, PULLDOWN }, /* FR */
	{ PORT_A, 7, MODE_ALT, 9, PULLDOWN }, /* STP TIM14_CH1 */

	/* Head 1 */
	{ PORT_D, 3, MODE_OUT, 0, PULLDOWN }, /* ST */
	{ PORT_D, 1, MODE_OUT, 0, PULLDOWN }, /* FR */
	{ PORT_A, 6, MODE_ALT, 9, PULLDOWN }, /* STP TIM13_CH1 */

	/* Head 2 */
	{ PORT_A, 15, MODE_OUT, 0, PULLDOWN }, /* ST */
	{ PORT_D, 0, MODE_OUT, 0, PULLDOWN }, /* FR */
	{ PORT_B, 14, MODE_ALT, 9, PULLDOWN }, /* STP TIM12_CH1 */

	{ -1, -1, -1, -1, -1 },
};

void
gpio_config(struct stm32f4_gpio_softc *sc)
{

	pin_configure(sc, neodenyy1_pins);
}
