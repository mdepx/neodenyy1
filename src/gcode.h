/*-
 * Copyright (c) 2024 Ruslan Bukin <br@bsdpad.com>
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

#ifndef _SRC_GCODE_H_
#define	_SRC_GCODE_H_

struct gcode_command {
	int type;
#define	CMD_TYPE_MOVE		1
#define	CMD_TYPE_ACTUATE	2
#define	CMD_TYPE_SENSOR_READ	3

	int x;
	int y;
	int z;
	int h1;
	int h2;
	int x_set;
	int y_set;
	int z_set;
	int h1_set;
	int h2_set;

	int actuate_target;
#define	PNP_ACTUATE_TARGET_PUMP		1
#define	PNP_ACTUATE_TARGET_AVAC1	2
#define	PNP_ACTUATE_TARGET_AVAC2	3
#define	PNP_ACTUATE_TARGET_NEEDLE	4
#define	PNP_ACTUATE_TARGET_PEEL		5
	int actuate_value;

	int sensor_read_target;
};

int gcode_mainloop(void);

#endif /* !_SRC_GCODE_H_ */
