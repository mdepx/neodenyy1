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
extern struct stm32f4_pwm_softc pwm_y_sc;
extern struct stm32f4_rng_softc rng_sc;

#define	PNP_MAX_X_NM	300000000	/* nanometers */
#define	PNP_MAX_Y_NM	300000000	/* nanometers */
#define	PNP_STEP_NM	6250		/* Length of a step, nanometers */
#define	PNP_STEPS_PER_MM	160

struct pnp_state {
	uint32_t pos_x;
	uint32_t pos_y;
};

static struct pnp_state pnp;
static mdx_sem_t xsem;
static mdx_sem_t ysem;

void
pnp_pwm_y_intr(void *arg, int irq)
{

	/* Step completed. */

	stm32f4_pwm_intr(arg, irq);

	mdx_sem_post(&ysem);
}

void
pnp_pwm_x_intr(void *arg, int irq)
{

	/* Step completed. */

	stm32f4_pwm_intr(arg, irq);

	mdx_sem_post(&xsem);
}

static int
pnp_is_x_home(void)
{

	return (pin_get(&gpio_sc, PORT_C, 6));
}

static int
pnp_is_yl_home(void)
{

	return (pin_get(&gpio_sc, PORT_C, 7));
}

#if 0
static int
pnp_is_yr_home(void)
{

	return (pin_get(&gpio_sc, PORT_C, 1));
}
#endif

static void
pnp_xenable(void)
{

	pin_set(&gpio_sc, PORT_D, 14, 1); /* X Vref */
	mdx_usleep(10000);
	pin_set(&gpio_sc, PORT_E, 6, 1); /* X ST */
	mdx_usleep(10000);
}

static void
pnp_yenable(void)
{

	pin_set(&gpio_sc, PORT_D, 13, 1); /* Vref */
	mdx_usleep(10000);
	pin_set(&gpio_sc, PORT_C, 0, 1); /* Y R ST */
	pin_set(&gpio_sc, PORT_A, 8, 1); /* Y L ST */
	mdx_usleep(10000);
}

static void
pnp_xset_direction(int dir)
{

	pin_set(&gpio_sc, PORT_E, 5, dir); /* X FR */
}

static void
pnp_yset_direction(int dir)
{
	int rdir;

	rdir = dir ? 0 : 1;

	pin_set(&gpio_sc, PORT_C, 13, rdir); /* Y R FR */
	pin_set(&gpio_sc, PORT_C, 9, dir); /* Y L FR */
}

static void
xstep(int speed)
{
	uint32_t freq;

	freq = speed * 50000;

	stm32f4_pwm_step(&pwm_x_sc, (1 << 0), freq);
}

#define	YLEFT	(1 << 0)
#define	YRIGHT	(1 << 1)
#define	YBOTH	(YLEFT | YRIGHT)

static void
ystep(int chanset, int speed)
{
	uint32_t freq;

	freq = speed * 50000;
	//chanset = (1 << 0) | (1 << 1);

	stm32f4_pwm_step(&pwm_y_sc, chanset, freq);
}

static int
pnp_xhome(void)
{
	int error;

	pnp_xset_direction(0);
	mdx_sem_init(&xsem, 0);

	error = 0;

	while (1) {
		if (!pnp_is_yl_home()) {
			error = 1;
			break;
		}
		if (pnp_is_x_home())
			break;
		xstep(25);
		mdx_sem_wait(&xsem);
	}

	if (pnp_is_yl_home() && pnp_is_x_home())
		printf("We are home\n");
	else
		printf("We are not at home\n");

	return (error);
}

