/**
 *  Andromeda
 *  Copyright (C) 2015  Bart Kuivenhoven
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <types.h>
#include <stdio.h>
#include <andromeda/system.h>
#include <lib/tree.h>

#define FREQUENCY_DIVIDER 1000

static int timer_subscribe_event(time_t time, uint16_t id, handler call_back,
                struct sys_timer* timer);
struct event {
        time_t time;
        handler event_call_back;
        uint16_t id;
        struct event* next;
};

static int timer_callback(uint16_t irq_no, uint16_t id, uint64_t r1,
                uint64_t r2, uint64_t r3 __attribute__((unused)),
                uint64_t r4 __attribute__((unused)))
{
        struct sys_timer* timer = NULL;
        /* If this interrupt is global, choose one of the global timers */
        if (r2 != 0x00) {
                /* GLOBAL */

                if (core.arch == NULL) {
                        panic("No architecture present");
                }
                if (core.arch->pic == NULL) {
                        panic("No interrupt controller found!");
                }
                if (core.arch->pic->timers == NULL) {
                        panic("Timers not initialised");
                }

                timer = core.arch->pic->timers->find(irq_no,
                                core.arch->pic->timers);
        } else {
                /* Local CPU timer */
                warning("CPUID is static in timer interrupt, "
                                "change this up please!\n");

                /* Get the appropriate CPU */
                struct sys_cpu* cpu = getcpu(r1); /* This is to work in the future */
                cpu = getcpu(0); /* For now, just assume CPU 0 */
                /* Traverse it's interrupt controller */
                if (cpu->pic == NULL) {
                        panic("No PIC data found for CPU");
                }
                /* And there we have the timer */
                timer = cpu->pic->timers;
        }
        if (timer == NULL) {
                panic("Invalid timer found!");
        }

        /* If the timer callback doesn't match the appropriate ID, panic */
        if (timer->interrupt_id != id) {
                panic("Something happened to our interrupt ID!");
        }

        /* Atomically increment the timer value */
        mutex_lock(&(timer->timer_lock));
        time_t tick = atomic_inc(&(timer->tick));
        if (tick == (uint32_t) (timer->freq) / FREQUENCY_DIVIDER) {
                timer->time++;
                atomic_sub(&(timer->tick),
                                (uint32_t) (timer->freq) / FREQUENCY_DIVIDER);
        }
        mutex_unlock(&(timer->timer_lock));

        /* Now go do something with the available events */
        if (timer->events == NULL) {
                warning("No events could be found due to NULL pointer\n");
        } else {
                /* Find our appropriate event */
                struct event* event = timer->events->find(timer->time,
                                timer->events);
                /* Delete the events */
                timer->events->delete(timer->time, timer->events);

                while (event != NULL ) {
                        if (event->time != timer->time) {
                                timer_subscribe_event(event->time, event->id,
                                                event->event_call_back, timer);
                                struct event* last = event;
                                event = event->next;
                                kfree(last);
                                continue;
                        }

                        event->event_call_back(event->id, timer->time, irq_no);

                        struct event* last = event;
                        event = event->next;
                        kfree(last);
                }
        }

        return -E_SUCCESS;
}

static int timer_set_freq_dummy(time_t freq, struct sys_timer* timer)
{
        warning("Someone tried to set my (timer)%X to %X\n", (int) timer,
                        (int) freq);
#ifdef DEBUG
        panic("Here you have a nice stack trace!");
#endif
        return -E_NOFUNCTION;
}

static int timer_subscribe_event(time_t time, uint16_t id, handler call_back,
                struct sys_timer* timer)
{
        /* Do some parameter checking */
        if (timer == NULL || call_back == NULL) {
                return -E_NULL_PTR;
        }

        /* Allocate a new event */
        struct event* event = kmalloc(sizeof(*event));
        if (event == NULL) {
                return -E_NOMEM;
        }

        /* Set up said event */
        memset(event, 0, sizeof(*event));
        event->event_call_back = call_back;
        event->id = id;
        event->time = time;

