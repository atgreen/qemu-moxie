/*
 * Interrupt Controller for the Moxie Marin SoC
 *
 * Copyright (C) 2014 Anthony Green
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation, or any later version.
 * See the COPYING file in the top-level directory.
 */
#include "hw/sysbus.h"

#define TYPE_MARIN_INTC "marin_intc"
#define MARIN_INTC(obj) OBJECT_CHECK(MarinINTCState, (obj), TYPE_MARIN_INTC)

typedef struct MarinINTCState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq parent_irq;

    uint16_t irq_state;
} MarinINTCState;

/* Update interrupt status after enabled or pending bits have been changed.  */
static void marin_intc_update(MarinINTCState *s)
{
  qemu_irq_raise(s->parent_irq);
}

/* Process a change in an external INTC input. */
static void marin_intc_handler(void *opaque, int irq, int level)
{
    MarinINTCState *s = opaque;

    s->irq_state |= (1 << irq);
    marin_intc_update(s);
}

static uint64_t marin_intc_read(void *opaque, hwaddr offset,
        unsigned size)
{
    MarinINTCState *s = opaque;

    return s->irq_state;
}

static void marin_intc_write(void *opaque, hwaddr offset,
        uint64_t value, unsigned size)
{
    MarinINTCState *s = opaque;

    s->irq_state ^= (s->irq_state & value);

    marin_intc_update(s);
}

static const MemoryRegionOps marin_intc_ops = {
    .read = marin_intc_read,
    .write = marin_intc_write,
    .impl = {
       .min_access_size = 2,
       .max_access_size = 2,
    },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static int marin_intc_init(SysBusDevice *sbd)
{
    DeviceState *dev = DEVICE(sbd);
    MarinINTCState *s = MARIN_INTC(dev);

    qdev_init_gpio_in(dev, marin_intc_handler, 4);
    sysbus_init_irq(sbd, &s->parent_irq);

    memory_region_init_io(&s->iomem, OBJECT(s), 
			  &marin_intc_ops, s, 
			  TYPE_MARIN_INTC, 2);
    sysbus_init_mmio(sbd, &s->iomem);

    return 0;
}

static void marin_intc_reset(DeviceState *d)
{
    MarinINTCState *s = MARIN_INTC(d);
    s->irq_state = 0;
}

static const VMStateDescription vmstate_marin_intc = {
    .name = TYPE_MARIN_INTC,
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT16(irq_state, MarinINTCState),
        VMSTATE_END_OF_LIST()
    }
};

static void marin_intc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);

    sdc->init = marin_intc_init;
    dc->reset = marin_intc_reset;
    dc->vmsd = &vmstate_marin_intc;
}

static const TypeInfo marin_intc_info = {
    .name = TYPE_MARIN_INTC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MarinINTCState),
    .class_init = marin_intc_class_init,
};

static void marin_intc_register_type(void)
{
    type_register_static(&marin_intc_info);
}

type_init(marin_intc_register_type)
