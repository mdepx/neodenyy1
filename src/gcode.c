/*-
 * Copyright (c) 2023-2024 Ruslan Bukin <br@bsdpad.com>
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

#include <arm/stm/stm32f4.h>

#include "board.h"
#include "gcode.h"
#include "pnp.h"

#define	GCODE_DEBUG
#undef	GCODE_DEBUG

#ifdef	GCODE_DEBUG
#define	dprintf(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define	dprintf(fmt, ...)
#endif

#define	MAX_DMA_BUF_SIZE	4096
#define	MAX_GCODE_LEN		256

static uint8_t dma_buffer[MAX_DMA_BUF_SIZE];
static uint8_t cmd_buffer[MAX_GCODE_LEN];
static int cmd_buffer_ptr;

static void
gcode_command_sensor_read(struct command *cmd)
{
	int val;

	if (cmd->sensor_read_target == 1) {
		val = pin_get(&gpio_sc, PORT_B, 3) ? 0 : 1;
		printf("ok V:%d\n", val);
	} else if (cmd->sensor_read_target == 2) {
		val = pin_get(&gpio_sc, PORT_D, 4) ? 0 : 1;
		printf("ok W:%d\n", val);
	}
}

static void
gcode_command_actuate(struct command *cmd)
{
	int cur;
	int val;

	val = cmd->actuate_value ? 1 : 0;

	switch (cmd->actuate_target) {
	case PNP_ACTUATE_TARGET_PUMP:
		pnp_henable(val);
		pin_set(&gpio_sc, PORT_B, 13, val);
		break;
	case PNP_ACTUATE_TARGET_AVAC1:
		pin_set(&gpio_sc, PORT_E, 2, val);
		break;
	case PNP_ACTUATE_TARGET_AVAC2:
		pin_set(&gpio_sc, PORT_E, 1, val);
		break;
	case PNP_ACTUATE_TARGET_NEEDLE:
		cur = pin_get(&gpio_sc, PORT_B, 5);
		if (cur && val) {
			printf("ERR: needle already set\n");
		} else if (!cur && !val) {
			printf("ERR: needle already cleared\n");
		} else {
			pin_set(&gpio_sc, PORT_E, 0, val);
			mdx_usleep(150000);

			if (val == 0) {
				/* Ensure it is not set. */
				do {
					cur = pin_get(&gpio_sc, PORT_B, 5);
					if (cur == 0)
						break;
					mdx_usleep(25000);
				} while (1);
			}
		}

		break;
	case PNP_ACTUATE_TARGET_PEEL:
		pin_set(&gpio_sc, PORT_B, 12, val);
		if (val)
			mdx_usleep(250000);
		break;
	default:
		break;
	}
}

