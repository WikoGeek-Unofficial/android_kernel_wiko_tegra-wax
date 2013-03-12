/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/io.h>

#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/gpio-tegra.h>
#include "board.h"
#include "tegra-board-id.h"
#include "gpio-names.h"

static int tegra_gpio_wakes[] = {
	TEGRA_GPIO_PL0,				/* wake0 */
	TEGRA_GPIO_PL2,				/* wake1 */
	TEGRA_GPIO_PM2,				/* wake2 */
	-EINVAL,				/* wake3 */
	TEGRA_GPIO_PM4,				/* wake4 */
	TEGRA_GPIO_PM7,				/* wake5 */
	TEGRA_GPIO_PN1,				/* wake6 */
	TEGRA_GPIO_PO0,				/* wake7 */
	TEGRA_GPIO_PO1,				/* wake8 */
	TEGRA_GPIO_PO2,				/* wake9 */
	-EINVAL,				/* wake10 */
	TEGRA_GPIO_PO3,				/* wake11 */
	TEGRA_GPIO_PO4,				/* wake12 */
	-EINVAL,				/* wake13 */
	TEGRA_GPIO_PO5,				/* wake14 */
	TEGRA_GPIO_PO6,				/* wake15 */
	-EINVAL,				/* wake16 */
	-EINVAL,				/* wake17 */
	-EINVAL,				/* wake18 */
	-EINVAL,				/* wake19 */
	-EINVAL,				/* wake20 */
	-EINVAL,				/* wake21 */
	-EINVAL,				/* wake22 */
	TEGRA_GPIO_PJ5,				/* wake23 */
	TEGRA_GPIO_PJ6,				/* wake24 */
	TEGRA_GPIO_PJ1,				/* wake25 */
	TEGRA_GPIO_PJ2,				/* wake26 */
	TEGRA_GPIO_PJ3,				/* wake27 */
	TEGRA_GPIO_PJ4,				/* wake28 */
	-EINVAL,				/* wake29 */
	-EINVAL,				/* wake30 */
	-EINVAL,				/* wake31 */
	-EINVAL,				/* wake32 */
	TEGRA_GPIO_PJ0,				/* wake33 */
	TEGRA_GPIO_PK2,				/* wake34 */
	TEGRA_GPIO_PI6,				/* wake35 */
	-EINVAL,				/* wake36 */
	-EINVAL,				/* wake37 */
	-EINVAL,				/* wake38 */
	-EINVAL,				/* wake39 */
	-EINVAL,				/* wake40 */
	-EINVAL,				/* wake41 */
	-EINVAL,				/* wake42 */
	-EINVAL,				/* wake43 */
	-EINVAL,				/* wake44 */
	TEGRA_GPIO_PBB6,			/* wake45 */
	-EINVAL,				/* wake46 */
	-EINVAL,				/* wake47 */
	-EINVAL,				/* wake48 */
	TEGRA_GPIO_PR7,				/* wake49 */
	TEGRA_GPIO_PR4,				/* wake50 */
	-EINVAL,				/* wake51 */
	-EINVAL,				/* wake52 */
	-EINVAL,				/* wake53 */
	TEGRA_GPIO_PQ5,				/* wake54 */
	-EINVAL,				/* wake55 */
};

