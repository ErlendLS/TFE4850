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

/* Main menu: strings are stored in FLASH,
 * the string pointer table and menu struct are stored in RAM.
 */
PROGMEM_DECLARE(char const, main_menu_title[]) = "Main Menu";
PROGMEM_DECLARE(char const, main_menu_1[]) = "Temperature";
PROGMEM_DECLARE(char const, main_menu_2[]) = "Lightsensor";
PROGMEM_DECLARE(char const, main_menu_3[]) = "Production Date";
PROGMEM_DECLARE(char const, main_menu_4[]) = "Date & Time";
PROGMEM_DECLARE(char const, main_menu_5[]) = "Toggle Backlight";

PROGMEM_STRING_T main_menu_strings[] = {
	main_menu_1,
	main_menu_2,
	main_menu_3,
	main_menu_4,
	main_menu_5,
};

static char * cdc_putint8(int8_t intval) {
	char * char_int;
	itoa(intval, char_int, 10);
	
	return char_int;	
}

static char * cdc_putint16(int16_t intval) {
	char * char_int;
	itoa(intval, char_int, 10);
	
	return char_int;
}

static void cdc_putstr(char * string) {
	char val;
	int i = 0;
	while ((val = string[i])) {
		udi_cdc_putc(val);
		i++;
	}
}

static void cdc_putuint32(uint32_t value) {
	char* buffer = (char*)calloc(10,sizeof(char));
	sprintf(buffer,"%u", value);
	cdc_putstr(buffer);
	free(buffer);
}

struct gfx_mono_menu main_menu = {
	// Title
	main_menu_title,
	// Array with menu strings
	main_menu_strings,
	// Number of menu elements
	5,
	// Initial selection
	0
};

void button_splash(void);

/**
 * \brief Show button names on display
 */
void button_splash(void)
{
	struct keyboard_event input;

	// Clear screen
	gfx_mono_draw_filled_rect(0, 0, 128, 32, GFX_PIXEL_CLR);

	gfx_mono_draw_filled_circle(10, 10, 4, GFX_PIXEL_SET, GFX_WHOLE);
	gfx_mono_draw_filled_circle(10, 22, 4, GFX_PIXEL_SET, GFX_WHOLE);
	gfx_mono_draw_filled_circle(117, 10, 4, GFX_PIXEL_SET, GFX_WHOLE);
	gfx_mono_draw_filled_circle(117, 22, 4, GFX_PIXEL_SET, GFX_WHOLE);

	gfx_mono_draw_string("Up", 90, 8, &sysfont);
	gfx_mono_draw_string("Down", 80, 20, &sysfont);
	gfx_mono_draw_string("Enter", 20, 8, &sysfont);
	gfx_mono_draw_string("Back", 20, 20, &sysfont);

	// Any key will exit the loop
	while (true) {
		keyboard_get_key_state(&input);
		if (input.type == KEYBOARD_RELEASE) {
			break;
		}
	}
}

//---Temperature variables---//

//! The thermometer image
uint8_t tempscale_img[] = {
	0x01, 0xf9, 0xfd, 0xfd, 0xf9, 0x01,
	0x41, 0xff, 0xff, 0xff, 0xff, 0x41,
	0x10, 0xff, 0xff, 0xff, 0xff, 0x10,
	0x9e, 0xbf, 0xbf, 0xbf, 0xbf, 0x9e
};

struct gfx_mono_bitmap tempscale;
// String to hold the converted temperature reading
char temperature_string[15];
// Variable to hold the image thermometer scale
uint8_t temp_scale;
// Variable for holding the actual temperature in Celsius
int16_t temperature;

/************************************************************************/
/*  TEMP Placement of ADCB reading support functions                                                               */
/************************************************************************/

// Variables for function below
bool adc_sensor_data_ready = false;
adc_result_t adc_sensor_sample = 0;
#define ADCB_CH0_MAX_SAMPLES 4

int16_t adcb_ch0_get_raw_value(void)
{
	return adc_sensor_sample;
}

const double neg_temp_coeff[9] = {0, 2.5173462E1, -1.1662878E0, -1.0833638E0, -8.9773540E-1, -3.7342377E-1, -8.6632643E-2, -1.0450598E-2, -5.1920577E-4};
const double pos_temp_coeff[9] = {0, 2.508355E1, 7.860106E-2, -2.503131E-1, 8.315270E-2, -1.228034E-2, 9.804036E-4, -4.413030E-5, 1.057734E-6, -1.052755E-8};

double temp_pol_rec(double* coeff, double v, int n)
{	
	int max_n = 9;
	double sum = 0;
	if (n < max_n)
	{
		sum += temp_pol_rec(coeff, v, n + 1);
	}
	
	sum += coeff[n]*pow(v, n);
	
	return sum;
}	


/************************************************************************/
/* This function converts a thermoelectric temperature to a temperature 
 * in the range -200C to +500C                                          */
