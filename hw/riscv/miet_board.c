/*
 * QEMU RISC-V MIET prototype Board
 *
 * Copyright (C) 2016, 2017 Antony Pavlov <antonynpavlov@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/char/serial.h"
#include "hw/boards.h"
#include "hw/riscv/cpudevs.h"
#include "sysemu/char.h"
#include "sysemu/arch_init.h"
#include "hw/loader.h"
#include "exec/address-spaces.h"
#include "hw/sysbus.h"             /* SysBusDevice */

#define TYPE_RISCV_MIET_BOARD "miet_board"
#define RISCV_MIET_BOARD(obj) OBJECT_CHECK(BoardState, (obj), TYPE_RISCV_MIET_BOARD)

#define BIOS_SIZE (16 * 1024)
#define BIOS_FILENAME "riscv_bios.bin"

typedef struct {
    SysBusDevice parent_obj;
} BoardState;

static void main_cpu_reset(void *opaque)
{
    RISCVCPU *cpu = opaque;
    cpu_reset(CPU(cpu));
}

static void riscv_miet_board_init(MachineState *args)
{
    const char *cpu_model = args->cpu_model;
    RISCVCPU *cpu;
    CPURISCVState *env;
    int i;
    char *filename;
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *bios = g_new(MemoryRegion, 1);

    DeviceState *dev = qdev_create(NULL, TYPE_RISCV_MIET_BOARD);
    object_property_set_bool(OBJECT(dev), true, "realized", NULL);

    /* Make sure the first 2 serial ports are associated with a device. */
    for (i = 0; i < 2; i++) {
        if (!serial_hds[i]) {
            char label[32];
            snprintf(label, sizeof(label), "serial%d", i);
            serial_hds[i] = qemu_chr_new(label, "null", NULL);
        }
    }

    /* init CPUs */
    if (cpu_model == NULL) {
        cpu_model = "any";
    }

    for (i = 0; i < smp_cpus; i++) {
        cpu = cpu_riscv_init(cpu_model);
        if (cpu == NULL) {
            fprintf(stderr, "Unable to find CPU definition\n");
            exit(1);
        }
        env = &cpu->env;

        /* Init internal devices */
        cpu_riscv_irq_init_cpu(env);
        cpu_riscv_clock_init(env);
        qemu_register_reset(main_cpu_reset, cpu);
    }
    cpu = RISCV_CPU(first_cpu);
    env = &cpu->env;

    memory_region_init_ram(bios, NULL, "bios", BIOS_SIZE,
                           &error_fatal);
    vmstate_register_ram_global(bios);
    memory_region_set_readonly(bios, true);

    /* Map the BIOS / boot exception handler. */
    memory_region_add_subregion(system_memory, 0xf0000000LL, bios);

    /* Load a BIOS / boot exception handler image. */
    if (bios_name == NULL) {
        bios_name = BIOS_FILENAME;
    }

    filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bios_name);
    if (filename) {
        load_image_targphys(filename, 0xf0000000LL, BIOS_SIZE);
        g_free(filename);
    }

    serial_mm_init(system_memory, 0x90000000LL, 2, NULL,
        115200, serial_hds[0], DEVICE_NATIVE_ENDIAN);
}

static int riscv_miet_board_sysbus_device_init(SysBusDevice *sysbusdev)
{
    return 0;
}

static void riscv_miet_board_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);
    k->init = riscv_miet_board_sysbus_device_init;
}

static const TypeInfo riscv_miet_board_device = {
    .name          = TYPE_RISCV_MIET_BOARD,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BoardState),
    .class_init    = riscv_miet_board_class_init,
};

static void riscv_miet_board_machine_init(MachineClass *mc)
{
    mc->desc = "RISC-V MIET Board";
    mc->init = riscv_miet_board_init;
    mc->max_cpus = 1;
}

DEFINE_MACHINE("miet", riscv_miet_board_machine_init)

static void riscv_miet_board_register_types(void)
{
    type_register_static(&riscv_miet_board_device);
}

type_init(riscv_miet_board_register_types);
