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

#ifndef _SRC_BOARD_H_
#define	_SRC_BOARD_H_

#define	MALLOC_REGION_START	0x20010000
#define	MALLOC_REGION_SIZE	0x00010000 /* 64kb */

extern struct stm32f4_dma_softc dma1_sc;
extern struct stm32f4_dma_softc dma2_sc;
extern struct stm32f4_gpio_softc gpio_sc;
extern struct stm32f4_pwm_softc pwm_x_sc;
extern struct stm32f4_pwm_softc pwm_y_sc;
extern struct stm32f4_pwm_softc pwm_z_sc;
extern struct stm32f4_pwm_softc pwm_h1_sc;
extern struct stm32f4_pwm_softc pwm_h2_sc;

uint32_t board_get_random(void);

#endif /* !_SRC_BOARD_H_ */
