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
	{ PORT_A,  9, MODE_ALT, 7, FLOAT }, /* USART1_TX */
	{ PORT_A, 10, MODE_ALT, 7, FLOAT }, /* USART1_RX */

	/* Placement Head. */
	{ PORT_E, 2, MODE_OUT, 0, FLOAT }, /* Air 1 */
	{ PORT_E, 1, MODE_OUT, 0, FLOAT }, /* Air 2 */
	{ PORT_E, 0, MODE_OUT, 0, FLOAT }, /* N */

	/* Needle Set */
	{ PORT_B, 5, MODE_INP, 0, FLOAT }, /* NS */

	/* At least one head is at Z. */
	{ PORT_B, 4, MODE_INP, 0, FLOAT }, /* SZ */

	/* Head 1 has component. */
	{ PORT_B, 3, MODE_INP, 0, FLOAT }, /* S1 */

	/* Head 2 has component. */
	{ PORT_D, 4, MODE_INP, 0, FLOAT }, /* S2 */

	{ PORT_A,  3, MODE_OUT, 0, FLOAT }, /* Vibrator */
	{ PORT_B, 11, MODE_OUT, 0, FLOAT }, /* PeelR */
	{ PORT_B, 12, MODE_OUT, 0, FLOAT }, /* PeelL */
	{ PORT_B, 13, MODE_OUT, 0, FLOAT }, /* Pump */

	/* Sensors are logic 1 (home) or 0. */
	{ PORT_C,  1, MODE_INP, 0, FLOAT }, /* SensorR */
	{ PORT_C,  6, MODE_INP, 0, FLOAT }, /* SensorL head */
	{ PORT_C,  7, MODE_INP, 0, FLOAT }, /* SensorL */
	{ -1, -1, -1, -1, -1 },
};

void
gpio_config(struct stm32f4_gpio_softc *sc)
{

	pin_configure(sc, neodenyy1_pins);
}
