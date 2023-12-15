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

#define	PNP_DEBUG
#undef	PNP_DEBUG

#ifdef	PNP_DEBUG
#define	dprintf(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define	dprintf(fmt, ...)
#endif

extern struct stm32f4_gpio_softc gpio_sc;
extern struct stm32f4_pwm_softc pwm_x_sc;
extern struct stm32f4_pwm_softc pwm_y_sc;
extern struct stm32f4_pwm_softc pwm_z_sc;
extern struct stm32f4_pwm_softc pwm_h1_sc;
extern struct stm32f4_pwm_softc pwm_h2_sc;
extern struct stm32f4_rng_softc rng_sc;

#define	PNP_MAX_X_NM	300000000	/* nanometers */
#define	PNP_MAX_Y_NM	300000000	/* nanometers */
#define	PNP_STEP_NM	6250		/* Length of a step, nanometers */
#define	PNP_STEPS_PER_MM	160

struct move_task {
	int new_pos;
	int steps;
	int check_home;
	int dir;
	int speed;
	mdx_sem_t task_compl_sem;
	int speed_control;

	/* Result */
	int home_found;
};

struct motor_state {
	int pos;
	int dir;
	int chanset;	/* PWM channels. */
	mdx_sem_t worker_sem;
	struct move_task task;
	char *name;
	void (*set_direction)(int dir);
	int (*is_at_home)(void);
	void (*step)(int chanset, int speed);
	mdx_sem_t step_sem;
};

struct pnp_state {
	struct motor_state motor_x;
	struct motor_state motor_y;
	struct motor_state motor_z;
	struct motor_state motor_h1;
	struct motor_state motor_h2;
};

static struct pnp_state pnp;

void
pnp_pwm_y_intr(void *arg, int irq)
{

	stm32f4_pwm_intr(arg, irq);
	mdx_sem_post(&pnp.motor_y.step_sem);
}

void
pnp_pwm_x_intr(void *arg, int irq)
{

	stm32f4_pwm_intr(arg, irq);
	mdx_sem_post(&pnp.motor_x.step_sem);
}

void
pnp_pwm_z_intr(void *arg, int irq)
{

	stm32f4_pwm_intr(arg, irq);
	mdx_sem_post(&pnp.motor_z.step_sem);
}

void
pnp_pwm_h1_intr(void *arg, int irq)
{

	stm32f4_pwm_intr(arg, irq);
	mdx_sem_post(&pnp.motor_h1.step_sem);
}

