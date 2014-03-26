/**
 * \file
 *
 * \brief XMEGA-A3BU Xplained demo application
 *
 * Copyright (c) 2011-2012 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */
/**
 * \mainpage
 *
 *
 * \section files Main files:
 * - main.c  Main application
 * - keyboard.c Keyboard driver
 * - lightsensor.c Lightsensor application
 * - production_date.c Time since production application
 * - date_time.c  Date and time from RTC32
 * - timezone.c handle timezones stored in EEPROM and selecting a new one
 * - cdc.c USB CDC class abstraction used for keyboard input
 */
#include <stdio.h>
#include <board.h>
#include <compiler.h>
#include <gfx_mono.h>
#include <gfx_mono_menu.h>
#include <gpio.h>
#include <rtc32.h>
#include <pmic.h>
#include <sysclk.h>
#include <sysfont.h>
#include <util/delay.h>
#include <math.h>
#include "adc_sensors.h"	// TODO: Can possibly be removed when adc_reading is moved to another file
#include "adc.h"			// Same as above
#include "date_time.h"
#include "lightsensor.h"
#include "keyboard.h"
#include "ntc_sensor.h"
#include "production_date.h"
#include "timezone.h"
#include "touch_api.h"
#include "cdc.h"
#include "conf_application.h"
#include "sysfont.h"
#include "twi_master_driver.h"
#include "utilities.h"
#include "sensors.h"

TWI_Master_t twiMaster;
uint16_t twiInt = 0xFFFF;

register8_t pressure_val[2] = {0,0};
register8_t internal_temp_val[2] = {0,0};

enum TWI_READING { PRESSURE , INTERNAL_TEMPERATURE } twi_reading;

/**
 * \brief Main function.
 *
 * Initializes the board and reads out the production date stored in EEPROM and
 * set timezone from EEPROM if it is set. If it is not set it will open the
 * timezone selector to select the local timezone. It then runs the menu system
 * in an infinite while loop.
 */
