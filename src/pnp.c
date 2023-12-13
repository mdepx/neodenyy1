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

#define	PNP_MAX_X_NM	368000000	/* nanometers */
#define	PNP_MAX_Y_NM	368000000	/* nanometers */
#define	PNP_STEP	6250		/* Length of a step, nanometers */

struct pnp_state {
	uint32_t pos_x;
	uint32_t pos_y;
};

static struct pnp_state pnp;
static mdx_sem_t sem;

void
pnp_pwm_x_intr(void *arg, int irq)
{

	/* Step completed. */

	stm32f4_pwm_intr(arg, irq);

	mdx_sem_post(&sem);
}

static int
pnp_is_x_home(void)
{

	return (pin_get(&gpio_sc, PORT_C, 6));
}

static int
pnp_is_y_home(void)
{

	return (pin_get(&gpio_sc, PORT_C, 7));
}

static void
pnp_xenable(void)
{

	pin_set(&gpio_sc, PORT_D, 14, 1); /* X Vref */
	mdx_usleep(10000);
	pin_set(&gpio_sc, PORT_E, 6, 1); /* X ST */
	mdx_usleep(10000);
}

static void
pnp_xset_direction(int dir)
{

	pin_set(&gpio_sc, PORT_E, 5, dir); /* X FR */
}

static void
xstep(int speed)
{
	uint32_t freq;

	freq = speed * 50000;

	stm32f4_pwm_step(&pwm_x_sc, 1 /* channel */, freq);
}

static int
pnp_xhome(void)
{
	int error;

	pnp_xset_direction(0);
	mdx_sem_init(&sem, 0);

	error = 0;

	while (1) {
		if (!pnp_is_y_home()) {
			error = 1;
			break;
		}
		if (pnp_is_x_home())
			break;
		xstep(3);
		mdx_sem_wait(&sem);
	}

	if (pnp_is_y_home() && pnp_is_x_home())
		printf("We are home\n");
	else
		printf("We are not at home\n");

	return (error);
}

static int
pnp_yhome(void)
{

	if (!pnp_is_y_home())
		return (1);

	return (0);
}

static int
pnp_home(void)
{
	int error;

	error = pnp_yhome();
	if (error)
		return (error);

	error = pnp_xhome();
	if (error)
		return (error);

	pnp.pos_x = 0;
	pnp.pos_y = 0;

	return (0);
}

static void
pnp_move(int abs_x_mm, int abs_y_mm)
{
	uint32_t new_pos_x;
	uint32_t steps;
	uint32_t delta;
	uint32_t left_steps, t;
	int speed;
	int dir;
	int i;

	new_pos_x = abs_x_mm * 1000000; /* nanometers */
	if (new_pos_x > PNP_MAX_X_NM)
		new_pos_x = PNP_MAX_X_NM;
	if (new_pos_x < 0)
		new_pos_x = 0;

	if (new_pos_x == pnp.pos_x)
		return;

	if (new_pos_x > pnp.pos_x) {
		dir = 1;
		delta = new_pos_x - pnp.pos_x;
	} else {
		dir = 0;
		delta = pnp.pos_x - new_pos_x;
	}

	steps = delta / PNP_STEP;
	pnp_xset_direction(dir);

printf("Steps to move %d\n", steps);

	speed = 100;

	for (i = 0; i < steps; i++) {
		left_steps = steps - i;

		speed = 100;

		t = i < left_steps ? i : left_steps;
		if (t < 100) {
			/* Gradually increase/decrease speed */
			t /= 100;
			speed = t < 25 ? 25 : t;
		}

		xstep(speed);
		mdx_sem_wait(&sem);

		if (dir == 1)
			pnp.pos_x += PNP_STEP;
		else {
			if (pnp_is_x_home())
				break;
			pnp.pos_x -= PNP_STEP;
		}
	}

printf("new x pos %d\n", pnp.pos_x);
}

int
pnp_test(void)
{
	int error;

	pnp.pos_x = -1;
	pnp.pos_y = -1;

	pin_set(&gpio_sc, PORT_D, 13, 1); /* Vref */
	pin_set(&gpio_sc, PORT_D, 15, 1); /* Vref */
	pin_set(&gpio_sc, PORT_C,  6, 1); /* Vref */

	pnp_xenable();

	error = pnp_home();
	if (error)
		return (error);

	pnp_move(100, 0);
	mdx_usleep(20000);

	pnp_move(50, 0);
	mdx_usleep(20000);

	pnp_move(150, 0);
	mdx_usleep(20000);

	pnp_move(100, 0);
	mdx_usleep(20000);

	pnp_move(0, 0);
	mdx_usleep(20000);

	pnp_move(360, 0);
	mdx_usleep(20000);

	pnp_move(220, 0);
	mdx_usleep(20000);

	pnp_move(300, 0);
	mdx_usleep(20000);

	pnp_move(0, 0);
	mdx_usleep(20000);

	return (0);
}
