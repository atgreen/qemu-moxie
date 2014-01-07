/*
 * Timer device for Moxie Marin
 *
 * Copyright (C) 2014 Anthony Green
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation, or any later version.
 * See the COPYING file in the top-level directory.
 */
#include "hw/sysbus.h"
#include "hw/ptimer.h"
#include "qemu/main-loop.h"

#define TYPE_MARIN_TIMER "marin_timer"
#define MARIN_TIMER(obj) OBJECT_CHECK(MarinTimerState, (obj), TYPE_MARIN_TIMER)

/* Marin timer implementation. */
typedef struct MarinTimerState {
    SysBusDevice parent_obj;

    QEMUBH *bh;
    qemu_irq irq;
    ptimer_state *ptimer;
} MarinTimerState;

static void marin_timer_tick(void *opaque)
{
    MarinTimerState *s = opaque;

    qemu_irq_raise(s->irq);
}

static int marin_timer_init(SysBusDevice *dev)
{
    MarinTimerState *s = MARIN_TIMER(dev);

    sysbus_init_irq(dev, &s->irq);

    s->bh = qemu_bh_new(marin_timer_tick, s);
    s->ptimer = ptimer_init(s->bh);
    ptimer_set_freq(s->ptimer, 50 * 1000 * 1000);

    return 0;
}

static void marin_timer_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);

    sdc->init = marin_timer_init;
}

static const TypeInfo marin_timer_info = {
    .name = TYPE_MARIN_TIMER,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MarinTimerState),
    .class_init = marin_timer_class_init,
};

static void marin_timer_register_type(void)
{
    type_register_static(&marin_timer_info);
}

type_init(marin_timer_register_type)
