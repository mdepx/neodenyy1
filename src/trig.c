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

#include "board.h"
#include "pnp.h"

#define	PNP_DEBUG
#undef	PNP_DEBUG

#ifdef	PNP_DEBUG
#define	dprintf(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define	dprintf(fmt, ...)
#endif

#define	M_PI	3.14159265358979323846
#define	DEG(x)	((180 / M_PI) * (x))
#define	RAD(x)	((M_PI / 180) * (x))

/*
 * cam_radius and new_z are in mm.
 * return value is motor rotation degrees.
 */
int
translate_z(float cam_radius, float new_z)
{
	float val;
	float deg;

	/* Can't rotate more than 180 deg. */
	if (new_z > cam_radius * 2)
		return (-1);

	deg = 0;

	if (new_z > cam_radius) {
		deg += 90;
		val = (new_z - cam_radius) / cam_radius;
	} else
		val = (new_z / cam_radius);

	deg += DEG(asin(val));

	/* Covert to nano. */
	deg *= 1000000;

	printf("new_z %f mm, deg %f\n", new_z, deg);

	return (0);
}
