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

#include <lib/msun/src/math.h>

#include <arm/stm/stm32f4.h>

#include "board.h"
#include "pnp.h"
#include "trig.h"

#define	PNP_DEBUG
#undef	PNP_DEBUG

#ifdef	PNP_DEBUG
#define	dprintf(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define	dprintf(fmt, ...)
#endif

#define	PNP_MAX_X_NM		(340000000)	/* nanometers */
#define	PNP_MAX_Y_NM		(368000000)	/* nanometers */
#define	CAM_RADIUS		(15000000)

/* XY steppers are in linear motion. */
#define	PNP_XY_FULL_REVO_NM	(40000000)
#define	PNP_XY_FULL_REVO_STEPS	(6400)
#define	PNP_XY_STEP_NM		(PNP_XY_FULL_REVO_NM / PNP_XY_FULL_REVO_STEPS)

/* Z stepper: we translate linear into rotational motion. */
#define	PNP_Z_FULL_REVO_DEG	(360000000)
#define	PNP_Z_FULL_REVO_STEPS	(12800)
#define	PNP_Z_STEP_DEG		(PNP_Z_FULL_REVO_DEG / PNP_Z_FULL_REVO_STEPS)

/* NR (Nozzle Rotation) steppers are in rotational motion. */
#define	PNP_NR_FULL_REVO_DEG	(360000000)
#define	PNP_NR_FULL_REVO_STEPS	(12800)
#define	PNP_NR_STEP_DEG		(PNP_NR_FULL_REVO_DEG / PNP_NR_FULL_REVO_STEPS)

struct move_task {
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
	int dir;
	int chanset;	/* PWM channels. */
	mdx_sem_t worker_sem;
	struct move_task task;
	const char *name;
	void (*set_direction)(int dir);
	int (*is_at_home)(void);
	void (*step)(int chanset, int speed);
	mdx_sem_t step_sem;
	int step_nm;	/* Length of a step, nanometers. Has to be signed. */

	int (*cam_translate_mm_to_deg)(float z, float cam_radius, int *result);
	int cam_radius;

	/*
	 * Current offset from home in steps.
	 * Could be negative for Z and heads.
	 */
	int steps;
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

static inline int
pnp_is_x_home(void)
{

	return (pin_get(&gpio_sc, PORT_C, 6));
}

static inline int
pnp_is_yl_home(void)
{

	return (pin_get(&gpio_sc, PORT_C, 7));
}

static inline int
pnp_is_z_home(void)
{

	return (pin_get(&gpio_sc, PORT_B, 4));
}

static inline int __unused
pnp_is_yr_home(void)
{

	return (pin_get(&gpio_sc, PORT_C, 1));
}

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

void
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
				motor->steps += 1;
			else
				motor->steps -= 1;
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
	int new_steps;

	task = &motor->task;

	new_steps = new_pos / motor->step_nm;

	if (new_steps > motor->steps) {
		task->dir = 1;
		delta = abs(new_steps - motor->steps);
	} else {
		task->dir = 0;
		delta = abs(motor->steps - new_steps);
	}

	task->steps = delta;
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
	uint32_t delta;
	int cam_radius;
	int new_z_steps;
	int new_z;
	int error;

	cam_radius = CAM_RADIUS;

	if (abs(new_pos) > cam_radius * 2) {
		printf("%s: Can't move Z to %d\n", __func__, new_pos);
		return (0);
	}

	motor = &pnp.motor_z;
	task = &motor->task;
	task->check_home = 0;
	task->speed_control = 1;

	/* Convert required position from mm to degrees if needed. */
	if (motor->cam_translate_mm_to_deg) {
		if (motor->cam_radius == 0)
			return (-1);
		error = motor->cam_translate_mm_to_deg(new_pos,
		    motor->cam_radius, &new_z);
		if (error) {
			printf("Error: can't translate coordinate\n");
			return (-2);
		}
		new_pos = new_z;
	}

	new_z_steps = new_pos / motor->step_nm;
	if (new_z_steps > motor->steps) {
		task->dir = 1;
		delta = abs(new_z_steps - motor->steps);
	} else {
		task->dir = 0;
		delta = abs(motor->steps - new_z_steps);
	}

