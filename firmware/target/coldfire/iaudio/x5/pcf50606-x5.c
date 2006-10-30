/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2006 by Linus Nielsen Feltzing
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include "config.h"
#include "system.h"
#include "kernel.h"
#include "pcf50606.h"
#include "button-target.h"
#include "powermgmt.h"

/* These voltages were determined by measuring the output of the PCF50606
   on a running X5, and verified by disassembling the original firmware */
static void set_voltages(void)
{
    static const unsigned char buf[4] =
    {
        0xf4,   /* IOREGC = 2.9V, ON in all states */
        0xf0,   /* D1REGC = 2.5V, ON in all states */
        0xf6,   /* D2REGC = 3.1V, ON in all states */
        0xf4,   /* D3REGC = 2.9V, ON in all states */
    };

    pcf50606_write_multiple(0x23, buf, 4);
}

static void init_pmu_interrupts(void)
{
    /* inital data is interrupt masks */
    unsigned char data[3] =
    {
        ~0x04, /* unmask ONKEY1S */
        ~0x00,
        ~0x06, /* unmask ACDREM, ACDINS  */
    };

    /* make sure GPI0 interrupt is off before unmasking anything */
    and_l(~0xf, &INTPRI5);  /* INT32 - Priority 0 (Off) */

    /* unmask the PMU interrupts we want to service */
    pcf50606_write_multiple(0x05, data, 3);
    /* clear INT1-3 as these are left set after standby */
    pcf50606_read_multiple(0x02, data, 3);

    /* Set to read pcf50606 INT but keep GPI0 off until init completes */
    and_l(~0x00000001, &GPIO_ENABLE);
    or_l(0x00000001, &GPIO_FUNCTION);
    or_l(0x00000100, &GPIO_INT_EN);     /* GPI0 H-L */
}

static inline void enable_pmu_interrupts(void)
{
    /* clear pending GPI0 interrupts first or it may miss the first
       H-L transition */
    or_l(0x00000100, &GPIO_INT_CLEAR);
    or_l(0x3, &INTPRI5); /* INT32 - Priority 3 */
}

void pcf50606_init(void)
{
    pcf50606_i2c_init();

    /* initialize pmu interrupts but don't service them yet */
    init_pmu_interrupts();
    set_voltages();

    pcf50606_write(0x39, 0x00); /* GPOOD0 = green led OFF */
    pcf50606_write(0x3a, 0x00); /* GPOOD1 = red led OFF */

    pcf50606_write(0x35, 0x11); /* Backlight PWM = 512Hz, 8/16, Active */
#ifdef BOOTLOADER
    /* Backlight starts OFF in bootloader */
    pcf50606_write(0x38, 0x80); /* Backlight OFF, GPO1INV=1, GPO1ACT=011 */
#else
    /* Keep backlight on when changing to firmware */
    pcf50606_write(0x38, 0xb0); /* Backlight ON, GPO1INV=1, GPO1ACT=011 */
#endif

    /* Accessory detect */
    pcf50606_write(0x33, 0x8e); /* ACDAPE=1, THRSHLD=2.40V */

    /* allow GPI0 interrupts from PMU now */
    enable_pmu_interrupts();
}

void pcf50606_reset_timeout(void)
{
    int level = set_irq_level(HIGHEST_IRQ_LEVEL);
    pcf50606_write(0x08, pcf50606_read(0x08) | 0x02); /* OOCC1 - TOTRST=1 */
    set_irq_level(level);
}

/* Handles interrupts generated by the pcf50606 */
void GPI0(void) __attribute__ ((interrupt_handler, section(".text")));
void GPI0(void)
{
    unsigned char data[3]; /* 0 = INT1, 1 = INT2, 2 = INT3 */

    /* Clear pending GPI0 interrupts */
    or_l(0x00000100, &GPIO_INT_CLEAR);

    /* clear pending interrupts from pcf50606 */
    pcf50606_read_multiple(0x02, data, 3);

    if (data[0] & 0x04)
    {
        /* ONKEY1S */
        if (GPIO_READ & 0x02000000)
            sys_poweroff();             /* main ONKEY */
        else
            pcf50606_reset_timeout();   /* remote ONKEY */
    }

    if (data[2] & 0x06)
    {
        /* ACDINS/ACDREM */
        /* Check if main buttons should be actually be scanned or not
           - bias towards "yes" out of paranoia. */
        button_enable_scan((data[2] & 0x02) != 0 ||
                           (pcf50606_read(0x33) & 0x01) != 0);
    }
}
