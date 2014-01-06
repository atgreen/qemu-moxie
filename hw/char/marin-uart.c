/*
 *  QEMU model of the Marin UART.
 *
 *  Copyright (c) 2013 Anthony Green <green@moxielogic.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "hw/hw.h"
#include "hw/sysbus.h"
#include "trace.h"
#include "sysemu/char.h"
#include "qemu/error-report.h"

enum {
    R_RXREADY = 0,
    R_TXREADY,
    R_RXBYTE,
    R_TXBYTE,
    R_MAX
};

#define TYPE_MARIN_UART "marin-uart"
#define MARIN_UART(obj) OBJECT_CHECK(MarinUartState, (obj), TYPE_MARIN_UART)

struct MarinUartState {
    SysBusDevice busdev;
    MemoryRegion regs_region;
    CharDriverState *chr;
    qemu_irq irq;

    uint16_t regs[R_MAX];
};
typedef struct MarinUartState MarinUartState;

static void uart_update_irq(MarinUartState *s)
{
}

static uint64_t uart_read(void *opaque, hwaddr addr,
                          unsigned size)
{
    MarinUartState *s = opaque;
    uint32_t r = 0;

    addr >>= 1;
    switch (addr) {
    case R_RXREADY:
        r = s->regs[R_RXREADY];
        break;
    case R_TXREADY:
        r = 1;
        break;
    case R_TXBYTE:
        r = s->regs[R_TXBYTE];
        break;
    case R_RXBYTE:
        r = s->regs[R_RXBYTE];
        s->regs[R_RXREADY] = 0;
        qemu_chr_accept_input(s->chr);
        break;
    default:
        error_report("marin_uart: read access to unknown register 0x"
                TARGET_FMT_plx, addr << 1);
        break;
    }

    return r;
}

static void uart_write(void *opaque, hwaddr addr, uint64_t value,
                       unsigned size)
{
    MarinUartState *s = opaque;
    unsigned char ch = value;

    addr >>= 1;
    switch (addr) {
    case R_TXBYTE:
        if (s->chr) {
            qemu_chr_fe_write(s->chr, &ch, 1);
        }
        break;

    default:
        error_report("marin_uart: write access to unknown register 0x"
                TARGET_FMT_plx, addr << 1);
        break;
    }

    uart_update_irq(s);
}

static const MemoryRegionOps uart_mmio_ops = {
    .read = uart_read,
    .write = uart_write,
    .valid = {
        .min_access_size = 2,
        .max_access_size = 2,
    },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void uart_rx(void *opaque, const uint8_t *buf, int size)
{
    MarinUartState *s = opaque;

    s->regs[R_RXBYTE] = *buf;
    s->regs[R_RXREADY] = 1;

    uart_update_irq(s);
}

static int uart_can_rx(void *opaque)
{
    MarinUartState *s = opaque;

    return !(s->regs[R_RXREADY]);
}

static void uart_event(void *opaque, int event)
{
}

static void marin_uart_reset(DeviceState *d)
{
    MarinUartState *s = MARIN_UART(d);
    int i;

    for (i = 0; i < R_MAX; i++) {
        s->regs[i] = 0;
    }
}

static int marin_uart_init(SysBusDevice *dev)
{
    MarinUartState *s = MARIN_UART(dev);

    sysbus_init_irq(dev, &s->irq);

    memory_region_init_io(&s->regs_region, OBJECT(s), &uart_mmio_ops, s,
            TYPE_MARIN_UART, R_MAX * 4);
    sysbus_init_mmio(dev, &s->regs_region);

    s->chr = qemu_char_get_next_serial();
    if (s->chr) {
        qemu_chr_add_handlers(s->chr, uart_can_rx, uart_rx, uart_event, s);
    }

    return 0;
}

static const VMStateDescription vmstate_marin_uart = {
    .name = TYPE_MARIN_UART,
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT16_ARRAY(regs, MarinUartState, R_MAX),
        VMSTATE_END_OF_LIST()
    }
};

static void marin_uart_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = marin_uart_init;
    dc->reset = marin_uart_reset;
    dc->vmsd = &vmstate_marin_uart;
}

static const TypeInfo marin_uart_info = {
    .name          = TYPE_MARIN_UART,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MarinUartState),
    .class_init    = marin_uart_class_init,
};

static void marin_uart_register_types(void)
{
    type_register_static(&marin_uart_info);
}

type_init(marin_uart_register_types)
