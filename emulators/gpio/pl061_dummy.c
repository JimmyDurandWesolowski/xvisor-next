/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file pl061_dummy.c
 * @author Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * @brief Fake PrimeCell PL061 GPIO Controller Emulator.
 *
 * The source has been largely adapted from QEMU 0.14.xx hw/pl061.c
 *
 * This is a dummy version of Arm PrimeCell PL061 General Purpose IO
 * with additional Luminary Micro Stellaris bits.
 *
 * Copyright (c) 2007 CodeSourcery.
 * Written by Paul Brook
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <drv/input.h>

#define MODULE_DESC			"Dummy PL061 GPIO Emulator"
#define MODULE_AUTHOR			"Jimmy Durand Wesolowski"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			pl061_emulator_init
#define	MODULE_EXIT			pl061_emulator_exit

struct pl061_state {
	struct vmm_guest *guest;
	vmm_spinlock_t lock;

	u8 id[12];
	u32 locked;
	u32 data;
	u32 old_data;
	u32 dir;
	u32 isense;
	u32 ibe;
	u32 iev;
	u32 im;
	u32 istate;
	u32 afsel;
	u32 dr2r;
	u32 dr4r;
	u32 dr8r;
	u32 odr;
	u32 pur;
	u32 pdr;
	u32 slr;
	u32 den;
	u32 cr;
	u32 float_high;
	u32 amsel;

	u32 irq;
	struct input_handler hndl;
};

static int pl061_reg_read(struct pl061_state *s,
			  u32 offset, u32 *dst)
{
	int rc = VMM_OK;

	vmm_spin_lock(&s->lock);

	if (offset >= 0xfd0 && offset < 0x1000) {
		*dst = *((u32 *)&s->id[(offset - 0xfd0) >> 2]);
	} else if (offset < 0x400) {
		*dst = s->data & (offset >> 2);
	} else {
		switch (offset & ~0x3) {
		case 0x400: /* Direction */
			*dst = s->dir;
			break;
		case 0x404: /* Interrupt sense */
			*dst = s->isense;
			break;
		case 0x408: /* Interrupt both edges */
			*dst = s->ibe;
			break;
		case 0x40c: /* Interrupt event */
			*dst = s->iev;
			break;
		case 0x410: /* Interrupt mask */
			*dst = s->im;
			break;
		case 0x414: /* Raw interrupt status */
			*dst = s->istate;
			break;
		case 0x418: /* Masked interrupt status */
			*dst = s->istate | s->im;
			break;
		case 0x420: /* Alternate function select */
			*dst = s->afsel;
			break;
		case 0x500: /* 2mA drive */
			*dst = s->dr2r;
			break;
		case 0x504: /* 4mA drive */
			*dst = s->dr4r;
			break;
		case 0x508: /* 8mA drive */
			*dst = s->dr8r;
			break;
		case 0x50c: /* Open drain */
			*dst = s->odr;
			break;
		case 0x510: /* Pull-up */
			*dst = s->pur;
			break;
		case 0x514: /* Pull-down */
			*dst = s->pdr;
			break;
		case 0x518: /* Slew rate control */
			*dst = s->slr;
			break;
		case 0x51c: /* Digital enable */
			*dst = s->den;
			break;
		case 0x520: /* Lock */
			*dst = s->locked;
			break;
		case 0x524: /* Commit */
			*dst = s->cr;
			break;
		case 0x528: /* Analog mode select */
			*dst = s->amsel;
			break;
		default:
			rc = VMM_EFAIL;
			break;
		};
	}

	vmm_spin_unlock(&s->lock);

	return rc;
}

static int pl061_reg_write(struct pl061_state *s,
			   u32 offset, u32 regmask, u32 regval)
{
	u8 mask;
	int rc = VMM_OK;

	vmm_spin_lock(&s->lock);

	if (offset < 0x400) {
		mask = (offset >> 2) & s->dir;
		s->data = (s->data & ~mask) | (regval & mask);
	} else {
		switch (offset & ~0x3) {
		case 0x404: /* Interrupt sense */
			s->isense &= regmask;
			s->isense |= (regval & 0xFF);
			break;
		case 0x408: /* Interrupt both edges */
			s->ibe &= regmask;
			s->ibe |= (regval & 0xFF);
			break;
		case 0x40c: /* Interrupt event */
			s->iev &= regmask;
			s->iev |= (regval & 0xFF);
			break;
		case 0x410: /* Interrupt mask */
			s->im &= regmask;
			s->im |= (regval & 0xFF);
			if (s->im) {
				vmm_printf("INT unmasked\n");
			} else {
				vmm_printf("INT masked\n");
			}
			break;
		case 0x41C: /* Interrupt clear */
			if (s->im) {
				vmm_printf("INT ack\n");
			}
			vmm_devemu_emulate_irq(s->guest, s->irq, 0);
			break;
		default:
			break;
		}
	}
	vmm_spin_unlock(&s->lock);

	return rc;
}

static int pl061_emulator_read8(struct vmm_emudev *edev,
				physical_addr_t offset,
				u8 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = pl061_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFF;
	}

	return rc;
}

