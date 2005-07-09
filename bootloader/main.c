/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2005 by Linus Nielsen Feltzing
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include "cpu.h"
#include "system.h"
#include "lcd.h"
#include "kernel.h"
#include "thread.h"
#include "ata.h"
#include "disk.h"
#include "font.h"
#include "adc.h"
#include "backlight.h"
#include "button.h"
#include "panic.h"
#include "power.h"
#include "file.h"

#define DRAM_START 0x31000000

#ifdef IRIVER_H100
#define MODEL_NUMBER 1
#else
#define MODEL_NUMBER 0
#endif

int line = 0;

char *modelname[] =
{
    "H120/140",
    "H110/115",
    "H300"
};

int usb_screen(void)
{
   return 0;
}

static void usb_enable(bool on)
{
    GPIO_OUT &= ~0x01000000;      /* GPIO24 is the Cypress chip power */
    GPIO_ENABLE |= 0x01000000;
    GPIO_FUNCTION |= 0x01000000;
    
    GPIO1_FUNCTION |= 0x00000080; /* GPIO39 is the USB detect input */

    if(on)
    {
        /* Power on the Cypress chip */
        GPIO_OUT |= 0x01000000;
        sleep(2);
    }
    else
    {
        /* Power off the Cypress chip */
        GPIO_OUT &= ~0x01000000;        
    }
}

bool usb_detect(void)
{
    return (GPIO1_READ & 0x80)?true:false;
}

void start_iriver_fw(void)
{
    asm(" move.w #0x2700,%sr");
    /* Reset the cookie for the crt0 crash check */
    asm(" move.l #0,%d0");
    asm(" move.l %d0,0x10017ffc");
    asm(" movec.l %d0,%vbr");
    asm(" move.l 0,%sp");
    asm(" lea.l 8,%a0");
    asm(" jmp (%a0)");
}

int load_firmware(void)
{
    int fd;
    int rc;
    int len;
    unsigned long chksum;
    char model[5];
    unsigned long sum;
    int i;
    unsigned char *buf = (unsigned char *)DRAM_START;
    char str[80];
    
    fd = open("/rockbox.iriver", O_RDONLY);
    if(fd < 0)
        return -1;

    len = filesize(fd) - 8;

    snprintf(str, 80, "Length: %x", len);
    lcd_puts(0, line++, str);
    lcd_update();

    lseek(fd, FIRMWARE_OFFSET_FILE_CRC, SEEK_SET);
    
    rc = read(fd, &chksum, 4);
    if(rc < 4)
        return -2;

    snprintf(str, 80, "Checksum: %x", chksum);
    lcd_puts(0, line++, str);
    lcd_update();

    rc = read(fd, model, 4);
    if(rc < 4)
        return -3;

    model[4] = 0;
    
    snprintf(str, 80, "Model name: %s", model);
    lcd_puts(0, line++, str);
    lcd_update();

    lseek(fd, FIRMWARE_OFFSET_FILE_DATA, SEEK_SET);

    rc = read(fd, buf, len);
    if(rc < len)
        return -4;

    close(fd);

    sum = MODEL_NUMBER;
    
    for(i = 0;i < len;i++) {
        sum += buf[i];
    }

    snprintf(str, 80, "Sum: %x", sum);
    lcd_puts(0, line++, str);
    lcd_update();

    if(sum != chksum)
        return -5;

    return 0;
}


void start_firmware(void)
{
    asm(" move.w #0x2700,%sr");
    /* Reset the cookie for the crt0 crash check */
    asm(" move.l #0,%d0");
    asm(" move.l %d0,0x10017ffc");
    asm(" move.l %0,%%d0" :: "i"(DRAM_START));
    asm(" movec.l %d0,%vbr");
    asm(" move.l %0,%%sp" :: "m"(*(int *)DRAM_START));
    asm(" move.l %0,%%a0" :: "m"(*(int *)(DRAM_START+4)));
    asm(" jmp (%a0)");
}

void main(void)
{
    int i;
    int rc;
    char buf[256];
    bool rc_on_button = false;
    bool on_button = false;
    int data;

    /* Read the buttons early */

    /* Set GPIO33, GPIO37, GPIO38  and GPIO52 as general purpose inputs */
    GPIO1_FUNCTION |= 0x00100062;
    GPIO1_ENABLE &= ~0x00100062;

    data = GPIO1_READ;
    if ((data & 0x20) == 0)
        on_button = true;
    
    if ((data & 0x40) == 0)
        rc_on_button = true;

    power_init();
    system_init();
    kernel_init();

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    /* Set up waitstates for the peripherals */
    set_cpu_frequency(0); /* PLL off */
#endif
    
    backlight_init();
    set_irq_level(0);
    lcd_init();
    font_init();
    adc_init();
    button_init();

    lcd_setfont(FONT_SYSFIXED);

    snprintf(buf, 256, "Rockboot version 3");
    lcd_puts(0, line++, buf);
    lcd_update();

    sleep(HZ/50); /* Allow the button driver to check the buttons */

    if(((button_status() & BUTTON_REC) == BUTTON_REC) ||
       ((button_status() & BUTTON_RC_REC) == BUTTON_RC_REC)) {
        lcd_puts(0, 8, "Starting original firmware...");
        lcd_update();
        start_iriver_fw();
    }

    if(on_button & button_hold() ||
       rc_on_button & remote_button_hold()) {
        lcd_puts(0, 8, "HOLD switch on, power off...");
        lcd_update();
        sleep(HZ*2);
        /* Reset the cookie for the crt0 crash check */
        asm(" move.l #0,%d0");
        asm(" move.l %d0,0x10017ffc");
        power_off();
    }

    rc = ata_init();
    if(rc)
    {
#ifdef HAVE_LCD_BITMAP
        char str[32];
        lcd_clear_display();
        snprintf(str, 31, "ATA error: %d", rc);
        lcd_puts(0, 1, str);
        lcd_update();
        while(!(button_get(true) & BUTTON_REL));
#endif
        panicf("ata: %d", rc);
    }

    /* A hack to enter USB mode without using the USB thread */
    if(usb_detect())
    {
        lcd_clear_display();
        lcd_puts(0, 7, "    Bootloader USB mode");
        lcd_update();

        ata_spin();
        ata_enable(false);
        usb_enable(true);
        while(usb_detect())
        {
            ata_spin(); /* Prevent the drive from spinning down */
            sleep(HZ);
        }

        system_reboot();
    }
    
    disk_init();

    rc = disk_mount_all();
    if (rc<=0)
    {
        lcd_clear_display();
        lcd_puts(0, 0, "No partition found");
        while(button_get(true) != SYS_USB_CONNECTED) {};
    }

    lcd_puts(0, line++, "Loading firmware");
    lcd_update();
    i = load_firmware();
    snprintf(buf, 256, "Result: %d", i);
    lcd_puts(0, line++, buf);
    lcd_update();

    if(i == 0)
        start_firmware();
    
    start_iriver_fw();
}

/* These functions are present in the firmware library, but we reimplement
   them here because the originals do a lot more than we want */

void reset_poweroff_timer(void)
{
}

void screen_dump(void)
{
}

int dbg_ports(void)
{
   return 0;
}

void mpeg_stop(void)
{
}

void usb_acknowledge(void)
{
}

void usb_wait_for_disconnect(void)
{
}