	task->steps = delta;
	task->speed = 100;

	motor->set_direction(task->dir);
	mdx_sem_post(&motor->worker_sem);
	mdx_sem_wait(&motor->task.task_compl_sem);

	dprintf("%s: new z %d\n", __func__, motor->pos);

	return (0);
}

static void
pnp_move_head_nonblock(int head, int new_pos)
{
	struct motor_state *motor;
	struct move_task *task;
	int new_h_steps;
	uint32_t delta;

	motor = head == 1 ? &pnp.motor_h1 : &pnp.motor_h2;
	task = &motor->task;
	task->check_home = 0;
	task->speed_control = 1;

	new_h_steps = new_pos / motor->step_nm;
	if (new_h_steps > motor->steps) {
		task->dir = 1;
		delta = abs(new_h_steps - motor->steps);
	} else {
		task->dir = 0;
		delta = abs(motor->steps - new_h_steps);
	}

	task->steps = delta;
	task->speed = 100;

	printf("%s: making %d steps\n", __func__, task->steps);

	motor->set_direction(task->dir);
	mdx_sem_post(&motor->worker_sem);
}

static void
pnp_move_head(int head, int new_pos)
{
	struct motor_state *motor;

	motor = head == 1 ? &pnp.motor_h1 : &pnp.motor_h2;

	pnp_move_head_nonblock(head, new_pos);

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
		task->steps = 10000000 / motor->step_nm;
		task->speed = 20;
		task->speed_control = 1;
		task->check_home = 0;
		mdx_sem_post(&motor->worker_sem);
		mdx_sem_wait(&task->task_compl_sem);
		if (motor->is_at_home())
			panic("still at home");
	}

	printf("%s is trying to reach home\n", motor->name);
	/* Try to reach home. */
	task->steps = PNP_MAX_Y_NM / motor->step_nm;
	task->check_home = 1;
	task->speed = 20;
	task->speed_control = 0;
	motor->set_direction(0);
	mdx_sem_post(&motor->worker_sem);
	mdx_sem_wait(&task->task_compl_sem);

	printf("%s is going into home for 1mm\n", motor->name);

	/* Now go into home for 1 mm. */
	task->steps = 1000000 / motor->step_nm;
	task->check_home = 0;
	task->speed = 5;
	task->speed_control = 0;
	motor->set_direction(0);
	mdx_sem_post(&motor->worker_sem);
	mdx_sem_wait(&task->task_compl_sem);

	motor->steps = 0;
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

	motor->steps = 0;
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

void
pnp_command_move(struct command *cmd)
{
	uint32_t x, y, z, h1, h2;

	if (cmd->x_set) {
		x = cmd->x;
		if (x > PNP_MAX_X_NM)
			x = PNP_MAX_X_NM;
		printf("moving X to %d\n", x);
		mover(&pnp.motor_x, x);
	}

	if (cmd->y_set) {
		y = cmd->y;
		if (y > PNP_MAX_Y_NM)
			y = PNP_MAX_Y_NM;
		printf("moving Y to %d\n", y);
		mover(&pnp.motor_y, y);
	}

	if (cmd->h1_set) {
		h1 = cmd->h1;
		printf("moving H1 to %d\n", h1);
		pnp_move_head_nonblock(1, h1);
	}

	if (cmd->h2_set) {
		h2 = cmd->h2;
		printf("moving H2 to %d\n", h2);
		pnp_move_head_nonblock(2, h2);
	}

	if (cmd->h1_set)
		mdx_sem_wait(&pnp.motor_h1.task.task_compl_sem);
	if (cmd->h2_set)
		mdx_sem_wait(&pnp.motor_h2.task.task_compl_sem);
	if (cmd->x_set)
		mdx_sem_wait(&pnp.motor_x.task.task_compl_sem);
	if (cmd->y_set)
		mdx_sem_wait(&pnp.motor_y.task.task_compl_sem);

	if (cmd->z_set) {
		z = cmd->z;
		printf("moving Z to %d\n", z);
		pnp_move_z(z);
	}
}

static void
pnp_motor_initialize(struct motor_state *motor, const char *name)
{

	mdx_sem_init(&motor->worker_sem, 0);
	mdx_sem_init(&motor->step_sem, 0);
	motor->name = name;
}

