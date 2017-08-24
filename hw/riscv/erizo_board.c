/*
 * QEMU Erizo generic board
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
#include "hw/boards.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/char/serial.h"
#include "sysemu/arch_init.h"
#include "hw/loader.h"
#include "hw/sysbus.h"
#include "target/riscv/cpu.h"
#include "qapi/error.h"

#define SRAM_ADDR 0x40000000LL
#define SRAM_SIZE (256 * 1024)

#define SDRAM_ADDR 0x80000000LL
#define SDRAM_SIZE (8 * 1024 * 1024)

#define UART0_ADDR 0x90000000LL

#define GPIO_ADDR 0x91000000LL
#define GPIO_SIZE (8)

#define BIOS_ADDR 0xf0000000LL
#define BIOS_SIZE (256 * 1024)
#define BIOS_FILENAME "riscv_bios.bin"

#define TYPE_ERIZO "erizo"
#define ERIZO(obj) OBJECT_CHECK(ErizoState, (obj), TYPE_ERIZO)

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    RISCVHartArrayState harts;
} ErizoState;

static void erizo_init(MachineState *machine)
{
    int i;
    char *filename;
    RISCVHartArrayState *harts;

    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *bios = g_new(MemoryRegion, 1);
    MemoryRegion *sram = g_new(MemoryRegion, 1);
    MemoryRegion *sdram = g_new(MemoryRegion, 1);
    MemoryRegion *gpio = g_new(MemoryRegion, 1);

    DeviceState *dev = qdev_create(NULL, TYPE_ERIZO);
    ErizoState *s = ERIZO(dev);

    qdev_init_nofail(dev);
    harts = &s->harts;

    object_initialize(harts, sizeof(s->harts), TYPE_RISCV_HART_ARRAY);
    object_property_add_child(OBJECT(machine), "soc", OBJECT(harts),
                              &error_abort);
    object_property_set_str(OBJECT(harts), TYPE_RISCV_CPU_RV32IMACU_NOMMU,
                            "cpu-type", &error_abort);
    object_property_set_int(OBJECT(harts), 1, "num-harts",
                            &error_abort);
    object_property_set_bool(OBJECT(harts), true, "realized",
                            &error_abort);

    object_property_set_bool(OBJECT(dev), true, "realized", NULL);

    /* Make sure the first 2 serial ports are associated with a device. */
    for (i = 0; i < 2; i++) {
        if (!serial_hds[i]) {
            char label[32];
            snprintf(label, sizeof(label), "serial%d", i);
            serial_hds[i] = qemu_chr_new(label, "null");
        }
    }

    memory_region_init_ram(gpio, NULL, "gpio", GPIO_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, GPIO_ADDR, gpio);

    memory_region_init_ram(sram, NULL, "sram", SRAM_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, SRAM_ADDR, sram);

    memory_region_init_ram(sdram, NULL, "sdram", SDRAM_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, SDRAM_ADDR, sdram);

    memory_region_init_ram(bios, NULL, "bios", BIOS_SIZE, &error_fatal);
    memory_region_set_readonly(bios, true);

    memory_region_add_subregion(system_memory, BIOS_ADDR, bios);

    if (bios_name == NULL) {
        bios_name = BIOS_FILENAME;
    }

    filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bios_name);
    if (filename) {
        load_image_targphys(filename, BIOS_ADDR, BIOS_SIZE);
        g_free(filename);
    }

    serial_mm_init(system_memory, UART0_ADDR, 2, NULL,
        115200, serial_hds[0], DEVICE_NATIVE_ENDIAN);
}

static int erizo_sysbus_device_init(SysBusDevice *sysbusdev)
{
    return 0;
}

static void erizo_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);
    k->init = erizo_sysbus_device_init;
}

static const TypeInfo erizo_device = {
    .name          = TYPE_ERIZO,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ErizoState),
    .class_init    = erizo_class_init,
};

static void erizo_machine_init(MachineClass *mc)
{
    mc->desc = "generic Erizo Board";
    mc->init = erizo_init;
    mc->max_cpus = 1;
}

DEFINE_MACHINE("erizo", erizo_machine_init)

static void erizo_register_types(void)
{
    type_register_static(&erizo_device);
}

type_init(erizo_register_types);
