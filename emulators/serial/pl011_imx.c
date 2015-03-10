/**
 * Copyright (c) 2011 Anup Patel.
 * All rights reserved.
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
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
 * @file pl011_imx.c
 * @author Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * @brief PrimeCell PL011 serial emulator to i.MX6 UART.
 * @details This source file implements the PrimeCell PL011 serial emulator.
 *
 * The source has been largely adapted from QEMU 0.14.xx hw/pl011.c
 *
 * Arm PrimeCell PL011 UART emulator to write directly on i.MX6 UART one.
 *
 * Copyright (c) 2006 CodeSourcery.
 * Written by Paul Brook
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_chardev.h>
#include <vmm_devemu.h>
#include <vmm_host_io.h>
#include <vio/vmm_vserial.h>
#include <libs/fifo.h>
#include <libs/stringlib.h>
#include <drv/imx-uart.h>
#include <drv/pl011.h>

#define MODULE_DESC			"PL011 to i.MX Serial Emulator"
#define MODULE_AUTHOR			"Jimmy Durand Wesolowski"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_VSERIAL_IPRIORITY+1)
#define	MODULE_INIT			pl011_imx_emulator_init
#define	MODULE_EXIT			pl011_imx_emulator_exit

extern u32 imx_read(struct vmm_chardev *cdev,
		    u8 *dest, size_t len, off_t __unused *off, bool sleep);
extern u32 imx_write(struct vmm_chardev *cdev,
		     u8 *src, size_t len, off_t __unused *off, bool sleep);

static int pl011_imx_emulator_read(struct vmm_emudev *edev,
				   physical_addr_t offset,
				   u32 *dst)
{
	struct imx_port *port = edev->priv;

	if (UART_PL011_DR == offset) {
		*dst = vmm_readl((void *)(port->base + URXD0));
		vmm_printf("Read 0x%0x\n", *dst);
	} else if (UART_PL011_FR == offset) {
		u32 status = vmm_readl((void*)(port->base + IMX21_UTS));

		*dst = 0;
		if (status & UTS_RXEMPTY) {
			*dst |= UART_PL011_FR_RXFE;
		}
		if (status & UTS_TXFULL) {
			*dst |= UART_PL011_FR_TXFF;
		}
	}

	return VMM_OK;
}

static int pl011_imx_emulator_read8(struct vmm_emudev *edev,
				    physical_addr_t offset,
				    u8 *dst)
{
	u32 val = 0;
	int rc = VMM_OK;

	rc = pl011_imx_emulator_read(edev, offset, &val);
	if (VMM_OK == rc) {
		*dst = val & 0xFF;
	}

	return rc;
}

static int pl011_imx_emulator_read16(struct vmm_emudev *edev,
				     physical_addr_t offset,
				     u16 *dst)
{
	u32 val = 0;
	int rc = VMM_OK;

	rc = pl011_imx_emulator_read(edev, offset, &val);
	if (VMM_OK == rc) {
		*dst = val & 0xFFFF;
	}

	return rc;
}

static int pl011_imx_emulator_read32(struct vmm_emudev *edev,
				     physical_addr_t offset,
				     u32 *dst)
{
	u32 val = 0;
	int rc = VMM_OK;

	rc = pl011_imx_emulator_read(edev, offset, &val);
	if (VMM_OK == rc) {
		*dst = val & 0xFFFFFFFF;
	}

	return rc;
}


static int pl011_imx_emulator_write(struct vmm_emudev *edev,
				    physical_addr_t offset,
				    u32 src_mask,
				    u32 src)
{
	struct imx_port *port = edev->priv;

	if (UART_PL011_DR == offset) {
		vmm_writel(src, (void *)(port->base + URTX0));
	}

	return VMM_OK;
}

static int pl011_imx_emulator_write8(struct vmm_emudev *edev,
				     physical_addr_t offset,
				     u8 src)
{
	return pl011_imx_emulator_write(edev, offset, 0xFFFFFF00, src);
}

static int pl011_imx_emulator_write16(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u16 src)
{
	return pl011_imx_emulator_write(edev, offset, 0xFFFF0000, src);
}

static int pl011_imx_emulator_write32(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u32 src)
{
	return pl011_imx_emulator_write(edev, offset, 0x00000000, src);
}

static int pl011_imx_emulator_probe(struct vmm_guest *guest,
				    struct vmm_emudev *edev,
				    const struct vmm_devtree_nodeid *eid)
{
	u32 temp = 0;
	const char *huart = NULL;
	struct vmm_chardev *dev =  NULL;
	struct imx_port *port = NULL;
	int rc = VMM_OK;

	vmm_printf("%s\n", __FUNCTION__);
	huart = vmm_devtree_attrval(edev->node, "host_uart");
	if (NULL == huart) {
		vmm_printf("Cannot find host UART name to send on\n");
		return VMM_EFAIL;
	}

	dev = vmm_chardev_find(huart);
	if (NULL == dev) {
		vmm_printf("Cannot find host UART character device to "
			   "send on\n");
		return VMM_EFAIL;
	}

	/* Ensure this controller is an i.MX UART one */
	if ((&imx_read != dev->read) || (&imx_write != dev->write)) {
		vmm_printf("UART character device is not an i.MX "
			   "UART\n");
		return VMM_EFAIL;
	}

	port = dev->priv;
	/* Ensure it is configured */
	if (!port || !port->base) {
		vmm_printf("UART is not configured\n");
		return VMM_EFAIL;
	}

	/* Ensure it is up and running */
	if (!(vmm_readl((void *)(port->base + UCR2)) & UCR1_UARTEN)) {
		vmm_printf("UART is not enabled\n");
		return VMM_EFAIL;
	}

	temp = vmm_readl((void *)(port->base + UCR1));
	temp &= ~UCR1_RRDYEN;
	vmm_writel(temp, (void *)(port->base + UCR1));

	edev->priv = port;
	vmm_printf("PL011 to i.MX on %s registered\n", huart);

	return rc;
}

static int pl011_imx_emulator_reset(struct vmm_emudev *edev)
{
	vmm_printf("%s\n", __FUNCTION__);
	return VMM_OK;
}

static int pl011_imx_emulator_remove(struct vmm_emudev *edev)
{
	edev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid pl011_imx_emuid_table[] = {
	{ .type = "serial",
	  .compatible = "pl011_imx",
	},
	{ /* end of list */ },
};

static struct vmm_emulator pl011_imx_emulator = {
	.name = "pl011_imx",
	.match_table = pl011_imx_emuid_table,
	.endian = VMM_DEVEMU_LITTLE_ENDIAN,
	.probe = pl011_imx_emulator_probe,
	.read8 = pl011_imx_emulator_read8,
	.write8 = pl011_imx_emulator_write8,
	.read16 = pl011_imx_emulator_read16,
	.write16 = pl011_imx_emulator_write16,
	.read32 = pl011_imx_emulator_read32,
	.write32 = pl011_imx_emulator_write32,
	.reset = pl011_imx_emulator_reset,
	.remove = pl011_imx_emulator_remove,
};

static int __init pl011_imx_emulator_init(void)
{
	return vmm_devemu_register_emulator(&pl011_imx_emulator);
}

static void __exit pl011_imx_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&pl011_imx_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
