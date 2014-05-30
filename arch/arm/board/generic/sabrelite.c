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

#include <generic_board.h>

/*
 * Initialization functions
 */

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
