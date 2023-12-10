/*-
 * Copyright (c) 2018-2020 Ruslan Bukin <br@bsdpad.com>
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
#include <sys/console.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <dev/display/panel.h>
#include <dev/display/dsi.h>
#include <arm/stm/stm32f4.h>

#include "board.h"

extern struct stm32f4_gpio_softc gpio_sc;

int
main(void)
{
	int i;

	printf("MDEPX started\n");

	printf("Sleeping 1 sec\n");
	for (i = 0; i < 10; i++) {
		mdx_usleep(100000);
		printf(".");
	}
	printf("Sleeping 1 sec done\n");

	pin_set(&gpio_sc, PORT_E, 6, 0); /* X ST */
	pin_set(&gpio_sc, PORT_E, 5, 0); /* X FR */
	pin_set(&gpio_sc, PORT_B, 8, 0); /* X STP */

	while (1)
		mdx_usleep(100000);

	for (i = 0; i < 10000; i++) {
		udelay(1);
		pin_set(&gpio_sc, PORT_B, 8, 1); /* X STP */
		udelay(1);
		pin_set(&gpio_sc, PORT_B, 8, 0); /* X STP */
	}

	//{ PORT_B,  8, MODE_ALT, 3, FLOAT }, /* STP TIM10_CH1 */

	while (1)
		mdx_usleep(100000);

	return (0);
}