static int tegra_wake_event_irq[] = {
	-EAGAIN, /* SPI3_MOSI */		/* wake0 */
	-EAGAIN, /* SPI3_SCK */			/* wake1 */
	-EAGAIN, /* BT_WAKE_AP */		/* wake2 */
	INT_SDMMC3, /* SDMMC3 DAT1 */		/* wake3 */
	-EAGAIN, /* NFC_INT_L */		/* wake4 */
	-EAGAIN, /* MOTION_INT_L */		/* wake5 */
	-EAGAIN, /* TOUCH_INT_L */		/* wake6 */
	-EAGAIN,				/* wake7 */
	-EAGAIN,				/* wake8 */
	INT_UARTC, /* UART3 RXD */		/* wake9 */
	INT_SDMMC4, /* SDMMC4 DAT1 */		/* wake10 */
	-EAGAIN,				/* wake11 */
	-EAGAIN,				/* wake12 */
	INT_SDMMC1, /* SDMMC1 DAT1 */		/* wake13 */
	-EAGAIN,				/* wake14 */
	INT_THERMAL,				/* wake15 */
	INT_RTC,				/* wake16 */
	INT_KBC,				/* wake17 */
	INT_EXTERNAL_PMU,			/* wake18 */
	-EINVAL, /* removed USB1 VBUS wake */	/* wake19 */
	-EINVAL, /* removed USB2 VBUS wake */	/* wake20 */
	-EINVAL, /* removed USB1 ID wake */	/* wake21 */
	-EINVAL, /* removed USB2 ID wake */	/* wake22 */
	-EAGAIN,				/* wake23 */
	-EAGAIN,				/* wake24 */
	-EAGAIN, /* KB_ROW0 */			/* wake25 */
	-EAGAIN, /* KB_ROW1 */			/* wake26 */
	-EAGAIN, /* KB_ROW2 */			/* wake27 */
	-EAGAIN, /* KB_COL0 */			/* wake28 */
	INT_MIPI_BIF, /* BCL */			/* wake29 */
	INT_AUDIO_CLUSTER, /* I2S0 SDATA OUT */	/* wake30 */
	-EINVAL,				/* wake31 */
	-EINVAL, /* ULPI DATA3 */		/* wake32 */
	-EAGAIN,				/* wake33 */
	-EAGAIN,				/* wake34 */
	-EAGAIN,				/* wake35 */
	-EAGAIN,				/* wake36 */
	-EINVAL, /* removed USB3 VBUS wake */	/* wake37 */
	-EINVAL, /* removed USB3 ID wake */	/* wake38 */
	INT_USB, /* USB1 UTMIP */		/* wake39 */
	-EINVAL, /* removed USB2 UTMIP wake */	/* wake40 */
	-EINVAL, /* removed USB3 UTMIP wake */	/* wake41 */
	INT_USB2, /* USB2 UHSIC PHY */		/* wake42 */
	-EINVAL, /* removed USB3 UHSIC PHY wake */	/* wake43 */
	INT_I2C, /* I2C1 DAT */			/* wake44 */
	-EAGAIN,				/* wake45 */
	INT_I2C5, /* PWR I2C DAT */		/* wake46 */
	INT_I2C2, /* I2C2 DAT */		/* wake47 */
	INT_I2C3, /* I2C3 DAT */		/* wake48 */
	-EAGAIN,				/* wake49 */
	-EAGAIN,				/* wake50 */
	INT_KBC, /* KBC11 */			/* wake51 */
	INT_HDMI, /* HDMI CEC */		/* wake52 */
	INT_I2C3, /* I2C3 CLK */		/* wake53 */
	-EAGAIN,				/* wake54 */
	INT_UARTC, /* UART3 CTS */		/* wake55 */
	-EAGAIN,				/* wake56 */
	-EAGAIN,				/* wake57 */
	-EAGAIN,				/* wake58 */
	INT_BB2AP_INT0,				/* wake59 */
	-EAGAIN,				/* wake60 */
	-EAGAIN,				/* wake61 */
	-EAGAIN,				/* wake62 */
	-EAGAIN,				/* wake63 */
};

static int last_gpio = -1;

int tegra_gpio_to_wake(int gpio)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tegra_gpio_wakes); i++) {
		if (tegra_gpio_wakes[i] == gpio) {
			pr_info("gpio wake%d for gpio=%d\n", i, gpio);
			last_gpio = i;
			return i;
		}
	}

	return -EINVAL;
}

int tegra_irq_to_wake(int irq)
{
	int i;
	int ret = -EINVAL;

	for (i = 0; i < ARRAY_SIZE(tegra_wake_event_irq); i++) {
		if (tegra_wake_event_irq[i] == irq) {
			pr_info("Wake%d for irq=%d\n", i, irq);
			ret = i;
			goto out;
		}
	}

	/* The gpio set_wake code bubbles the set_wake call up to the irq
	 * set_wake code. This insures that the nested irq set_wake call
	 * succeeds, even though it doesn't have to do any pm setup for the
	 * bank.
	 *
	 * This is very fragile - there's no locking, so two callers could
	 * cause issues with this.
	 */
	if (last_gpio < 0)
		goto out;

	if (tegra_gpio_get_bank_int_nr(tegra_gpio_wakes[last_gpio]) == irq) {
		pr_info("gpio bank wake found: wake%d for irq=%d\n", i, irq);
		ret = last_gpio;
	}

out:
	return ret;
}

int tegra_wake_to_irq(int wake)
{
	int ret;

	if (wake < 0)
		return -EINVAL;

	if (wake >= ARRAY_SIZE(tegra_wake_event_irq))
		return -EINVAL;

	ret = tegra_wake_event_irq[wake];
	if (ret == -EAGAIN) {
		ret = tegra_gpio_wakes[wake];
		if (ret != -EINVAL)
			ret = gpio_to_irq(ret);
	}

	return ret;
}

int tegra_disable_wake_source(int wake)
{
	if (wake >= ARRAY_SIZE(tegra_wake_event_irq))
		return -EINVAL;

	tegra_wake_event_irq[wake] = -EINVAL;
	return 0;
}
