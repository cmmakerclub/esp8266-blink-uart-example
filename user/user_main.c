/*
 * File	: user_main.c
 * This file is part of Espressif's AT+ command set program.
 * Copyright (C) 2013 - 2016, Espressif Systems
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "ets_sys.h"
#include "driver/uart.h"
#include "os_type.h"
#include "osapi.h"
#include "gpio.h"



#define MAXTIMINGS 10000
#define BREAKTIME 20

#define user_procTaskPrio        0
#define user_procTaskQueueLen    1
os_event_t    user_procTaskQueue[user_procTaskQueueLen];

static void user_procTask(os_event_t *events);

static volatile os_timer_t some_timer;
extern void user_esp_platform_load_param(void *param, uint16 len);

void some_timerfunc(void *arg)
{
    //Do blinky stuff
    if (GPIO_REG_READ(GPIO_OUT_ADDRESS) & BIT2)
    {
        //Set GPIO2 to LOW
        uart0_sendStr("\r\nGPIO2 LOW\r\n");
        gpio_output_set(0, BIT2, BIT2, 0);
    }
    else
    {
        //Set GPIO2 to HIGH
        gpio_output_set(BIT2, 0, BIT2, 0);
        uart0_sendStr("\r\nGPIO2 HIGH\r\n");
    }
}

//Do nothing function
static void ICACHE_FLASH_ATTR
user_procTask(os_event_t *events)
{
    os_delay_us(10);
}



static void ICACHE_FLASH_ATTR
readDHT(void *arg)
{
    int counter = 0;
    int laststate = 1;
    int i = 0;
    int j = 0;
    int checksum = 0;
    //int bitidx = 0;
    //int bits[250];

    int data[100];

    data[0] = data[1] = data[2] = data[3] = data[4] = 0;

    GPIO_OUTPUT_SET(2, 1);
    os_delay_us(250000);
    GPIO_OUTPUT_SET(2, 0);
    os_delay_us(20000);
    GPIO_OUTPUT_SET(2, 1);
    os_delay_us(40);
    GPIO_DIS_OUTPUT(2);
    PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO2_U);


    // wait for pin to drop?
    while (GPIO_INPUT_GET(2) == 1 && i<100000) {
          os_delay_us(1);
          i++;
    }

        if(i==100000)
          return;

    // read data!

    for (i = 0; i < MAXTIMINGS; i++) {
        counter = 0;
        while ( GPIO_INPUT_GET(2) == laststate) {
            counter++;
                        os_delay_us(1);
            if (counter == 1000)
                break;
        }
        laststate = GPIO_INPUT_GET(2);
        if (counter == 1000) break;

        //bits[bitidx++] = counter;

        if ((i>3) && (i%2 == 0)) {
            // shove each bit into the storage bytes
            data[j/8] <<= 1;
            if (counter > BREAKTIME)
                data[j/8] |= 1;
            j++;
        }
    }

/*
    for (i=3; i<bitidx; i+=2) {
        os_printf("bit %d: %d\n", i-3, bits[i]);
        os_printf("bit %d: %d (%d)\n", i-2, bits[i+1], bits[i+1] > BREAKTIME);
    }
    os_printf("Data (%d): 0x%x 0x%x 0x%x 0x%x 0x%x\n", j, data[0], data[1], data[2], data[3], data[4]);
*/
    char buffer[30];
    float temp_p, hum_p;
    if (j >= 39) {
        checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
        if (data[4] == checksum) {
            /* yay! checksum is valid */

            hum_p = data[0] * 256 + data[1];
            hum_p /= 10;

            temp_p = (data[2] & 0x7F)* 256 + data[3];
            temp_p /= 10.0;
            if (data[2] & 0x80)
                temp_p *= -1;
            // sendReading(temp_p, hum_p);
            os_printf("Temp =  %d *C, Hum = %d \%\n", (int)(temp_p*100), (int)(hum_p*100));
            os_sprintf(buffer, "Temp =  %d *C, Hum = %d \%\n", (int)(temp_p*100), (int)(hum_p*100));
            uart0_sendStr(buffer);
        }
    }

}


void user_init(void)
{

  uart_init(BIT_RATE_115200, BIT_RATE_115200);

  os_printf("\r\nblinky is ready!!!\r\n");
  uart0_sendStr("\r\nblinky is ready\r\n");

      // Initialize the GPIO subsystem.
    gpio_init();

    // //Set GPIO2 to output mode
    // PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

    //Set GPIO2 to output mode
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
    PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO2_U);

    //Set GPIO2 low
    // gpio_output_set(0, BIT2, BIT2, 0);

    //Disarm timer
    os_timer_disarm(&some_timer);

    //Setup timer
    // os_timer_setfn(&some_timer, (os_timer_func_t *)some_timerfunc, NULL);
    //Setup timer
    os_timer_setfn(&some_timer, (os_timer_func_t *)readDHT, NULL);


    //Arm the timer
    //&some_timer is the pointer
    //1000 is the fire time in ms
    //0 for once and 1 for repeating
    os_timer_arm(&some_timer, 1000, 1);
    
    //Start os task
    system_os_task(user_procTask, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);


}
