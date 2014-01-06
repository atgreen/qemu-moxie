/*
 * Moxie GDB server stub
 *
 * Copyright (c) 2013 Anthony Green
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
 */
#include "config.h"
#include "qemu-common.h"
#include "exec/gdbstub.h"

#define NUM_CORE_REGS (16 + 1)

int moxie_cpu_gdb_read_register(CPUState *cs, uint8_t *mem_buf, int n)
{
    MoxieCPU *cpu = MOXIE_CPU(cs);
    CPUMoxieState *env = &cpu->env;

    if (n < 16) {
        return gdb_get_reg32(mem_buf, env->gregs[n]);
    } else if (n == 16) {
        return gdb_get_reg32(mem_buf, env->pc);
    }
    return 0;
}

int moxie_cpu_gdb_write_register(CPUState *cs, uint8_t *mem_buf, int n)
{
    MoxieCPU *cpu = MOXIE_CPU(cs);
    CPUMoxieState *env = &cpu->env;
    uint32_t tmp;

    if (n > NUM_CORE_REGS) {
        return 0;
    }

    tmp = ldl_p(mem_buf);

    if (n < 16) {
        env->gregs[n] = tmp;
    } else {
        env->pc = tmp;
    }
    return 4;
}