static int
pnp_yhome(void)
{
	int count;
	int speed;
	int i;

	speed = 25;

	mdx_sem_init(&ysem, 0);

	if (pnp_is_yl_home()) {
		/* Already at home, move back a bit. */
		pnp_yset_direction(1);
		for (i = 0; i < (PNP_STEPS_PER_MM * 10); i++) {
			ystep(YBOTH, speed);
			mdx_sem_wait(&ysem);
		}
	}

	if (pnp_is_yl_home())
		panic("still at home");

	/* Now try to reach home. */
	pnp_yset_direction(0);

	count = -1;

	while (1) {
#if 0
		if (pnp_is_yl_home() && pnp_is_yr_home())
			break;
		else if (!pnp_is_yl_home() && pnp_is_yr_home())
			ystep(YLEFT, 30);
		else if (pnp_is_yl_home() && !pnp_is_yr_home())
			ystep(YRIGHT, 30);
		else if (!pnp_is_yl_home() && !pnp_is_yr_home())
			ystep(YBOTH, 60);
#endif

		if (pnp_is_yl_home() && count == -1) {
			/*
			 * We have reached home for the first time.
			 * But we need to move a mm or so into home.
			 */
			count = PNP_STEPS_PER_MM;
			speed = 15;
		}

		if (count > 0)
			count--;

		if (count == 0)
			break;

		ystep(YBOTH, speed);
		mdx_sem_wait(&ysem);
	}

	printf("y compl\n");

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
pnp_move(uint32_t new_pos_x, uint32_t new_pos_y)
{
	uint32_t xsteps, ysteps;
	uint32_t xdelta, ydelta;
	uint32_t t;
	int xcount, ycount;
	int xspeed, yspeed;
	int xdir, ydir;
	int xstop, ystop;

	xcount = ycount = 0;
	xstop = ystop = 0;

	if (new_pos_x > PNP_MAX_X_NM)
		new_pos_x = PNP_MAX_X_NM;
	if (new_pos_x < 0)
		new_pos_x = 0;

	if (new_pos_y > PNP_MAX_Y_NM)
		new_pos_y = PNP_MAX_Y_NM;
	if (new_pos_y < 0)
		new_pos_y = 0;

	if (new_pos_x == pnp.pos_x)
		xstop = 1;
	if (new_pos_y == pnp.pos_y)
		ystop = 1;

	if (new_pos_x > pnp.pos_x) {
		xdir = 1;
		xdelta = new_pos_x - pnp.pos_x;
	} else {
		xdir = 0;
		xdelta = pnp.pos_x - new_pos_x;
	}

	if (new_pos_y > pnp.pos_y) {
		ydir = 1;
		ydelta = new_pos_y - pnp.pos_y;
	} else {
		ydir = 0;
		ydelta = pnp.pos_y - new_pos_y;
	}

	xsteps = xdelta / PNP_STEP_NM;
	ysteps = ydelta / PNP_STEP_NM;
	pnp_xset_direction(xdir);
	pnp_yset_direction(ydir);

	//printf("Steps to move %d %d\n", xsteps, ysteps);

	while (1) {
		xspeed = yspeed = 100;

		t = xcount < (xsteps - xcount) ? xcount : (xsteps - xcount);
		if (t < 1000) {
			/* Gradually increase/decrease speed */
			xspeed = (t * 100) / 1000;
			if (xspeed < 15)
				xspeed = 15;
		}

		t = ycount < (ysteps - ycount) ? ycount : (ysteps - ycount);
		if (t < 1000) {
			/* Gradually increase/decrease speed */
			yspeed = (t * 100) / 1000;
			if (yspeed < 15)
				yspeed = 15;
		}

	//printf("%d %d\n", xspeed, yspeed);

		if (xstop && ystop)
			break;
		if (xstop == 0)
			xstep(xspeed);
		if (ystop == 0)
			ystep(YBOTH, yspeed);

		if (xstop == 0) {
			mdx_sem_wait(&xsem);
			if (xcount++ == (xsteps - 1))
				xstop = 1;
			if (xdir == 1)
				pnp.pos_x += PNP_STEP_NM;
			else
				pnp.pos_x -= PNP_STEP_NM;
		}

		if (ystop == 0) {
			mdx_sem_wait(&ysem);
			if (ycount++ == (ysteps - 1))
				ystop = 1;
			if (ydir == 1)
				pnp.pos_y += PNP_STEP_NM;
			else
				pnp.pos_y -= PNP_STEP_NM;
		}
	}

	//printf("new x y %d %d\n", pnp.pos_x, pnp.pos_y);
	//printf("count x y %d %d\n", xcount, ycount);
}

static uint32_t
get_random(void)
{
	uint32_t data;
	int error;

	do {
		error = stm32f4_rng_data(&rng_sc, &data);
		if (error == 0)
			break;
	} while (1);

	return (data);
}

int
pnp_test(void)
{
	uint32_t new_x;
	uint32_t new_y;
	int error;
	int i;

	pnp.pos_x = -1;
	pnp.pos_y = -1;

#if 1
	/* TODO */
	pin_set(&gpio_sc, PORT_D, 15, 1); /* Vref */
	pin_set(&gpio_sc, PORT_C,  6, 1); /* Vref */
#endif

	pnp_yenable();
	pnp_xenable();

	error = pnp_home();
	if (error)
		return (error);

	for (i = 0; i < 10; i++) {
		new_x = get_random() % PNP_MAX_X_NM;
		new_y = get_random() % PNP_MAX_Y_NM;
		printf("moving to %u %u\n", new_x, new_y);
		pnp_move(new_x, new_y);
		mdx_usleep(25000);
	}

	pnp_move(0, 0);
	printf("Final x y %d %d\n", pnp.pos_x, pnp.pos_y);

	return (0);
}