static int pl061_emulator_read16(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u16 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = pl061_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFFFF;
	}

	return rc;
}

static int pl061_emulator_read32(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u32 *dst)
{
	return pl061_reg_read(edev->priv, offset, dst);
}

static int pl061_emulator_write8(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u8 src)
{
	return pl061_reg_write(edev->priv, offset, 0xFFFFFF00, src);
}

static int pl061_emulator_write16(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u16 src)
{
	return pl061_reg_write(edev->priv, offset, 0xFFFF0000, src);
}

static int pl061_emulator_write32(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u32 src)
{
	return pl061_reg_write(edev->priv, offset, 0x00000000, src);
}

static int pl061_emulator_reset(struct vmm_emudev *edev)
{
	struct pl061_state *s = edev->priv;

	vmm_spin_lock(&s->lock);

	s->locked = 1;
	s->cr = 0xff;

	vmm_spin_unlock(&s->lock);

	return VMM_OK;
}

static int pl061_emulator_abs_event(struct input_handler *ihnd,
				    struct input_dev *idev,
				    unsigned int type, unsigned int code,
				    int value)
{
	struct pl061_state *s = ihnd->priv;

	if (idev->id.bustype != 0x18)
		return VMM_OK;
	vmm_spin_lock(&s->lock);
	if (s->im) {
		vmm_printf("send IRQ %d to guest [%d]\n", s->irq, value);
		vmm_devemu_emulate_irq(s->guest, s->irq, 1);
		if (value)
			s->data |= 1;
		else
			s->data &= ~1;
	}
	vmm_spin_unlock(&s->lock);

	return VMM_OK;
}

static int pl061_emulator_probe(struct vmm_guest *guest,
				struct vmm_emudev *edev,
				const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	struct pl061_state *s;

	s = vmm_zalloc(sizeof(struct pl061_state));
	if (!s) {
		rc = VMM_EFAIL;
		goto pl061_emulator_probe_done;
	}

	if (eid->data) {
		s->id[0] = ((const u8 *)eid->data)[0];
		s->id[1] = ((const u8 *)eid->data)[1];
		s->id[2] = ((const u8 *)eid->data)[2];
		s->id[3] = ((const u8 *)eid->data)[3];
		s->id[4] = ((const u8 *)eid->data)[4];
		s->id[5] = ((const u8 *)eid->data)[5];
		s->id[6] = ((const u8 *)eid->data)[6];
		s->id[7] = ((const u8 *)eid->data)[7];
		s->id[8] = ((const u8 *)eid->data)[8];
		s->id[9] = ((const u8 *)eid->data)[9];
		s->id[10] = ((const u8 *)eid->data)[10];
		s->id[11] = ((const u8 *)eid->data)[11];
	}

	rc = vmm_devtree_irq_get(edev->node, &s->irq, 0);
	if (rc) {
		goto pl061_emulator_probe_freestate_failed;
	}
	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);

	/* Setup input handler */
	s->hndl.name = edev->emu->name;
	s->hndl.evbit[0] |= BIT_MASK(EV_KEY);
	s->hndl.event = pl061_emulator_abs_event;
	s->hndl.priv = s;
	if (input_register_handler(&s->hndl)) {
		goto pl061_emulator_probe_freestate_failed;
	}

	/* Connect input handler */
	if (input_connect_handler(&s->hndl)) {
		goto pl061_emulator_probe_unregister_handler;
	}

	edev->priv = s;

	return rc;

pl061_emulator_probe_unregister_handler:
	input_unregister_handler(&s->hndl);
pl061_emulator_probe_freestate_failed:
	vmm_free(s);
pl061_emulator_probe_done:
	return rc;
}

static int pl061_emulator_remove(struct vmm_emudev *edev)
{
	struct pl061_state *s = edev->priv;

	if (s) {
		input_unregister_handler(&s->hndl);
		vmm_free(s);
		edev->priv = NULL;
	}

	return VMM_OK;
}

static u8 pl061_id[12] =
  { 0x00, 0x00, 0x00, 0x00, 0x61, 0x10, 0x04, 0x00, 0x0d, 0xf0, 0x05, 0xb1 };

static struct vmm_devtree_nodeid pl061_emuid_table[] = {
	{
		.type = "gpio",
		.compatible = "primecell,pl061-dummy",
		.data = (void *)&pl061_id,
	},
	{ /* end of list */ },
};

static struct vmm_emulator pl061_emulator = {
	.name = "pl061",
	.match_table = pl061_emuid_table,
	.endian = VMM_DEVEMU_LITTLE_ENDIAN,
	.probe = pl061_emulator_probe,
	.read8 = pl061_emulator_read8,
	.write8 = pl061_emulator_write8,
	.read16 = pl061_emulator_read16,
	.write16 = pl061_emulator_write16,
	.read32 = pl061_emulator_read32,
	.write32 = pl061_emulator_write32,
	.reset = pl061_emulator_reset,
	.remove = pl061_emulator_remove,
};

static int __init pl061_emulator_init(void)
{
	return vmm_devemu_register_emulator(&pl061_emulator);
}

static void __exit pl061_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&pl061_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