/************************************************************************/
int16_t thermoel_to_temp(double v)
{
	double* temp_coeff;
	
	if (v >= 0)
	{
		temp_coeff = &pos_temp_coeff;
	}
	else
	{
		temp_coeff = &neg_temp_coeff;
	}
	
	int16_t temp = (int16_t)temp_pol_rec(temp_coeff, v, 0);
	
	return temp;
}

/**
 * \brief Translate raw value into temperature
 *
 *
 */ 
int16_t adcb_ch0_get_temperature(void)
{
	int delta_v = 0.1;
	int16_t top = 4095;	//12-bit max value
	double vref = 2.5;
	int16_t res = adcb_ch0_get_raw_value();
	
	// Calculate vinp
	double vinp = ((double)res/(double)(top+1))*vref - delta_v;

	double off = 0;
	double gain = 1;
	
	double v_tc = (vinp - off)/gain;
	
	int16_t t = thermoel_to_temp(v_tc);
	
	return (int16_t)t;	
}

/************************************************************************/
/* Reads the ADCB CH0                                                                     */
/************************************************************************/
static inline void adcb_ch0_measure(void)
{
	adc_start_conversion(&ADCB, ADC_CH0);
}

/**
 * \brief Check of there is ADCB data ready to be read
 *
 * When data is ready to be read this function will return true, and assume
 * that the data is going to be read so it sets the ready flag to false.
 *
 * \retval true if the ADCB value is ready to be read
 * \retval false if data is not ready yet
 */
bool adcb_data_is_ready(void)
{
	irqflags_t irqflags;
	/* We will need to save and turn of global interrupts to make sure that we
	read the latest adcb value and not the next one if a conversation finish
	before one have time read the data. */
	irqflags = cpu_irq_save();
	if (adc_sensor_data_ready) {
		adc_sensor_data_ready = false;
		cpu_irq_restore(irqflags);
		return true;
	} else {
		cpu_irq_restore(irqflags);
		return false;
	}
}

/************************************************************************/
/*  Temperature display function. Applies to ADCA as well as this test  */
/************************************************************************/

void temp_disp_init()
{	
	/* TODO: Replaced for testing
	// Initiate a temperature sensor reading
	ntc_measure();
	*/
	// Initiate a ADCB reading
	adcb_ch0_measure();
	
	// Struct for holding the temperature scale background
	tempscale.type = GFX_MONO_BITMAP_RAM;
	tempscale.width = 6;
	tempscale.height = 32;
	tempscale.data.pixmap = tempscale_img;

	// Screen border
	gfx_mono_draw_rect(0, 0, 128, 32, GFX_PIXEL_SET);
	// Clear screen
	gfx_mono_draw_filled_rect(1, 1, 126, 30, GFX_PIXEL_CLR);
	
	//Paint thermometer on screen
	gfx_mono_put_bitmap(&tempscale, 10, 0);
	
	/* TODO: Simply replaced for testing
	// wait for NTC data to ready
	while (!ntc_data_is_ready());
	// Read the temperature once the ADC reading is done
	temperature = ntc_get_temperature();
	*/
	// Wait for ADCB date to ready
	while (!adcb_data_is_ready());
	temperature = adcb_ch0_get_temperature();//adcb_ch0_get_raw_value();
	
	
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
}

/************************************************************************/
/* END Temp display function                                            */
/************************************************************************/




/**
 * \brief Callback for the ADC conversion complete
 *
 * The ADC module will call this function on a conversion complete.
 *
 * \param adc the ADC from which the interrupt came
 * \param ch_mask the ch_mask that produced the interrupt
 * \param result the result from the ADC
 */
void adcb_handler(ADC_t *adc, uint8_t ch_mask, adc_result_t result)
{
	static uint8_t ch0_sensor_samples = 0;

	// UNKNOWN on channel 0 
	if (ch_mask == ADC_CH0) {
		// TODO: Read sample
		
		ch0_sensor_samples++;
		if (ch0_sensor_samples == 1) {
			adc_sensor_sample = result;
			adc_sensor_data_ready = false;
		} else {
			adc_sensor_sample += result;
			adc_sensor_sample >>= 1;
		}
		if (ch0_sensor_samples == ADCB_CH0_MAX_SAMPLES) {
			ch0_sensor_samples = 0;
			adc_sensor_data_ready = true;
		} else {
			adcb_ch0_measure();
		}
		
	} else if (ch_mask == ADC_CH1) {
		// There is no CH1 atm!
		/*
		ntc_sensor_samples++;
		if (ntc_sensor_samples == 1) {
			ntc_sensor_sample = result;
			ntc_sensor_data_ready = false;
		} else {
			ntc_sensor_sample += result;
			ntc_sensor_sample >>= 1;
		}
		if (ntc_sensor_samples == NTC_SENSOR_MAX_SAMPLES) {
			ntc_sensor_samples = 0;
			ntc_sensor_data_ready = true;
		} else {
			ntc_measure();
		*/
	}
}

