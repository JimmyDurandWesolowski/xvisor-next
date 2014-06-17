/**
 * Copyright (c) 2014 OpenWide.
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
 * This file is inspired from the zero read-only memory emulator,
 * zero.c from Anup Patel (Copyright (c) 2012).
 *
 * @file gshmem.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Guest memory exchange emulator.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vmm_manager.h>

#define MODULE_DESC			"Guest Memory Exchange Emulator"
#define MODULE_AUTHOR			"Jimmy Durand Wesolowski"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			gshmem_emulator_init
#define	MODULE_EXIT			gshmem_emulator_exit

#define OFF_GUEST_IRQ			0


typedef struct gshmem_info_t {
	u32 irq;
	struct vmm_guest *guest_src;
	struct vmm_guest *guest_dst;
} gshmem_info_t;


static int gshmem_emulator_read(struct vmm_emudev *edev,
				physical_addr_t offset,
				u32 *dst)
{
	gshmem_info_t *info = edev->priv;

	switch (offset)
	{
	case OFF_GUEST_IRQ:
		*dst = info->guest_dst->id;
		break;
	default:
		return VMM_EINVALID;
	}

	return VMM_OK;
}

static int gshmem_emulator_write(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u32 src)
{
	gshmem_info_t *info = edev->priv;
	struct vmm_guest *guest = NULL;

	vmm_printf("VMM: Writing %u\n", src);

	if (NULL == (guest = vmm_manager_guest(src)))
	{
		vmm_printf("Invalid guest id: %u\n");
		return VMM_OK;
	}
	info->guest_dst = guest;

	vmm_printf("VMM: Sending IRQ #%u to guest %s\n", info->irq,
		   guest->name);
	vmm_devemu_emulate_irq(guest, info->irq, 1);

	return VMM_OK;
}

static int gshmem_emulator_read8(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u8 *dst)
{
	u32 dst32 = 0;

	gshmem_emulator_read(edev, offset, &dst32);
	*dst = dst32 & 0xFF;
	return VMM_OK;
}

static int gshmem_emulator_read16(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u16 *dst)
{
	u32 dst32 = 0;

	gshmem_emulator_read(edev, offset, &dst32);
	*dst = dst32 & 0xFF;
	return VMM_OK;
}


static int gshmem_emulator_read32(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u32 *dst)
{
	gshmem_emulator_read(edev, offset, dst);
	return VMM_OK;
}

static int gshmem_emulator_write8(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u8 src)
{
	gshmem_emulator_write(edev, offset, src);
	return VMM_OK;
}

static int gshmem_emulator_write16(struct vmm_emudev *edev,
				   physical_addr_t offset,
				   u16 src)
{
	gshmem_emulator_write(edev, offset, src);
	return VMM_OK;
}

static int gshmem_emulator_write32(struct vmm_emudev *edev,
				   physical_addr_t offset,
				   u32 src)
{
	gshmem_emulator_write(edev, offset, src);
	return VMM_OK;
}

static int gshmem_emulator_reset(struct vmm_emudev *edev)
{
	return VMM_OK;
}

static int gshmem_emulator_probe(struct vmm_guest *guest,
				 struct vmm_emudev *edev,
				 const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	gshmem_info_t *info = NULL;

	/* TODO: It can checks if there is a physical shared memory */
	if (NULL == (info = vmm_zalloc(sizeof (gshmem_info_t))))
		return VMM_ENOMEM;

	if (0 != (rc = vmm_devtree_irq_get(edev->node, &info->irq, 0)))
	{
		vmm_printf("Failed to get gshmem IRQ #\n");
		return rc;
	}

	info->guest_src = guest;
	edev->priv = info;

	return VMM_OK;
}

static int gshmem_emulator_remove(struct vmm_emudev *edev)
{
	vmm_free(edev->priv);
	return VMM_OK;
}

static struct vmm_devtree_nodeid gshmem_emuid_table[] = {
	{ .type = "misc",
	  .compatible = "gshmem",
	},
	{ /* end of list */ },
};

static struct vmm_emulator gshmem_emulator = {
	.name = "gshmem",
	.match_table = gshmem_emuid_table,
	.endian = VMM_DEVEMU_NATIVE_ENDIAN,
	.probe = gshmem_emulator_probe,
	.read8 = gshmem_emulator_read8,
	.write8 = gshmem_emulator_write8,
	.read16 = gshmem_emulator_read16,
	.write16 = gshmem_emulator_write16,
	.read32 = gshmem_emulator_read32,
	.write32 = gshmem_emulator_write32,
	.reset = gshmem_emulator_reset,
	.remove = gshmem_emulator_remove,
};

static int __init gshmem_emulator_init(void)
{
	return vmm_devemu_register_emulator(&gshmem_emulator);
}

static void __exit gshmem_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&gshmem_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