void
pnp_pwm_h2_intr(void *arg, int irq)
{

	stm32f4_pwm_intr(arg, irq);
	mdx_sem_post(&pnp.motor_h2.step_sem);
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

static int
pnp_is_z_home(void)
{

	return (pin_get(&gpio_sc, PORT_B, 4));
}

#if 0
static int
pnp_is_yr_home(void)
{

	return (pin_get(&gpio_sc, PORT_C, 1));
}
#endif

static void
pnp_xenable(int enable)
{

	pin_set(&gpio_sc, PORT_D, 14, enable); /* X Vref */
	mdx_usleep(10000);
	pin_set(&gpio_sc, PORT_E, 6, enable); /* X ST */
	mdx_usleep(10000);
}

static void
pnp_yenable(int enable)
{

	pin_set(&gpio_sc, PORT_D, 15, enable); /* Y Vref */
	mdx_usleep(10000);
	pin_set(&gpio_sc, PORT_C, 0, enable); /* Y R ST */
	pin_set(&gpio_sc, PORT_A, 8, enable); /* Y L ST */
	mdx_usleep(10000);
}

static void
pnp_zenable(int enable)
{

	pin_set(&gpio_sc, PORT_D, 13, enable); /* Z Vref */
	mdx_usleep(10000);
	pin_set(&gpio_sc, PORT_E, 4, enable); /* ST */
	mdx_usleep(10000);
}

static void
pnp_henable(int enable)
{

	pin_set(&gpio_sc, PORT_D, 12, enable); /* H Vref */
	mdx_usleep(10000);
	pin_set(&gpio_sc, PORT_D, 3, enable); /* H1 ST */
	pin_set(&gpio_sc, PORT_A, 15, enable); /* H2 ST */
	mdx_usleep(10000);
}

static void
pnp_xset_direction(int dir)
{

	pin_set(&gpio_sc, PORT_E, 5, dir); /* X FR */
}

static void
pnp_h1set_direction(int dir)
{

	pin_set(&gpio_sc, PORT_D, 1, dir); /* H1 FR */
}

static void
pnp_h2set_direction(int dir)
{

	pin_set(&gpio_sc, PORT_D, 0, dir); /* H2 FR */
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
pnp_zset_direction(int dir)
{

	pin_set(&gpio_sc, PORT_E, 3, dir); /* Z FR */
}

static void
xstep(int chanset, int speed)
{
	uint32_t freq;

	freq = speed * 150000;

	stm32f4_pwm_step(&pwm_x_sc, chanset, freq);
}

static void
ystep(int chanset, int speed)
{
	uint32_t freq;

	freq = speed * 150000;

	stm32f4_pwm_step(&pwm_y_sc, chanset, freq);
}

static void
zstep(int chanset, int speed)
{
	uint32_t freq;

	freq = speed * 150000;

	stm32f4_pwm_step(&pwm_z_sc, chanset, freq);
}

static void
h1step(int chanset, int speed)
{
	uint32_t freq;

	freq = speed * 50000;

	stm32f4_pwm_step(&pwm_h1_sc, chanset, freq);
}

static void
h2step(int chanset, int speed)
{
	uint32_t freq;

	freq = speed * 50000;

	stm32f4_pwm_step(&pwm_h2_sc, chanset, freq);
}

static int
calc_speed(int i, int steps, int speed)
{
	int t;

	t = i < (steps - i) ? i : (steps - i);
	if (t < 1000) {
		/* Gradually increase/decrease speed */
		speed = (t * 100) / 1000;
		if (speed < 15)
			speed = 15;
	}

	return (speed);
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
		dprintf("%s: TR\n", __func__);

		steps = task->steps;
		speed = task->speed;

		dprintf("%s: steps needed %d\n", __func__, steps);

		for (i = 0; i < steps; i++) {
			if (task->check_home && motor->is_at_home()) {
				task->home_found = 1;
				break;
			}

			if (task->speed_control)
				speed = calc_speed(i, steps, speed);

			motor->step(motor->chanset, speed);
			mdx_sem_wait(&motor->step_sem);
			if (task->dir == 1)
				motor->pos += PNP_STEP_NM;
			else
				motor->pos -= PNP_STEP_NM;
		}

		dprintf("%s: new pos %d\n", motor->name, motor->pos);
		mdx_sem_post(&task->task_compl_sem);
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
	task->speed = 100;
	task->speed_control = 1;

	motor->set_direction(task->dir);
	mdx_sem_post(&motor->worker_sem);
}

static int
pnp_move_xy(uint32_t new_pos_x, uint32_t new_pos_y)
{

	if (new_pos_x > PNP_MAX_X_NM)
		new_pos_x = PNP_MAX_X_NM;
	if (new_pos_y > PNP_MAX_Y_NM)
		new_pos_y = PNP_MAX_Y_NM;

	mover(&pnp.motor_x, new_pos_x);
	mover(&pnp.motor_y, new_pos_y);

	mdx_sem_wait(&pnp.motor_x.task.task_compl_sem);
	mdx_sem_wait(&pnp.motor_y.task.task_compl_sem);

	dprintf("%s: new pos %d %d\n", __func__, pnp.motor_x.pos,
	    pnp.motor_y.pos);

	return (0);
}

static int
pnp_move_z(int new_pos)
{
	struct motor_state *motor;
	struct move_task *task;
	int delta;

	motor = &pnp.motor_z;
	task = &motor->task;
	task->check_home = 0;
	task->speed_control = 1;

	task->new_pos = new_pos;
	if (task->new_pos > motor->pos) {
		task->dir = 1;
		delta = task->new_pos - motor->pos;
	} else {
		task->dir = 0;
		delta = motor->pos - task->new_pos;
	}

	task->steps = abs(delta) / PNP_STEP_NM;
	task->speed = 100;

	motor->set_direction(task->dir);
	mdx_sem_post(&motor->worker_sem);
	mdx_sem_wait(&motor->task.task_compl_sem);

	dprintf("%s: new z %d\n", __func__, motor->pos);

	return (0);
}

static void
pnp_move_head(int head, int new_pos)
{
	struct motor_state *motor;
	struct move_task *task;
	int delta;

	motor = head == 1 ? &pnp.motor_h1 : &pnp.motor_h2;
	task = &motor->task;
	task->check_home = 0;
	task->speed_control = 1;

	task->new_pos = new_pos;
	if (task->new_pos > motor->pos) {
		task->dir = 1;
		delta = task->new_pos - motor->pos;
	} else {
		task->dir = 0;
		delta = motor->pos - task->new_pos;
	}

	task->steps = abs(delta) / PNP_STEP_NM;
	task->speed = 100;

	motor->set_direction(task->dir);
	mdx_sem_post(&motor->worker_sem);
	mdx_sem_wait(&motor->task.task_compl_sem);

	dprintf("%s: head%d new_pos %d\n", __func__, head, motor->pos);
}

static void
pnp_move_home_motor(struct motor_state *motor)
{
	struct move_task *task;

	task = &motor->task;

	/* Ensure we are not at home. */
	if (motor->is_at_home()) {
		/* Already at home, move back a bit. */
		motor->set_direction(1);
		task->steps = PNP_STEPS_PER_MM * 10;
		task->speed = 20;
		task->speed_control = 0;
		task->check_home = 0;
		mdx_sem_post(&motor->worker_sem);
		mdx_sem_wait(&task->task_compl_sem);
		if (motor->is_at_home())
			panic("still at home");
	}

	/* Try to reach home. */
	task->steps = PNP_MAX_Y_NM / PNP_STEP_NM;
	task->check_home = 1;
	task->speed = 30;
	task->speed_control = 0;
	motor->set_direction(0);
	mdx_sem_post(&motor->worker_sem);
	mdx_sem_wait(&task->task_compl_sem);

	/* Now go into home for 1 mm. */
	task->steps = 1000000 / PNP_STEP_NM;
	task->check_home = 0;
	task->speed = 15;
	task->speed_control = 0;
	motor->set_direction(0);
	mdx_sem_post(&motor->worker_sem);
	mdx_sem_wait(&task->task_compl_sem);

	motor->pos = 0;
	printf("%s home reached\n", motor->name);
}

static int
pnp_move_home_z(struct motor_state *motor)
{
	struct move_task *task;
	int found;
	int steps;
	int dir;
	int i;

	task = &motor->task;

	/* First leave home. */
	if (pnp_is_z_home()) {
		task->steps = 200;
		task->check_home = 0;
		task->speed = 15;
		task->speed_control = 0;
		task->home_found = 0;
		motor->set_direction(1);
		mdx_sem_post(&motor->worker_sem);
		mdx_sem_wait(&task->task_compl_sem);
		/* TODO: ensure we left it. */
	}

	steps = 100;
	dir = 1;
	found = 0;

	/* Now find home once again. */
	for (i = 0; i < 20; i++) {
		printf("Making %d steps towards %d\n", steps, dir);
		motor->set_direction(dir);
		task->steps = steps;
		task->check_home = 1;
		task->speed = 15;
		task->speed_control = 0;
		task->home_found = 0;
		mdx_sem_post(&motor->worker_sem);
		mdx_sem_wait(&task->task_compl_sem);
		if (task->home_found) {
			found = 1;
			break;
		}
		steps += 200;
		dir = !dir;
	}

	if (found == 0) {
		printf("Error: Z Home not found\n");
		return (-1);
	}

	/* Now make 50 steps into home. */
	task->steps = 50;
	task->check_home = 0;
	task->speed = 15;
	task->speed_control = 0;
	task->home_found = 0;
	motor->set_direction(dir);
	mdx_sem_post(&motor->worker_sem);
	mdx_sem_wait(&task->task_compl_sem);

	motor->pos = 0;
	printf("z home found\n");

	return (0);
}

static int
pnp_move_home(void)
{
	int error;

	error = pnp_move_home_z(&pnp.motor_z);
	if (error)
		return (error);

	pnp_move_home_motor(&pnp.motor_y);
	pnp_move_home_motor(&pnp.motor_x);

	return (0);
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
pnp_thread_create(char *name, void *arg)
{
	struct thread *td;

	td = mdx_thread_create(name, 1 /* prio */, 500 /* quantum */,
	    8192 /* stack */, pnp_worker_thread, arg);
	if (td == NULL) {
		printf("%s: Failed to create X mover thread\n", __func__);
		return (-1);
	}

	mdx_sched_add(td);

	return (0);
}

static int
pnp_initialize(void)
{
	int error;

	bzero(&pnp, sizeof(struct pnp_state));

	pnp_motor_initialize(&pnp.motor_x, "X Motor");
	pnp.motor_x.set_direction = pnp_xset_direction;
	pnp.motor_x.step = xstep;
	pnp.motor_x.chanset = (1 << 0);
	pnp.motor_x.is_at_home = pnp_is_x_home;
	mdx_sem_init(&pnp.motor_x.task.task_compl_sem, 0);

	pnp_motor_initialize(&pnp.motor_y, "Y Motor");
	pnp.motor_y.set_direction = pnp_yset_direction;
	pnp.motor_y.step = ystep;
	pnp.motor_y.chanset = ((1 << 0) | (1 << 1));
	pnp.motor_y.is_at_home = pnp_is_yl_home;
	mdx_sem_init(&pnp.motor_y.task.task_compl_sem, 0);

	pnp_motor_initialize(&pnp.motor_z, "Z Motor");
	pnp.motor_z.set_direction = pnp_zset_direction;
	pnp.motor_z.step = zstep;
	pnp.motor_z.chanset = (1 << 0);
	pnp.motor_z.is_at_home = pnp_is_z_home;
	mdx_sem_init(&pnp.motor_z.task.task_compl_sem, 0);

	pnp_motor_initialize(&pnp.motor_h1, "H1 Motor");
	pnp.motor_h1.set_direction = pnp_h1set_direction;
	pnp.motor_h1.step = h1step;
	pnp.motor_h1.chanset = (1 << 0);
	pnp.motor_h1.is_at_home = NULL;
	mdx_sem_init(&pnp.motor_h1.task.task_compl_sem, 0);

	pnp_motor_initialize(&pnp.motor_h2, "H2 Motor");
	pnp.motor_h2.set_direction = pnp_h2set_direction;
	pnp.motor_h2.step = h2step;
	pnp.motor_h2.chanset = (1 << 0);
	pnp.motor_h2.is_at_home = NULL;
	mdx_sem_init(&pnp.motor_h2.task.task_compl_sem, 0);

	error = pnp_thread_create("X Motor", &pnp.motor_x);
	if (error) {
		printf("%s: Failed to create X mover thread\n", __func__);
		return (-1);
	}

	error = pnp_thread_create("Y Motor", &pnp.motor_y);
	if (error) {
		printf("%s: Failed to create Y mover thread\n", __func__);
		return (-1);
	}

	error = pnp_thread_create("Z Motor", &pnp.motor_z);
	if (error) {
		printf("%s: Failed to create Z mover thread\n", __func__);
		return (-1);
	}

	error = pnp_thread_create("H1 Motor", &pnp.motor_h1);
	if (error) {
		printf("%s: Failed to create H1 mover thread\n", __func__);
		return (-1);
	}

	error = pnp_thread_create("H2 Motor", &pnp.motor_h2);
	if (error) {
		printf("%s: Failed to create H2 mover thread\n", __func__);
		return (-1);
	}

	pnp_xenable(1);
	pnp_yenable(1);
	pnp_zenable(1);

	return (0);
}

static void
pnp_deinitialize(void)
{

	pnp_xenable(0);
	pnp_yenable(0);
	pnp_zenable(0);
}

static void
pnp_test_heads(void)
{

	printf("starting moving head\n");
	while (2) {
		pnp_henable(1);
		pnp_move_head(1, 10000000);
		pnp_move_head(2, 10000000);
		pnp_henable(0);
		mdx_usleep(500000);

		pnp_henable(1);
		pnp_move_head(1, -10000000);
		pnp_move_head(2, -10000000);
		pnp_henable(0);
		mdx_usleep(500000);
	}
	printf("head moving done\n");
}

static void
pnp_move_random(void)
{
	uint32_t new_x;
	uint32_t new_y;
	int i;

	for (i = 0; i < 100; i++) {
		new_x = get_random() % PNP_MAX_X_NM;
		new_y = get_random() % PNP_MAX_Y_NM;
		printf("%d: moving to %u %u\n", i, new_x, new_y);
		pnp_move_xy(new_x, new_y);
		pnp_move_z(-20000000);
		pnp_move_z(20000000);
		pnp_move_z(0);
	}

	pnp_move_xy(0, 0);
}

int
pnp_test(void)
{
	int error;

	pnp_initialize();
	pnp_test_heads();
	error = pnp_move_home();
	if (error)
		return (error);
	pnp_move_random();
	pnp_deinitialize();

	return (0);
}
