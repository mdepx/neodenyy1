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
#include <sys/thread.h>

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

struct move_task {
	uint32_t new_pos;
	uint32_t steps;
	int check_home;
	int dir;
	int speed;
};

struct motor_state {
	uint32_t pos;
	uint32_t dir;
	uint32_t chanset;	/* PWM channels. */
	mdx_sem_t worker_sem;
	struct move_task task;
	char *name;
	void (*set_direction)(int dir);
	int (*is_at_home)(void);
	void (*step)(int chanset, int speed);
	mdx_sem_t step_sem;
};

struct pnp_state {
	uint32_t pos_x;
	uint32_t pos_y;

	struct motor_state motor_x;
	struct motor_state motor_y;
};

static struct pnp_state pnp;
static mdx_sem_t xsem;
static mdx_sem_t ysem;

void
pnp_pwm_y_intr(void *arg, int irq)
{

	/* Step completed. */

	stm32f4_pwm_intr(arg, irq);

	//mdx_sem_post(&ysem);
	mdx_sem_post(&pnp.motor_y.step_sem);
}

void
pnp_pwm_x_intr(void *arg, int irq)
{

	/* Step completed. */

	stm32f4_pwm_intr(arg, irq);

	//mdx_sem_post(&xsem);
	mdx_sem_post(&pnp.motor_x.step_sem);
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
xstep(int chanset, int speed)
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

	stm32f4_pwm_step(&pwm_y_sc, chanset, freq);
}

static void
pnp_worker_thread(void *arg)
{
	struct motor_state *motor;
	struct move_task *task;
	uint32_t steps;
	int speed;
	int i;

	motor = arg;
	task = &motor->task;

	while (1) {
		mdx_sem_wait(&motor->worker_sem);
		printf("%s: TR\n", __func__);

		steps = task->steps;
		speed = task->speed;

		printf("%s: steps needed %d\n", __func__, steps);

		for (i = 0; i < steps; i++) {
			if (task->check_home && motor->is_at_home())
				break;
			motor->step(motor->chanset, speed);
			mdx_sem_wait(&motor->step_sem);
			if (task->dir == 1)
				motor->pos += PNP_STEP_NM;
			else
				motor->pos -= PNP_STEP_NM;
		}

		printf("%s: new pos %d\n", motor->name, motor->pos);
	}
}

static void
mover(struct motor_state *motor, uint32_t new_pos)
{
	struct move_task *task;
	uint32_t delta;

	task = &motor->task;

	task->new_pos = new_pos < 0 ? 0 : new_pos;
	if (task->new_pos > motor->pos) {
		task->dir = 1;
		delta = task->new_pos - motor->pos;
	} else {
		task->dir = 0;
		delta = motor->pos - task->new_pos;
	}

	task->steps = delta / PNP_STEP_NM;
	task->speed = 50;

	motor->set_direction(task->dir);

	mdx_sem_post(&motor->worker_sem);
}

static int __unused
pnp_move_xy(uint32_t new_pos_x, uint32_t new_pos_y)
{

	if (new_pos_x > PNP_MAX_X_NM)
		new_pos_x = PNP_MAX_X_NM;
	if (new_pos_y > PNP_MAX_Y_NM)
		new_pos_y = PNP_MAX_Y_NM;

	mover(&pnp.motor_x, new_pos_x);
	mover(&pnp.motor_y, new_pos_y);

	return (0);
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
		xstep(0, 25);
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
			xstep(0, xspeed);
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

static void
pnp_motor_initialize(struct motor_state *state, char *name)
{

	mdx_sem_init(&state->worker_sem, 0);
	mdx_sem_init(&state->step_sem, 0);
	state->name = name;
}

static int
pnp_initialize(void)
{
	struct thread *td1, *td2;

	bzero(&pnp, sizeof(struct pnp_state));

	td1 = mdx_thread_create("X Motor", 1 /* prio */, 500 /* quantum */,
	    8192 /* stack */, pnp_worker_thread, &pnp.motor_x);
	if (td1 == NULL) {
		printf("%s: Failed to create X mover thread\n", __func__);
		return (-1);
	}

	td2 = mdx_thread_create("Y Motor", 1 /* prio */, 500 /* quantum */,
	    8192 /* stack */, pnp_worker_thread, &pnp.motor_y);
	if (td2 == NULL) {
		printf("%s: Failed to create Y mover thread\n", __func__);
		return (-1);
	}

	mdx_sched_add(td1);
	mdx_sched_add(td2);

	pnp_motor_initialize(&pnp.motor_x, "X Motor");
	pnp.motor_x.set_direction = pnp_xset_direction;
	pnp.motor_x.step = xstep;
	pnp.motor_x.chanset = (1 << 0);
	pnp.motor_x.is_at_home = pnp_is_x_home;

	pnp_motor_initialize(&pnp.motor_y, "Y Motor");
	pnp.motor_y.set_direction = pnp_yset_direction;
	pnp.motor_y.step = ystep;
	pnp.motor_y.chanset = ((1 << 0) | (1 << 1));
	pnp.motor_x.is_at_home = pnp_is_yl_home;

	pnp_xenable();
	pnp_yenable();

	return (0);
}

static void
pnp_move_home(void)
{

	//pnp_move_xy(100 * 1000000, 100 * 1000000);
}

static void
pnp_test_new(void)
{

	pnp_move_xy(100 * 1000000, 100 * 1000000);
}

int
pnp_test(void)
{
	uint32_t new_x;
	uint32_t new_y;
	int error;
	int i;

#if 1
	/* TODO */
	pin_set(&gpio_sc, PORT_D, 15, 1); /* Vref */
	pin_set(&gpio_sc, PORT_C,  6, 1); /* Vref */
#endif

	pnp.pos_x = -1;
	pnp.pos_y = -1;

	pnp_initialize();
	pnp_move_home();
	pnp_test_new();

	while (1)
		mdx_usleep(100000);

	return (0);

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
