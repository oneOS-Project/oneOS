/*
 * Copyright (C) 2020-2021 Nikita Melekhin. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Modified by: bellrise
 */

#include <drivers/x86/fpu.h>
#include <libkern/kassert.h>
#include <libkern/log.h>
#include <mem/vmm/vmm.h>
#include <platform/generic/registers.h>
#include <platform/generic/system.h>
#include <platform/x86/isr_handler.h>
#include <tasking/cpu.h>
#include <tasking/dump.h>
#include <tasking/sched.h>
#include <tasking/tasking.h>
#include <tasking/thread.h>

#define ERR_BUF_SIZE 64
static char err_buf[ERR_BUF_SIZE];

static const char* exception_messages[32] = {
    "Division by zero",
    "Debug",
    "Non-maskable interrupt",
    "Breakpoint",
    "Detected overflow",
    "Out-of-bounds",
    "Invalid opcode",
    "No coprocessor",
    "Double fault",
    "Coprocessor segment overrun",
    "Bad TSS",
    "Segment not present",
    "Stack fault",
    "General protection fault",
    "Page fault",
    "Unknown interrupt",
    "Coprocessor fault",
    "Alignment check",
    "Machine check",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

void isr_handler(trapframe_t* frame)
{
    int res;
    proc_t* proc;

    system_disable_interrupts();
    cpu_enter_kernel_space();

    proc = NULL;
    if (likely(RUNNING_THREAD)) {
        proc = RUNNING_THREAD->process;
        if (RUNNING_THREAD->process->is_kthread)
            RUNNING_THREAD->tf = frame;
    }

    switch (frame->int_no) {
    /* Division by 0 or kernel trap (if no process). */
    case 0:
        if (proc) {
            log_warn("Crash: division by zero in T%d\n", RUNNING_THREAD->tid);
            dump_and_kill(proc);
        } else {
            snprintf(
                err_buf, ERR_BUF_SIZE, "Kernel trap at %x, type %d=%s",
                frame->eip, frame->int_no,
                &exception_messages[frame->int_no]);
            kpanic_tf(err_buf, frame);
        }
        break;

    /* Debug, non-maskable interrupt, breakpoint, detected overflow,
           out of bounds. */
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        log_error("Int w/o handler: %d: %s: %d", frame->int_no,
            exception_messages[frame->int_no], frame->err);
        system_stop();
        break;

    /* Invalid opcode or kernel trap (if no process). */
    case 6:
        if (proc) {
            log_warn("Crash: invalid opcode in %d tid\n", RUNNING_THREAD->tid);
            dump_and_kill(proc);
        } else {
            snprintf(
                err_buf, ERR_BUF_SIZE, "Kernel trap at %x, type %d=%s",
                frame->eip, frame->int_no, &exception_messages[frame->int_no]);
            kpanic_tf(err_buf, frame);
        }
        break;

    /* No coprocessor */
    case 7:
        fpu_handler();
        break;

    /* Double fault and other. */
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
        log_error("Int w/o handler: %d: %s: %d", frame->int_no,
            exception_messages[frame->int_no], frame->err);
        system_stop();
        break;

    case 14:
        res = vmm_page_fault_handler(frame->err, read_cr2());
        if (res != SHOULD_CRASH)
            break;

        if (proc) {
            log_warn("Crash: pf err %d at %x: %d pid, %x eip\n",
                frame->err, read_cr2(), proc->pid, frame->eip);
            dump_and_kill(proc);
        } else {
            snprintf(
                err_buf, ERR_BUF_SIZE, "Kernel trap at %x, type %d=%s",
                frame->eip, frame->int_no, &exception_messages[frame->int_no]);
            kpanic_tf(err_buf, frame);
        }
        break;

    case 15:
        log_error("Int w/o handler: %d: %s: %d", frame->int_no,
            exception_messages[frame->int_no], frame->err);
        system_stop();
        break;
    default:
        log_error("Int w/o handler: %d: %d", frame->int_no, frame->err);
        system_stop();
    }

    /* When we are leaving the interrupt handler, we want to jump back into
       user space and enable the x86 PIC again */
    cpu_leave_kernel_space();
    system_enable_interrupts_only_counter();
}