int main(void)
{
	uint8_t menu_status;
	struct keyboard_event input;
	uint32_t rtc_timestamp;

	sysclk_init();
	board_init();
	pmic_init();
	gfx_mono_init();
	//touch_init();		// TODO: Can probably be removed seeing as we're not going to use it at this time
	//adc_sensors_init();

	//TODO: Evaluate initialization
	adc_b_sensors_init();	//Initialize ADCB

	// Enable display backlight
	gpio_set_pin_high(NHD_C12832A1Z_BACKLIGHT);

	// Workaround for known issue: Enable RTC32 sysclk
	sysclk_enable_module(SYSCLK_PORT_GEN, SYSCLK_RTC);
	while (RTC32.SYNCCTRL & RTC32_SYNCBUSY_bm) {
		// Wait for RTC32 sysclk to become stable
	}

	// If we have battery power and RTC is running, don't initialize RTC32
	if (rtc_vbat_system_check(false) != VBAT_STATUS_OK) {
		rtc_init();

		// Set current time to after production date
		rtc_timestamp = production_date_get_timestamp() + 1;
		rtc_set_time(rtc_timestamp);
	}

	// Get current time
	rtc_timestamp = rtc_get_time();
	// Make sure RTC has not been set to a too early date .
	if (rtc_timestamp < FIRST_POSSIBLE_TIMESTAMP) {
		// Set time to 01.01.2011 00:00:00
		rtc_set_time(FIRST_POSSIBLE_TIMESTAMP);
	}

	// Initialize USB CDC class
	cdc_start();

	cpu_irq_enable();

	// Display a splash screen showing button functions
	//button_splash();

	// Set timezone from EEPROM or to a default value
	timezone_init();


	// ADDED: Initializing temperature display
	temp_disp_init();

	// 26.02.14 TWI init
	sysclk_enable_peripheral_clock(&TWIC);
	TWI_MasterInit(&twiMaster, &TWIC, TWI_MASTER_INTLVL_LO_gc, 400);

	/* Main loop.
	 * Reads and interprets sensors. Sends data for logging.
	 */
	while (true) {

		do {
			do {

				/* Enable LO interrupt level. */
				PMIC.CTRL |= PMIC_LOLVLEN_bm;
				sei();

				// TODO: Read internal temp sensor from I2C to calibrate
				{
					// Handling for internal temp

				}

				//Write I2C status
				bool twiStatus = TWI_MasterRead(&twiMaster, 0x28, 4); // Reading pressure
				twi_reading = PRESSURE;								  // TODO : Move these two to a discrete function to remove chance of fucking up

				//START TEMP PRINT
				_delay_ms(10000);	//NOTE: ms actually means microseconds in this case
				rtc_timestamp = rtc_get_time();
				{
					//TODO: Set a proper id for the log sample. (So it can be properly recognized)
					char * logid = "ADC";
					cdc_putstr(logid);	//Identify sample as on-chip NTC temp (Will be changed).

					//Data separator character
					udi_cdc_putc(',');

					//Timestamp
					cdc_putuint32(rtc_timestamp);

					//Data separator character
					udi_cdc_putc(',');

					/*
					// Testing
					adcb_ch0_measure();
					adcb_ch1_measure();
					adcb_ch2_measure();
					adcb_ch3_measure();

					int16_t temp0 = adcb_chX_get_temperature(0);
					char * temp_s0 = int16_tostr(temp0);
					int16_t temp1 = adcb_chX_get_temperature(1);
					char * temp_s1 = int16_tostr(temp1);
					int16_t temp2 = adcb_chX_get_temperature(2);
					char * temp_s2 = int16_tostr(temp2);

					cdc_putstr(temp_s0);	//temperature in string form
					udi_cdc_putc('\r');	//return
					udi_cdc_putc('\n');	//newline
					cdc_putstr(temp_s1);	//temperature in string form
					udi_cdc_putc('\r');	//return
					udi_cdc_putc('\n');	//newline
					cdc_putstr(temp_s2);	//temperature in string form
					udi_cdc_putc('\r');	//return
					udi_cdc_putc('\n');	//newline
					*/
				}


				//Paint thermometer on screen
				//gfx_mono_put_bitmap(&tempscale, 10, 0);

				// ADCB-Testing
				adcb_ch0_measure();
				adcb_ch1_measure();
				adcb_ch2_measure();
				while (!adcb_data_is_ready());	// Wait for data to be ready
				int16_t temp0 = adcb_chX_get_temperature(0);
				int16_t temp1 = adcb_chX_get_temperature(1);
				int16_t temp2 = adcb_chX_get_temperature(2);


				// Assigning to permanent variables
				temperature0 = temp0;
				temperature1 = temp1;
				temperature2 = temp2;
				// ********************** START UPDATE SCREEN ************************

				cdc_putstr(int16_tostr(temp0));	//temperature in string form
				udi_cdc_putc('\r');	//return
				udi_cdc_putc('\n');	//newline
				cdc_putstr(int16_tostr(temp1));	//temperature in string form
				udi_cdc_putc('\r');	//return
				udi_cdc_putc('\n');	//newline
				cdc_putstr(int16_tostr(temp2));	//temperature in string form
				udi_cdc_putc('\r');	//return
				udi_cdc_putc('\n');	//newline
				
				snprintf(temp1_string, sizeof(temp1_string), "TMP1:%3iC",
				temperature0);

				snprintf(temp2_string, sizeof(temp2_string), "TMP2:%3iC",
				temperature1);

				snprintf(temp3_string, sizeof(temp3_string), "TMP3:%3iC",
				temperature2);

				snprintf(pressure_string, sizeof(pressure_string), "BAR:%4.3f",
				bar_pressure);

				// TODO: Set up variables and call methods for reading all the values
				// Draw the Celsius string
				gfx_mono_draw_string(temp1_string, 1, 5, &sysfont);	// Temp1
				gfx_mono_draw_string(temp2_string, 64, 5, &sysfont);	// Temp2
				gfx_mono_draw_string(temp3_string, 1, 20, &sysfont);	// Temp3
				gfx_mono_draw_string(pressure_string, 64, 20, &sysfont);	// Pressure
				//*********************** END UPDATE SCREEN ***************************

				//if (twiMaster.status == TWIM_STATUS_READY) {

					twiInt = pressure_val[0];//twiMaster.readData[0];
					twiInt = (twiInt << 8);
					twiInt += pressure_val[1];//twiMaster.readData[1];
					twiInt &= ~(1 << 14);
					twiInt &= ~(1 << 15);

					bar_pressure = pressureval_to_bar(twiInt);

					cdc_putstr("TWI,");
					cdc_putuint32(rtc_timestamp);
					udi_cdc_putc(',');
					cdc_putstr(double_tostr(bar_pressure));
					cdc_putstr("\r\n");
				//}

				keyboard_get_key_state(&input);

			// Wait for key release
			} while (input.type != KEYBOARD_RELEASE);


			// Send key to menu system
			//menu_status = gfx_mono_menu_process_key(&main_menu, input.keycode);
		// Wait for something useful to happen in the menu system
		} while (menu_status == GFX_MONO_MENU_EVENT_IDLE);
	}
}

/*! TWIC Master Interrupt vector. */
ISR(TWIC_TWIM_vect)
{
	TWI_MasterInterruptHandler(&twiMaster);

	if (twiMaster.status == TWIM_STATUS_READY)	// Check if we're done reading data.
	{
		if (twi_reading == PRESSURE)
		{
			pressure_val[0] = twiMaster.readData[0];
			pressure_val[1] = twiMaster.readData[1];
		}
		else if (twi_reading == INTERNAL_TEMPERATURE)
		{
			internal_temp_val[0] = twiMaster.readData[0];
			internal_temp_val[1] = twiMaster.readData[1];
		}
	}
}
