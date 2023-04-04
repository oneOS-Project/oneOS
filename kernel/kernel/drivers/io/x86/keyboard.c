/*
 * Copyright (C) 2020-2022 The opuntiaOS Project Authors.
 *  + Contributed by Nikita Melekhin <nimelehin@gmail.com>
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <algo/ringbuffer.h>
#include <drivers/io/x86/keyboard.h>
#include <drivers/irq/irq_api.h>
#include <io/tty/tty.h>
#include <libkern/libkern.h>
#include <platform/x86/port.h>

static driver_desc_t _keyboard_driver_info();
static key_t _kbdriver_apply_modifiers(key_t key);

static void keyboard_int_handler(irq_line_t il)
{
    uint32_t scancode = (uint32_t)port_read8(0x60);
    generic_emit_key_set1(scancode);
}

static void _kbdriver_notification(uintptr_t msg, uintptr_t param)
{
    if (msg == DEVMAN_NOTIFICATION_DEVFS_READY) {
        if (generic_keyboard_create_devfs() < 0) {
            kpanic("Can't init keyboard in devfs");
        }
    }
}

static driver_desc_t _keyboard_driver_info()
{
    driver_desc_t kbd_desc = { 0 };
    kbd_desc.type = DRIVER_INPUT_SYSTEMS_DEVICE;
    kbd_desc.flags = DRIVER_DESC_FLAG_START;
    kbd_desc.system_funcs.on_start = kbdriver_run;
    kbd_desc.system_funcs.recieve_notification = _kbdriver_notification;
    kbd_desc.functions[DRIVER_INPUT_SYSTEMS_ADD_DEVICE] = kbdriver_run;
    kbd_desc.functions[DRIVER_INPUT_SYSTEMS_GET_LAST_KEY] = 0;
    kbd_desc.functions[DRIVER_INPUT_SYSTEMS_DISCARD_LAST_KEY] = 0;
    return kbd_desc;
}

void kbdriver_install()
{
    devman_register_driver(_keyboard_driver_info(), "kbd86");
}
devman_register_driver_installation(kbdriver_install);

int kbdriver_run()
{
    irq_register_handler(irqline_from_id(1), 0, 0, keyboard_int_handler, BOOT_CPU_MASK);
    generic_keyboard_init();
    return 0;
}
