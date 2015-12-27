/*
 * QEMU/marin SoC emulation
 *
 * Emulates the FPGA-hosted Marin SoC
 *
 * Copyright (c) 2013 Anthony Green
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "hw/sysbus.h"
#include "hw/hw.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "exec/address-spaces.h"
#include "elf.h"

typedef struct {
    MoxieCPU *cpu;
    hwaddr bootstrap_pc;
} ResetInfo;

typedef struct {
    uint64_t ram_size;
    const char *kernel_filename;
    const char *kernel_cmdline;
    const char *initrd_filename;
    uint64_t entry;
} LoaderParams;

static void load_kernel(MoxieCPU *cpu, LoaderParams *loader_params)
{
    uint64_t kernel_low, kernel_high;
    long kernel_size;
    long initrd_size;
    ram_addr_t initrd_offset;

    kernel_size = load_elf(loader_params->kernel_filename,  NULL, NULL,
                           &loader_params->entry, &kernel_low, &kernel_high, 1,
                           EM_MOXIE, 0);

    if (!kernel_size) {
        fprintf(stderr, "qemu: could not load kernel '%s'\n",
                loader_params->kernel_filename);
        exit(1);
    }

    /* load initrd */
    initrd_size = 0;
    initrd_offset = 0;
    if (loader_params->initrd_filename) {
        initrd_size = get_image_size(loader_params->initrd_filename);
        if (initrd_size > 0) {
            initrd_offset = (kernel_high + ~TARGET_PAGE_MASK)
              & TARGET_PAGE_MASK;
            if (initrd_offset + initrd_size > loader_params->ram_size) {
                fprintf(stderr,
                        "qemu: memory too small for initial ram disk '%s'\n",
                        loader_params->initrd_filename);
                exit(1);
            }
            initrd_size = load_image_targphys(loader_params->initrd_filename,
                                              initrd_offset,
                                              ram_size);
        }
        if (initrd_size == (target_ulong)-1) {
            fprintf(stderr, "qemu: could not load initial ram disk '%s'\n",
                    loader_params->initrd_filename);
            exit(1);
        }
    }
}

static void main_cpu_reset(void *opaque)
{
    ResetInfo *reset_info = opaque;
    CPUMoxieState *env = &reset_info->cpu->env;

    cpu_reset(CPU(reset_info->cpu));
    env->pc = (uint32_t)reset_info->bootstrap_pc;
}

static inline DeviceState *marin_uart_create(hwaddr base,
        qemu_irq irq)
{
    DeviceState *dev;

    dev = qdev_create(NULL, "marin-uart");
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, base);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq);

    return dev;
}

static inline DeviceState *marin_intc_create(hwaddr base)
{
    DeviceState *dev;

    dev = qdev_create(NULL, "marin_intc");
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, base);

    return dev;
}

static inline DeviceState *marin_timer_create(qemu_irq irq)
{
    DeviceState *dev;

    dev = qdev_create(NULL, "marin_timer");
    qdev_init_nofail(dev);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq);

    return dev;
}

static void marin_init(MachineState *machine)
{
    MoxieCPU *cpu = NULL;
    ram_addr_t ram_size = machine->ram_size;
    const char *cpu_model = machine->cpu_model;
    const char *kernel_filename = machine->kernel_filename;
    const char *kernel_cmdline = machine->kernel_cmdline;
    const char *initrd_filename = machine->initrd_filename;
    CPUMoxieState *env;
    MemoryRegion *address_space_mem = get_system_memory();
    MemoryRegion *ocram = g_new(MemoryRegion, 1);
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    MemoryRegion *rom = g_new(MemoryRegion, 1);
    hwaddr ram_base = 0x2ffff000;
    ResetInfo *reset_info;
    LoaderParams loader_params;

    reset_info = g_malloc0(sizeof(ResetInfo));

    /* Init CPUs. */
    if (cpu_model == NULL) {
        cpu_model = "MoxieLite-moxie-cpu";
    }
    cpu = cpu_moxie_init(cpu_model);
    if (!cpu) {
        fprintf(stderr, "Unable to find CPU definition\n");
        exit(1);
    }
    reset_info->cpu = cpu;
    env = &cpu->env;

    qemu_register_reset(main_cpu_reset, reset_info);

    /* Allocate RAM. */
    memory_region_init_ram(ocram, NULL, "marin-onchip.ram", 0x1000*4, &error_fatal);
    vmstate_register_ram_global(ocram);
    memory_region_add_subregion(address_space_mem, 0x10000000, ocram);

    memory_region_init_ram(ram, NULL, "marin-external.ram", ram_size, &error_fatal);
    vmstate_register_ram_global(ram);
    memory_region_add_subregion(address_space_mem, ram_base, ram);

    memory_region_init_ram(rom, NULL, "moxie.rom", 128*0x1000, &error_fatal);
    vmstate_register_ram_global(rom);
    memory_region_add_subregion(get_system_memory(), 0x1000, rom);

    reset_info->bootstrap_pc = 0x1000;

    if (kernel_filename) {
        loader_params.ram_size = ram_size;
        loader_params.kernel_filename = kernel_filename;
        loader_params.kernel_cmdline = kernel_cmdline;
        loader_params.initrd_filename = initrd_filename;
        load_kernel(cpu, &loader_params);
        reset_info->bootstrap_pc = (hwaddr) loader_params.entry;
    }

    marin_intc_create(0xF0000010);
    marin_uart_create(0xF0000008, env->irq[4]);
    marin_timer_create(env->irq[1]);
}

static void marin_machine_init(MachineClass *mc)
{
    mc->desc = "Moxie SoC";
    mc->init = marin_init;
    mc->is_default = 1;
}

DEFINE_MACHINE("marin", marin_machine_init)