static void
gcode_command(char *line, int len)
{
	struct command cmd;
	uint8_t letter;
	char *endp;
	char *end;
	float value;

#if 1
	int i;
	printf("GCODE: ");
	for (i = 0; i < len; i++)
		printf("%c", line[i]);
	printf("\n");
#endif

	bzero(&cmd, sizeof(struct command));

	end = line + len;
	while (line < end) {
		letter = *line;

		/* Skip spaces. */
		if (letter == ' ') {
			line += 1;
			continue;
		}

		if (letter < 'A' || letter > 'Z') {
			printf("Fatal error.\n");
			break;
		}

		/* Skip letter. */
		line += 1;

		/* Ensure we are dealing with digit, + or -. */
		if ((*line < '0' || *line > '9') &&
		    (*line != '+') &&
		    (*line != '-'))
			break;

		value = strtof(line, &endp);
		line = endp;

		printf("%s: value %.3f\n", __func__, value);

		switch (letter) {
		case 'M':
			if (value == 800.0f)
				cmd.type = CMD_TYPE_ACTUATE;
			else if (value == 105.0f)
				cmd.type = CMD_TYPE_SENSOR_READ;
			break;
		case 'G':
			if (value == 0.0f) /* Linear move. */
				cmd.type = CMD_TYPE_MOVE;
			break;
		case 'X':
			cmd.x = value * 1000000;
			cmd.x_set = 1;
			break;
		case 'Y':
			cmd.y = value * 1000000;
			cmd.y_set = 1;
			break;
		case 'Z':
			cmd.z = value * 1000000;
			cmd.z_set = 1;
			break;
		case 'I':
			cmd.h1 = value * 1000000;
			cmd.h1_set = 1;
			break;
		case 'J':
			cmd.h2 = value * 1000000;
			cmd.h2_set = 1;
			break;
		case 'P':
			cmd.actuate_target |= PNP_ACTUATE_TARGET_PUMP;
			cmd.actuate_value = value;
			break;
		case 'V':
			/* Air vacuum 1 */
			cmd.actuate_target |= PNP_ACTUATE_TARGET_AVAC1;
			cmd.actuate_value = value;
			break;
		case 'W':
			/* Air vacuum 2 */
			cmd.actuate_target |= PNP_ACTUATE_TARGET_AVAC2;
			cmd.actuate_value = value;
			break;
		case 'N':
			/* Air vac sensors read. */
			cmd.sensor_read_target = value;
			break;
		case 'D':
			/* Needle */
			cmd.actuate_target |= PNP_ACTUATE_TARGET_NEEDLE;
			cmd.actuate_value = value;
			break;
		case 'O':
			/* Peel */
			cmd.actuate_target |= PNP_ACTUATE_TARGET_PEEL;
			cmd.actuate_value = value;
			break;
		case 'F':
			break;
		default:
			break;
		}
	}

	printf("OK\n");

	switch (cmd.type) {
	case CMD_TYPE_MOVE:
		pnp_command_move(&cmd);
		break;
	case CMD_TYPE_ACTUATE:
		gcode_command_actuate(&cmd);
		break;
	case CMD_TYPE_SENSOR_READ:
		gcode_command_sensor_read(&cmd);
		break;
	};

	printf("COMPLETE\n");
}

static void
gcode_process_data(int ptr, int len)
{
	uint8_t *start;
	uint8_t ch;
	int i;

	start = &dma_buffer[ptr];
	for (i = 0; i < len; i++) {
		ch = start[i];
		dprintf("ch %d\n", ch);
		cmd_buffer[cmd_buffer_ptr] = ch;
		if (ch == '\n') { /* LF */
			gcode_command(cmd_buffer, cmd_buffer_ptr);
			cmd_buffer_ptr = 0;
		} else
			cmd_buffer_ptr += 1;
	}
}

static void
gcode_dmarecv_init(void)
{
	struct stm32f4_dma_conf conf;

	bzero(&conf, sizeof(struct stm32f4_dma_conf));
	conf.mem0 = (uintptr_t)dma_buffer;
	conf.sid = 2;
	conf.periph_addr = USART1_BASE + USART_DR;
	conf.dir = 0;
	conf.channel = 4;
	conf.circ = 1;
	conf.psize = 8;
	conf.nbytes = MAX_DMA_BUF_SIZE;

	stm32f4_dma_setup(&dma2_sc, &conf);
	stm32f4_dma_control(&dma2_sc, 2, 1);
}

int
gcode_mainloop(void)
{
	uint32_t cnt;
	int ptr;

	ptr = 0;
	cmd_buffer_ptr = 0;

	gcode_dmarecv_init();

	/* Periodically poll for a new data. */
	while (1) {
		cnt = stm32f4_dma_getcnt(&dma2_sc, 2);
		cnt = MAX_DMA_BUF_SIZE - cnt;

		if (cnt > ptr) {
			gcode_process_data(ptr, (cnt - ptr));
			ptr = cnt;
		} else if (cnt < ptr) {
			/* Buffer wrapped. */
			gcode_process_data(ptr, MAX_DMA_BUF_SIZE - ptr);
			gcode_process_data(0, cnt);
			ptr = cnt;
		}

		mdx_usleep(10000);
	}

	return (0);
}
