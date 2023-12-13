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
#include <sys/thread.h>

#include <dev/display/panel.h>
#include <dev/display/dsi.h>
#include <dev/intc/intc.h>

#include <arm/stm/stm32f4.h>
#include <arm/arm/nvic.h>

#include "board.h"
#include "gpio.h"

struct stm32f4_usart_softc usart_sc;
struct stm32f4_gpio_softc gpio_sc;
struct stm32f4_flash_softc flash_sc;
struct stm32f4_pwr_softc pwr_sc;
struct stm32f4_rcc_softc rcc_sc;
struct stm32f4_timer_softc timer_sc;

struct arm_nvic_softc nvic_sc;
struct mdx_device dev_nvic = { .sc = &nvic_sc };

struct stm32f4_pwm_softc pwm_x_sc;

void
udelay(uint32_t usec)
{
	int i;

	/* TODO: implement me */

	for (i = 0; i < usec * 42; i++)
		;
}

void
usleep(uint32_t usec)
{

	mdx_usleep(usec);
}

static void
uart_putchar(int c, void *arg)
{
	struct stm32f4_usart_softc *sc;
 
	sc = arg;
 
	if (c == '\n')
		stm32f4_usart_putc(sc, '\r');

	stm32f4_usart_putc(sc, c);
}

void
board_init(void)
{
	struct stm32f4_rcc_pll_conf pconf;
	uint32_t reg;

	stm32f4_flash_init(&flash_sc, FLASH_BASE);
	stm32f4_rcc_init(&rcc_sc, RCC_BASE);
	stm32f4_pwr_init(&pwr_sc, PWR_BASE);

	pconf.pllm = 16;
	pconf.plln = 336;
	pconf.pllq = 7;
	pconf.pllp = 0;
	pconf.external = 1;
	pconf.rcc_cfgr = (CFGR_PPRE2_2 | CFGR_PPRE1_4);
	stm32f4_rcc_pll_configure(&rcc_sc, &pconf);

	stm32f4_flash_setup(&flash_sc);
	reg = (GPIOAEN | GPIOBEN | GPIOCEN | GPIODEN | GPIOEEN);
	stm32f4_rcc_setup(&rcc_sc, reg, 0, 0, (TIM14EN),
	    (TIM1EN | TIM8EN | TIM10EN | USART1EN));
	stm32f4_gpio_init(&gpio_sc, GPIO_BASE);
	gpio_config(&gpio_sc);

	stm32f4_usart_init(&usart_sc, USART1_BASE, 84000000, 115200);
	mdx_console_register(uart_putchar, (void *)&usart_sc);

	printf("Mdepx started\n");

	/* (168 / PPRE2_2) * 2 = 168MHz */
	stm32f4_timer_init(&timer_sc, TIM8_BASE, 168000000);
	arm_nvic_init(&dev_nvic, NVIC_BASE);

	/* TIM 1 */
	mdx_intc_setup(&dev_nvic, 27, stm32f4_pwm_intr, &pwm_x_sc);
	mdx_intc_enable(&dev_nvic, 27);

	/* TIM 8 */
	mdx_intc_setup(&dev_nvic, 46, stm32f4_timer_intr, &timer_sc);
	mdx_intc_enable(&dev_nvic, 46);

	/* TIM 10 */
	mdx_intc_setup(&dev_nvic, 25, stm32f4_pwm_intr, &pwm_x_sc);
	mdx_intc_enable(&dev_nvic, 25);
}
