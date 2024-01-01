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

#include "trig.h"

#define	TRIG_DEBUG
#undef	TRIG_DEBUG

#ifdef	TRIG_DEBUG
#define	dprintf(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define	dprintf(fmt, ...)
#endif

#define	M_PI	3.14159265358979323846
#define	DEG(x)	((180 / M_PI) * (x))
#define	RAD(x)	((M_PI / 180) * (x))

/*
 * cam_radius and z are in mm.
 * return value is motor rotation degrees multiplied by 1000000.
 *
 * Note: absolute z is expected as input.
 */
int
trig_translate_z(float z0, float cam_radius)
{
	float val;
	float deg;
	float z;
	int result;

	z = abs(z0);

	/* Can't rotate for more than 180 deg. */
	if (z > cam_radius * 2)
		return (-1);

	val = z / cam_radius - 1;
	deg = 90 + DEG(asin(val));

	/* Convert to nano. */
	deg *= 1000000;

	/* Check if negative. */
	if (z0 < 0)
		deg *= -1;

	result = deg;

	dprintf("%s: z %f mm, deg %d\n", __func__, z, result);

	return (result);
}

void
trig_test(void)
{
	int cam_radius;
	int result;
	float j;

	cam_radius = 15000000;

	for (j = 0; j < 30; j += 1) {
		result = trig_translate_z(j * 1000000, cam_radius);
		printf("%s: z %f mm, deg %d\n", __func__, j, result);
	}
}
