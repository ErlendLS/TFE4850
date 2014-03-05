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
 * This is the application that is shipped with the XMEGA-A3BU Xplained
 * kits. It consists of a simple menu system and applications demonstrating
 * the features on the kit.
 *
 * The following application are available in the main menu:
 * - "Temperature",  Display the temperature measured by the connected NTC
 *                   sensor.
 * - "Lightsensor",  Display measurements from the connected lightsensor
 * - "Production Date", Display the time elapsed since the part left the
 *                      factory.
 * - "Date & time" Display the current date & time
 * - "Set Timezone", Select your timezone +- UTC
 * - "Toggle backlight", Toggle the LCD backlight
 *
 * \section files Main files:
 * - main.c  Main application
 * - bitmaps.c Bitmaps used in the application
 * - keyboard.c Keyboard driver
 * - ntc_sensor.c Temperature sensor application
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

TWI_Master_t* twi;
TWI_t* module;

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
	touch_init();
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
	PORTE.DIRSET = 0xFF;
	PORTD.DIRCLR = 0xFF;
	PORTCFG.MPCMASK = 0xFF;
	PORTD.PIN0CTRL |= PORT_INVEN_bm;
	
	TWI_MasterInit(twi, module, TWI_MASTER_INTLVL_LO_gc, 400);	
	
	/* Enable LO interrupt level. */
	PMIC.CTRL |= PMIC_LOLVLEN_bm;
	sei();
	
	/* Main loop.
	 * Reads and interprets sensors. Sends data for logging.
	 */
	while (true) {

		do {			
			do {
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
					
					// Testing
					adcb_ch0_measure();

					while (!adcb_data_is_ready());
					int16_t temp = adcb_ch0_get_temperature();
					char * temp_s = cdc_putint16(temp);
					cdc_putstr(temp_s);	//temperature in string form
					udi_cdc_putc('\r');	//return
					udi_cdc_putc('\n');	//newline
				}
				
				
				//Paint thermometer on screen
				gfx_mono_put_bitmap(&tempscale, 10, 0);
				
				// ADCB-Testing
				adcb_ch0_measure();
				while (!adcb_data_is_ready());	// Wait for data to be ready
				int16_t temp = adcb_ch0_get_temperature();
				
				// TODO
				temperature = temp;
				// Convert the temperature into the thermometer scale
				temp_scale = -0.36 * temperature + 20.25;
				if (temp_scale <= 0) {
					temp_scale = 0;
				}
				
				// Draw the scale element on top of the background temperature image
				gfx_mono_draw_filled_rect(12, 3, 2, temp_scale,
				GFX_PIXEL_CLR);
				
				snprintf(temperature_string, sizeof(temperature_string), "%3i Celsius",
				temperature);

				// Draw the Celsius string
				gfx_mono_draw_string(temperature_string, 22, 13, &sysfont);
				//END Draw temperature
				
				/* Enable LO interrupt level. */
				PMIC.CTRL |= PMIC_LOLVLEN_bm;
				sei();
				
				//Write I2C status
				bool twiStatus = TWI_MasterRead(twi, 0x28, 4);
				
				while (twi->status != TWIM_STATUS_READY) {
					/* Wait until transaction is complete. */
				}
				
				uint16_t twiInt = twi->readData[0];
				(twiInt << 8);
				twiInt += twi->readData[1];
				twiInt &= ~(1 << 14);
				twiInt &= ~(1 << 15);
				
				cdc_putstr(cdc_putint16(twi->result));
				cdc_putstr("\r\n");
				cdc_putstr(cdc_putint16(twiInt));
				cdc_putstr("\r\n");
				
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
	TWI_MasterInterruptHandler(twi);
}
