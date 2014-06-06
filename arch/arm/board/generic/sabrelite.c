/**
 * Copyright (c) 2014 Jimmy Durand Wesolowski
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file imx6.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Freescale i.MX6 board specific code
 *
 * Adapted from linux/drivers/mfd/vexpres-sysreg.c
 *
 * Copyright (c) 2014 Anup Patel.
 *
 * The original source is licensed under GPL.
 *
 */
#include <vmm_error.h>
#include <vmm_devtree.h>
#include <vmm_host_io.h>
#include <vmm_main.h>

#include <generic_board.h>

/*
 * Initialization functions
 */

#define IMX2_WDT_WCR		0x00		/* Control Register */
#define IMX2_WDT_WCR_WT		(0xFF << 8)	/* -> Watchdog Timeout Field */
#define IMX2_WDT_WCR_WRE	(1 << 3)	/* -> WDOG Reset Enable */
#define IMX2_WDT_WCR_WDE	(1 << 2)	/* -> Watchdog Enable */

#define IMX2_WDT_WSR		0x02		/* Service Register */
#define IMX2_WDT_SEQ1		0x5555		/* -> service sequence 1 */
#define IMX2_WDT_SEQ2		0xAAAA		/* -> service sequence 2 */

#define IMX2_WDT_WRSR		0x04		/* Reset Status Register */
#define IMX2_WDT_WRSR_TOUT	(1 << 1)	/* -> Reset due to Timeout */

#define IMX2_WDT_MAX_TIME	128
#define IMX2_WDT_DEFAULT_TIME	60		/* in seconds */

#define WDOG_SEC_TO_COUNT(s)	((s * 2 - 1) << 8)

#define IMX2_WDT_STATUS_OPEN	0
#define IMX2_WDT_STATUS_STARTED	1
#define IMX2_WDT_EXPECT_CLOSE	2

static inline void imx2_wdt_setup(virtual_addr_t wdog_base, u8 timeout)
{
	u16 val = vmm_readw((void*)(wdog_base + IMX2_WDT_WCR));

	/* Strip the old watchdog Time-Out value */
	val &= ~IMX2_WDT_WCR_WT;
	/* Generate reset if WDOG times out */
	val &= ~IMX2_WDT_WCR_WRE;
	/* Keep Watchdog Disabled */
	val &= ~IMX2_WDT_WCR_WDE;
	/* Set the watchdog's Time-Out value */
	val |= WDOG_SEC_TO_COUNT(timeout);

	vmm_writew(val, (void*)(wdog_base + IMX2_WDT_WCR));

	/* enable the watchdog */
	val |= IMX2_WDT_WCR_WDE;
	vmm_writew(val, (void*)(wdog_base + IMX2_WDT_WCR));
}

static inline void imx2_wdt_ping(virtual_addr_t wdog_base)
{
	vmm_writew(IMX2_WDT_SEQ1, (void*)(wdog_base + IMX2_WDT_WSR));
	vmm_writew(IMX2_WDT_SEQ2, (void*)(wdog_base + IMX2_WDT_WSR));
}

static int imx_reset(struct vmm_devtree_node *node)
{
	int rc = 0;
	virtual_addr_t wdog_base = 0;

	/* TODO: Create a watchdog driver */
	/* Determine the watchdog register map */
	node = vmm_devtree_find_compatible(NULL, NULL, "fsl,imx6q-wdt");
	if (!node) {
		return VMM_ENODEV;
	}

	rc = vmm_devtree_regmap(node, &wdog_base, 0);
	if (rc) {
		return rc;
	}

	imx2_wdt_setup(wdog_base, 0);
	imx2_wdt_ping(wdog_base);

	return VMM_OK;
}

static int __init imx6_early_init(struct vmm_devtree_node *node)
{
	/* /\* Sysreg early init *\/ */
	/* imx6_sysreg_of_early_init(); */

	/* /\* Determine dvimode function *\/ */
	/* node = vmm_devtree_find_compatible(NULL, NULL, "arm,imx6-dvimode"); */
	/* if (!node) { */
	/* 	return VMM_ENODEV; */
	/* } */
	/* dvimode_func = imx6_config_func_get_by_node(node); */
	/* if (!dvimode_func) { */
	/* 	return VMM_ENODEV; */
	/* } */

	/* /\* Setup CLCD (before probing) *\/ */
	/* node = vmm_devtree_find_compatible(NULL, NULL, "arm,pl111"); */
	/* if (node) { */
	/* 	node->system_data = &clcd_system_data; */
	/* } */
	vmm_register_system_reset(imx_reset);

	return 0;
}

static int __init imx6_final_init(struct vmm_devtree_node *node)
{
	/* Nothing to do here. */
	return VMM_OK;
}

static struct generic_board imx6_info = {
	.name		= "iMX6",
	.early_init	= imx6_early_init,
	.final_init	= imx6_final_init,
};

GENERIC_BOARD_DECLARE(imx6, "arm,imx6q", &imx6_info);