        /* See if we can find another event at this time */
        volatile struct event* found = timer->events->find((int) time,
                        timer->events);
        /* If not, we'll add this one */
        if (found == NULL) {
                return timer->events->add((int) time, event, timer->events);
        }

        /* If not, we'll add this one to the existing list */
        while (found->next != NULL ) {
                found = found->next;
        }
        found->next = event;

        return -E_SUCCESS;
}

#ifdef TIMER_DBG

static int timer_dbg(int16_t id, time_t time,
                int16_t irq_no __attribute__((unused)))
{
        printf("[ DEBUG ] Timer time: %X\n", (int32_t) time);

        if (id != 0) {
                panic("Incorrect ID!");
        }

        /* Finds the timer connected to CPU 0 and subscribes to it */
        timer_subscribe_event(time + 10, 0, timer_dbg, get_cpu_timer(0));

        return -E_SUCCESS;
}

static int timer_dbg_pit(int16_t id, time_t time, int16_t irq_no)
{
        printf("[ DEBUG ] Timer time: %X\n", (int32_t) time);

        if (id != 0) {
                panic("Incorrect id");
        }

        struct sys_timer* timer = get_global_timer(irq_no);

        if (timer == NULL) {
                printf("NULL timer found!\n");
        }
        timer_subscribe_event(time + 10, 0, timer_dbg_pit, timer);

        return -E_SUCCESS;
}

#endif

static struct sys_timer* init_timer(time_t freq, int32_t interrupt_id)
{
        struct sys_timer* timer = kmalloc(sizeof(*timer));
        if (timer == NULL) {
                panic("Out of memory!");
        }

        memset(timer, 0, sizeof(*timer));

        timer->events = tree_new_avl();
        timer->freq = freq;
        timer->time = 0;
        timer->set_freq = timer_set_freq_dummy;
        timer->subscribe = timer_subscribe_event;
        timer->interrupt_id = interrupt_id;

        return timer;
}

int andromeda_timer_init(time_t freq, int16_t irq_no)
{
        int32_t id = interrupt_register(irq_no, timer_callback);
        if (core.arch == NULL) {
                panic("Architecture abstraction not yet complete!");
        }
        if (core.arch->pic == NULL) {
                panic("Global interrupt controller not yet initialised");
        }

        struct sys_io_pic* pic = core.arch->pic;
        if (pic->timers == NULL) {
                pic->timers = tree_new_avl();
        }
        if (pic->timers == NULL) {
                panic("Out of memory initialising timers!");
        }

        struct sys_timer* timer = init_timer(freq, id);

        pic->timers->add(irq_no, timer, pic->timers);

#ifdef TIMER_DBG
        timer_subscribe_event(10, 0, timer_dbg_pit, timer);
#endif
        return -E_SUCCESS;
}

int cpu_timer_init(int cpuid, time_t freq, int16_t irq_no)
{
        int32_t id = interrupt_register(irq_no, timer_callback);
        struct sys_cpu* cpu = getcpu(cpuid);
        if (cpu == NULL) {
                panic("CPU structure not found!");
        }
        if (cpu->pic == NULL) {
                panic("No interrupt controller structure found!");
        }

        struct sys_timer* timer = init_timer(freq, id);

        cpu->pic->timers = timer;
#ifdef TIMER_DBG
        timer_subscribe_event(10, 0, timer_dbg, timer);
#endif

        return -E_NOFUNCTION;
}

struct sys_timer* get_global_timer(int16_t irq_no)
{
        struct sys_timer* timer = NULL;

        if (core.arch == NULL) {
                panic("ARCH not initialised");
        }
        if (core.arch->pic == NULL) {
                panic("No interrupt controller found!");
        }
        if (core.arch->pic->timers == NULL) {
                panic("Too soon still, you have no timers data structure");
        }

        timer = core.arch->pic->timers->find(irq_no, core.arch->pic->timers);

        return timer;
}

struct sys_timer* get_cpu_timer(int16_t cpu)
{
        struct sys_cpu* c = getcpu(cpu);

        if (c->pic == NULL) {
                panic("Too soon, no interrupt controller initialised");
        }

        return c->pic->timers;
}