static int
pnp_thread_create(const char *name, void *arg)
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
	pnp.motor_x.step_nm = PNP_XY_STEP_NM;
	pnp.motor_x.set_direction = pnp_xset_direction;
	pnp.motor_x.step = xstep;
	pnp.motor_x.chanset = (1 << 0);
	pnp.motor_x.is_at_home = pnp_is_x_home;
	mdx_sem_init(&pnp.motor_x.task.task_compl_sem, 0);

	pnp_motor_initialize(&pnp.motor_y, "Y Motor");
	pnp.motor_y.step_nm = PNP_XY_STEP_NM;
	pnp.motor_y.set_direction = pnp_yset_direction;
	pnp.motor_y.step = ystep;
	pnp.motor_y.chanset = ((1 << 0) | (1 << 1));
	pnp.motor_y.is_at_home = pnp_is_yl_home;
	mdx_sem_init(&pnp.motor_y.task.task_compl_sem, 0);

	pnp_motor_initialize(&pnp.motor_z, "Z Motor");
	pnp.motor_z.step_nm = PNP_Z_STEP_DEG;
	pnp.motor_z.set_direction = pnp_zset_direction;
	pnp.motor_z.step = zstep;
	pnp.motor_z.chanset = (1 << 0);
	pnp.motor_z.is_at_home = pnp_is_z_home;
	pnp.motor_z.cam_translate_mm_to_deg = trig_translate_z;
	pnp.motor_z.cam_radius = CAM_RADIUS;
	mdx_sem_init(&pnp.motor_z.task.task_compl_sem, 0);

	pnp_motor_initialize(&pnp.motor_h1, "H1 Motor");
	pnp.motor_h1.step_nm = PNP_NR_STEP_DEG;
	pnp.motor_h1.set_direction = pnp_h1set_direction;
	pnp.motor_h1.step = h1step;
	pnp.motor_h1.chanset = (1 << 0);
	pnp.motor_h1.is_at_home = NULL;
	mdx_sem_init(&pnp.motor_h1.task.task_compl_sem, 0);

	pnp_motor_initialize(&pnp.motor_h2, "H2 Motor");
	pnp.motor_h2.step_nm = PNP_NR_STEP_DEG;
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
	int i;

	pnp_henable(1);

	printf("starting moving head\n");
	for (i = 0; i < 1; i++) {
		pnp_move_head(1, 10000000);
		pnp_move_head(2, 10000000);
		mdx_usleep(500000);

		pnp_move_head(1, -10000000);
		pnp_move_head(2, -10000000);
		mdx_usleep(500000);

		pnp_move_head(1, 0);
		pnp_move_head(2, 0);
		mdx_usleep(500000);
	}
	printf("head moving done\n");

	pnp_henable(0);
}

static void
pnp_move_random(void)
{
	uint32_t new_x;
	uint32_t new_y;
	int i;

	for (i = 0; i < 2; i++) {
		new_x = board_get_random() % PNP_MAX_X_NM;
		new_y = board_get_random() % PNP_MAX_Y_NM;
		printf("%d: moving to %u %u\n", i, new_x, new_y);
		pnp_move_xy(new_x, new_y);
		pnp_move_z(-10000000);
		pnp_move_z(10000000);
		pnp_move_z(0);
	}

	//pnp_move_z(-31000000);
	//pnp.motor_z.steps = 0;

	pnp_move_xy(0, 0);
}

static int
pnp_test_z(void)
{
	int error;
	int i;

	error = pnp_move_home_z(&pnp.motor_z);
	if (error)
		return (error);

	while (1) {
		for (i = 6; i < 17; i++) {
			pnp_move_z(i * 1000000);
			mdx_usleep(500000);
			mdx_usleep(500000);
		}
	}

	return (0);
}

int
pnp_test(void)
{
	int error;

	pnp_initialize();
	pnp_test_heads();
	if (1 == 0)
		pnp_test_z();
	error = pnp_move_home();
	if (error)
		return (error);
	pnp_move_random();
	pnp_mainloop();
	pnp_deinitialize();

	return (0);
}