/************************************************************************/
/* Initializes the adc_b for reading external sensors.
   Should be moved to own file with headers etc. when completed and tested
                                                                        */
/************************************************************************/
void adc_b_sensors_init()
{
	struct adc_config adc_conf;
	struct adc_channel_config adc_ch_conf;

	/* Clear the ADC configuration structs */
	adc_read_configuration(&ADCB, &adc_conf);
	adcch_read_configuration(&ADCB, ADC_CH0, &adc_ch_conf);
	
	/* configure the ADCB module:
	- unsigned, 12-bit resolution
	- TODO: Refer to ext. voltage reference instead of internal VCC / 1.6 reference
	- 31 kHz max clock rate
	- manual conversion triggering
	- callback function
	*/
	adc_set_conversion_parameters(&adc_conf, ADC_SIGN_OFF, ADC_RES_12,
			ADC_REF_AREFB);	// Reference voltage might have to be set to ..._AREFB_gc instead
	adc_set_clock_rate(&adc_conf, 125000UL);
	adc_set_conversion_trigger(&adc_conf, ADC_TRIG_MANUAL, 0, 0);
	adc_write_configuration(&ADCB, &adc_conf);
	adc_set_callback(&ADCB, &adcb_handler);
	
	/* Configure ADC B channel 0 (test source):
	 * - single-ended measurement
	 * - interrupt flag set on completed conversion
	 * - interrupts enabled
	 */
	adcch_set_input(&adc_ch_conf, ADCCH_POS_PIN1, ADCCH_NEG_NONE, 1);
	adcch_set_interrupt_mode(&adc_ch_conf, ADCCH_MODE_COMPLETE);
	adcch_enable_interrupt(&adc_ch_conf);
	adcch_write_configuration(&ADCB, ADC_CH0, &adc_ch_conf);
	
	// Enable ADC
	adc_enable(&ADCB);
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
	
	/* Main loop. Read keyboard status and pass input to menu system.
	 * When an element has been selected in the menu, it will return the
	 * index of the element that should be run. This can be an application
	 * or another menu.
	 */
	while (true) {
		
		/*ORIGINAL CODE
		// (re)initialize menu system
		gfx_mono_menu_init(&main_menu);
		*/

		do {			
			do {
				//START TEMP PRINT
				_delay_ms(10000);	//NOTE: ms actually means microseconds in this case
				rtc_timestamp = rtc_get_time();
				{
					//TEST: Print temperature to udi_cdc
					
					//ID
					char * logid = "NTC_OC";
					cdc_putstr(logid);	//Identify sample as on-chip NTC temp.
					
					//Data separator character
					udi_cdc_putc(',');
					
					//Timestamp
					cdc_putuint32(rtc_timestamp);
					
					//Data separator character
					udi_cdc_putc(',');
					
					//Temperature
					//ntc_measure();
					// Testing
					adcb_ch0_measure();
					//int8_t temp = ntc_get_temperature();
					while (!adcb_data_is_ready());
					int16_t temp = adcb_ch0_get_temperature();
					char * temp_s = cdc_putint16(temp);
					cdc_putstr(temp_s);	//temperature in string form
					udi_cdc_putc('\r');	//return
					udi_cdc_putc('\n');	//newline
					//END TEST
				}
				//END TEMP PRINT
				
				//START Drawing Temperature:
				
				//Paint thermometer on screen
				gfx_mono_put_bitmap(&tempscale, 10, 0);
				
				/*
				// wait for NTC data to ready
				while (!ntc_data_is_ready());
				// Read the temperature once the ADC reading is done
				temperature = ntc_get_temperature();
				*/
				// ADCB-Testing
				adcb_ch0_measure();
				while (!adcb_data_is_ready());
				int16_t temp = adcb_ch0_get_temperature();
				
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
				
				keyboard_get_key_state(&input);
				
			// Wait for key release
			} while (input.type != KEYBOARD_RELEASE);

		
			// Send key to menu system
			//menu_status = gfx_mono_menu_process_key(&main_menu, input.keycode);
		// Wait for something useful to happen in the menu system
		} while (menu_status == GFX_MONO_MENU_EVENT_IDLE);
		
		/* DEMO CODE
		switch (menu_status) {
		case 0:
			ntc_sensor_application();
			break;
		case 1:
			lightsensor_application();
			break;
		case 2:
			production_date_application();
			break;
		case 3:
			date_time_application();
			break;
		case 4:
			// Toggle LCD Backlight
			gpio_toggle_pin(NHD_C12832A1Z_BACKLIGHT);
			break;
		case GFX_MONO_MENU_EVENT_EXIT:
			// Fall trough to default and show button splash
		default:
			button_splash();
			break;
		};
		ntc_sensor_application();
		*/
	}
}
