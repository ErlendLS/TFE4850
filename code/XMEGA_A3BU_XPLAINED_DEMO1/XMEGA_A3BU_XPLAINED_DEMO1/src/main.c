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
//#include "keyboard.h"
//#include "ntc_sensor.h"
#include "production_date.h"
#include "timezone.h"
//#include "touch_api.h"
#include "cdc.h"
#include "conf_application.h"
#include "sysfont.h"
#include "twi_master_driver.h"
#include "utilities.h"
#include "sensors.h"
#include "tc.h"

TWI_Master_t twiMaster;
uint16_t twiInt = 0xFFFF;

register8_t pressure_val[2] = {0,0};
register8_t internal_temp_val[2] = {0,0};

enum TWI_READING { PRESSURE , INTERNAL_TEMPERATURE , NONE } twi_reading;

int delay_counter = 0;
int twi_counter = 0;
int screen_counter = 0;
unsigned long long time_counter = 0; //You shall not overflow!

static void adc_callback(void)
{
	delay_counter++; //Decides when to do the rest of the while loop
	twi_counter++; //Decides when to perform a twi reading
	screen_counter++; //Decides when to update the screen
	time_counter++; //The timestamp
	adcb_ch0_measure();
	adcb_ch1_measure();
	adcb_ch2_measure();
	//The adc must do stuff here 200 times a second
}

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
	char char_double[32];
	char char_int[32];

	sysclk_init();
	board_init();
	pmic_init();
	gfx_mono_init();
	cpu_irq_enable();
	//touch_init();		// TODO: Can probably be removed seeing as we're not going to use it at this time
	//adc_sensors_init();

	/************************************************************************/
	/* Start timer initialization                                                                     */
	/************************************************************************/
	tc_enable(&TCC0);
	tc_set_overflow_interrupt_callback(&TCC0, adc_callback);
	tc_set_wgm(&TCC0, TC_WG_NORMAL);
	tc_write_period(&TCC0, 60000);
	tc_set_overflow_interrupt_level(&TCC0, TC_INT_LVL_LO);
	tc_write_clock_source(&TCC0, TC_CLKSEL_DIV2_gc);
	/************************************************************************/
	/* Stop timer initialization                                                                     */
	/************************************************************************/


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

	// Display a splash screen showing button functions
	//button_splash();

	// Set timezone from EEPROM or to a default value
	timezone_init();

	// Initiate calibration values for temperature
	temp_ch_calibration_setup();

	// ADDED: Initializing temperature display
	temp_disp_init();

	// 26.02.14 TWI init
	sysclk_enable_peripheral_clock(&TWIC);
	TWI_MasterInit(&twiMaster, &TWIC, TWI_MASTER_INTLVL_LO_gc, 400);

	// TODO: Is this appropriate to have both here and inside the loop?
	/* Enable LO interrupt level. */
	PMIC.CTRL |= PMIC_LOLVLEN_bm;
	sei();

	// Initializing internal temp sensor
	while(twiMaster.status != TWIM_STATUS_READY);		// Wait for initialization
	Byte configData[2] = {0x03, 0x40};					// Write 1 to bit 7 of register address 0x03 in the internal temp sensor
	TWI_MasterWrite(&twiMaster, 0x48, configData, 2);	// Configure internal temp sensor to 16-bit instead of default 13-bit
	while(twiMaster.status != TWIM_STATUS_READY);		// Wait for  write to complete

	/* Main loop.
	 * Reads and interprets sensors. Sends data for logging.
	 */
	while (true) {

		do {
			do {

				//Delay 0.25 seconds
				if(twi_counter >= 50){
					twi_counter = 0;
					/* Enable LO interrupt level. */
					PMIC.CTRL |= PMIC_LOLVLEN_bm;
					sei();

					//Write I2C status
					if(twiMaster.status == TWIM_STATUS_READY && twi_reading == PRESSURE)
					{
						bool twiStatus = TWI_MasterRead(&twiMaster, 0x48, 2); // Reading internal temperature
						twi_reading = INTERNAL_TEMPERATURE;					  // TODO : Move these two to a discrete function to remove chance of fucking up
					}
					else if (twiMaster.status == TWIM_STATUS_READY && twi_reading == INTERNAL_TEMPERATURE)
					{

						bool twiStatus = TWI_MasterRead(&twiMaster, 0x28, 4); // Reading pressure
						twi_reading = PRESSURE;								  // TODO : Move these two to a discrete function to remove chance of fucking up
					}
				}

				//Delay 0.05 seconds
				if(delay_counter >= 10){
					delay_counter = 0;

					//START TEMP PRINT
					rtc_timestamp = rtc_get_time();
					double timestamp = (double)(time_counter)/200;

					// ADCB-Testing
					while (!adcb_data_is_ready());	// Wait for data to be ready
					int16_t temp0 = adcb_chX_get_temperature(0);
					int16_t temp1 = adcb_chX_get_temperature(1);
					int16_t temp2 = adcb_chX_get_temperature(2);


					// Assigning to permanent variables
					temperature0 = temp0;
					temperature1 = temp1;
					temperature2 = temp2;

					//TODO: Set a proper id for the log sample. (So it can be properly recognized)
					char * logid = "ADC0";
					cdc_putstr(logid);	//Identify sample
					//Data separator character
					udi_cdc_putc(',');
					//Timestamp
					sprintf(char_double, "%f", timestamp);
					cdc_putstr(char_double);
					udi_cdc_putc(',');
					cdc_putstr(int16_tostr(temp0));	//temperature in string form
					udi_cdc_putc('\r');	//return
					udi_cdc_putc('\n');	//newline

					logid = "ADC1";
					cdc_putstr(logid);
					udi_cdc_putc(',');
					sprintf(char_double, "%f", timestamp);
					cdc_putstr(char_double);
					udi_cdc_putc(',');
					cdc_putstr(int16_tostr(temp1));	//temperature in string form
					udi_cdc_putc('\r');	//return
					udi_cdc_putc('\n');	//newline

					logid = "ADC2";
					cdc_putstr(logid);
					udi_cdc_putc(',');
					sprintf(char_double, "%f", timestamp);
					cdc_putstr(char_double);
					udi_cdc_putc(',');
					cdc_putstr(int16_tostr(temp2));	//temperature in string form
					udi_cdc_putc('\r');	//return
					udi_cdc_putc('\n');	//newline

					// Converting pressure into int16
					twiInt = pressure_val[0];//twiMaster.readData[0];
					twiInt = (twiInt << 8);
					twiInt += pressure_val[1];//twiMaster.readData[1];
					twiInt &= ~(1 << 14);
					twiInt &= ~(1 << 15);

					bar_pressure = pressureval_to_bar(twiInt);

					//Data to be printed
					cdc_putstr("TWI_PRESSURE,");
					//Put timestamp
					sprintf(char_double, "%f", timestamp);
					cdc_putstr(char_double);
					udi_cdc_putc(',');
					//Put data
					sprintf(char_double, "%f", bar_pressure);
					cdc_putstr(char_double);
					cdc_putstr("\r\n");

					// Convert internal sensor data
					int16_t twiTemp = internal_temp_val[0];
					twiTemp = (twiTemp << 8);
					twiTemp += internal_temp_val[1];

					double internal_temperature_code;
					if (twiTemp < 0)	// Negative temperature
					{
						twiTemp *= -1;	// Making value positive
						internal_temperature_code = (twiTemp - 65536);
					}
					else
					{
						internal_temperature_code = twiTemp;
					}

					update_internal_voltage_offset(internal_temperature_code);

					keyboard_get_key_state(&input);
				}

				// ********************** START UPDATE SCREEN ************************
				if(screen_counter >= 100){
					screen_counter = 0;

					snprintf(temp1_string, sizeof(temp1_string), "TMP1:%3iC",
					temperature0);

					snprintf(temp2_string, sizeof(temp2_string), "TMP2:%3iC",
					temperature1);

					snprintf(temp3_string, sizeof(temp3_string), "TMP3:%3iC",
					temperature2);

					snprintf(pressure_string, sizeof(pressure_string), "BAR:%3.2f",
					bar_pressure);

					// TODO: Set up variables and call methods for reading all the values
					// Draw the Celsius string
					gfx_mono_draw_string(temp1_string, 1, 5, &sysfont);	// Temp1
					gfx_mono_draw_string(temp2_string, 64, 5, &sysfont);	// Temp2
					gfx_mono_draw_string(temp3_string, 1, 20, &sysfont);	// Temp3
					gfx_mono_draw_string(pressure_string, 64, 20, &sysfont);	// Pressure
				}
				//*********************** END UPDATE SCREEN ***************************

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
