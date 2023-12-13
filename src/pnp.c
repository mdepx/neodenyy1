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
#include <sys/console.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sem.h>

#include <dev/display/panel.h>
#include <dev/display/dsi.h>
#include <arm/stm/stm32f4.h>

#include "board.h"
#include "pnp.h"

extern struct stm32f4_gpio_softc gpio_sc;
extern struct stm32f4_pwm_softc pwm_x_sc;

mdx_sem_t sem;

struct motor {
	int pos_x;
	int pos_y;
};

void
pnp_pwm_x_intr(void *arg, int irq)
{

	/* Step completed. */

	stm32f4_pwm_intr(arg, irq);

	mdx_sem_post(&sem);
}

#if 0
int
is_at_home(void)
{

	return (pin_get(&gpio_sc, PORT_C, 6));
}

void
move_to_home(void)
{

	stm32f4_pwm_setup(&pwm_x_sc);
}
#endif

static void
pnp_xenable(void)
{

	pin_set(&gpio_sc, PORT_D, 14, 1); /* X Vref */
	mdx_usleep(100);
	pin_set(&gpio_sc, PORT_E, 6, 1); /* X ST */
}

static void
pnp_xset_direction(int dir)
{

	pin_set(&gpio_sc, PORT_E, 5, dir); /* X FR */
}

static void
xstep(void)
{

	stm32f4_pwm_step(&pwm_x_sc, 1 /* channel */);
}

void
pnp_xtest(void)
{
	int i;

	pnp_xenable();
	pnp_xset_direction(1);
	mdx_sem_init(&sem, 0);

	for (i = 0; i < 6400; i++) {
		xstep();
		mdx_sem_wait(&sem);
	}
}
