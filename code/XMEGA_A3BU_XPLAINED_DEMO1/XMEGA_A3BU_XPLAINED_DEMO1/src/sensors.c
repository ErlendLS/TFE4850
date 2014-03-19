/*
 * sensors.c
 *
 * Created: 05.03.2014 10:07:18
 *  Author: ErlendLS
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <gfx_mono.h>
#include <gfx_mono_menu.h>
#include <gpio.h>
#include <sysfont.h>
#include "cdc.h"
#include "sysfont.h"
#include "adc_sensors.h"
#include "adc.h"
#include "utilities.h"
#include "sensors.h"

#define ADCB_CH0_MAX_SAMPLES 4
#define ADCB_CH1_MAX_SAMPLES 4
#define ADCB_CH2_MAX_SAMPLES 4
#define ADCB_CH3_MAX_SAMPLES 4

bool adc_sensor_data_ready_ch0 = false;
bool adc_sensor_data_ready_ch1 = false;
bool adc_sensor_data_ready_ch2 = false;
bool adc_sensor_data_ready_ch3 = false;

adc_result_t adc_sensor_sample_ch0 = 0;
adc_result_t adc_sensor_sample_ch1 = 0;
adc_result_t adc_sensor_sample_ch2 = 0;
adc_result_t adc_sensor_sample_ch3 = 0;

//! The thermometer image
uint8_t tempscale_img[] = {
	0x01, 0xf9, 0xfd, 0xfd, 0xf9, 0x01,
	0x41, 0xff, 0xff, 0xff, 0xff, 0x41,
	0x10, 0xff, 0xff, 0xff, 0xff, 0x10,
	0x9e, 0xbf, 0xbf, 0xbf, 0xbf, 0x9e
};

// Calibration structs for temperature channels
temp_ch_calibration temp_ch_0_low;
temp_ch_calibration temp_ch_0_high;
temp_ch_calibration temp_ch_1_low;
temp_ch_calibration temp_ch_1_high;
temp_ch_calibration temp_ch_2_low;
temp_ch_calibration temp_ch_2_high;
temp_ch_calibration temp_ch_3_low;			// There are only 3 channels in use at the moment, so this is just to support future extension
temp_ch_calibration temp_ch_3_high;
temp_ch_calibration temp_ch_internal_low;	// Own struct for the internal temperature sensor, may differ slightly from the others
temp_ch_calibration temp_ch_internal_high;


/************************************************************************/
/* 2D Array for holding the calibration structs.
  Indexed as follows: 0 = ch0, 1 = ch1 etc. 4 = internal
  Second index is 0 = low calibration point, 1 = high point             */
/************************************************************************/
temp_ch_calibration temp_ch_calibration_arr[5][2];


struct gfx_mono_bitmap tempscale;
// String to hold the converted temperature reading
//char temperature_string[15];
// Variables to hold temperature and pressure
char temp1_string[15];
char temp2_string[15];
char temp3_string[15];
char pressure_string[15];
// Variable to hold the image thermometer scale
uint8_t temp_scale;
// Variable for holding the actual temperature in Celsius
int16_t temperature;
// Variable for holding the pressure in bar
double bar_pressure;

void temp_ch_calibration_setup()
{
	temp_ch_internal_low.gain = 1;
	temp_ch_internal_low.offset = 0;
	temp_ch_internal_high.gain = 1;
	temp_ch_internal_high.offset = 0;

	temp_ch_calibration_arr[4][0] = temp_ch_internal_low;
	temp_ch_calibration_arr[4][1] = temp_ch_internal_high;

	temp_ch_0_low.gain = 1;
	temp_ch_0_low.offset = 0;
	temp_ch_0_high.gain = 1;
	temp_ch_0_high.offset = 0;

	temp_ch_calibration_arr[0][0] = temp_ch_0_low;
	temp_ch_calibration_arr[0][1] = temp_ch_0_high;

	temp_ch_1_low.gain = 1;
	temp_ch_1_low.offset = 0;
	temp_ch_1_high.gain = 1;
	temp_ch_1_high.offset = 0;

	temp_ch_calibration_arr[1][0] = temp_ch_1_low;
	temp_ch_calibration_arr[1][1] = temp_ch_1_high;

	temp_ch_2_low.gain = 1;
	temp_ch_2_low.offset = 0;
	temp_ch_2_high.gain = 1;
	temp_ch_2_high.offset = 0;

	temp_ch_calibration_arr[2][0] = temp_ch_2_low;
	temp_ch_calibration_arr[2][1] = temp_ch_2_high;

	temp_ch_3_low.gain = 1;
	temp_ch_3_low.offset = 0;
	temp_ch_3_high.gain = 1;
	temp_ch_3_high.offset = 0;

	temp_ch_calibration_arr[3][0] = temp_ch_3_low;
	temp_ch_calibration_arr[3][1] = temp_ch_3_high;

}

int16_t adcb_ch0_get_raw_value(void)
{
	return adc_sensor_sample_ch0;
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
/* Function to convert pressure-value from Honeywell I2C pressure sensor
 * to Bar                                                               */
/************************************************************************/
double pressureval_to_bar(int16_t val)
{
	int16_t output = val;
	int16_t output_max = 0x3999;
	int16_t output_min = 0x0666;
	int pressure_max = 15;	// Max pressure value in psi
	int pressure_min = 0;	// Min pressure value in psi

	double psi = ((double)((output-output_min)*(pressure_max-pressure_min)))/(double)(output_max-output_min) + (double)pressure_min;

	double bar = psi*0.0689475729;

	return bar;
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
void adcb_ch0_measure(void)
{
	adc_start_conversion(&ADCB, ADC_CH0);
}

void adcb_ch1_measure(void)
{
	adc_start_conversion(&ADCB, ADC_CH1);
}

void adcb_ch2_measure(void)
{
	adc_start_conversion(&ADCB, ADC_CH2);
}

void adcb_ch3_measure(void)
{
	adc_start_conversion(&ADCB, ADC_CH3);
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
	if (adc_sensor_data_ready_ch0) {
		adc_sensor_data_ready_ch0 = false;
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

	// Paint thermometer on screen
	// gfx_mono_put_bitmap(&tempscale, 10, 0);	// TODO: Can be removed.

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
	/* temp_scale = -0.36 * temperature + 20.25;
	if (temp_scale <= 0) {
		temp_scale = 0;
	}
	*/

	// Draw the scale element on top of the background temperature image
	/*gfx_mono_draw_filled_rect(12, 3, 2, temp_scale,
	GFX_PIXEL_CLR);
	*/

	/*snprintf(temperature_string, sizeof(temperature_string), "%3i C",
	temperature);

	// Draw the Celsius string
	gfx_mono_draw_string(temperature_string, 22, 13, &sysfont);
	*/
	// ********************** START UPDATE SCREEN ************************

	snprintf(temp1_string, sizeof(temp1_string), "TMP1:%3iC",
	temperature);

	snprintf(temp2_string, sizeof(temp2_string), "TMP2:%3iC",
	temperature);

	snprintf(temp3_string, sizeof(temp3_string), "TMP3:%3iC",
	temperature);

	snprintf(pressure_string, sizeof(pressure_string), "BAR:%3i",
	temperature);

	// TODO: Set up variables and call methods for reading all the values
	// Draw the Celsius string
	gfx_mono_draw_string(temp1_string, 1, 5, &sysfont);	// Temp1
	gfx_mono_draw_string(temp2_string, 64, 5, &sysfont);	// Temp2
	gfx_mono_draw_string(temp3_string, 1, 20, &sysfont);	// Temp3
	gfx_mono_draw_string(pressure_string, 64, 20, &sysfont);	// Pressure
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
	static uint8_t ch1_sensor_samples = 0;
	static uint8_t ch2_sensor_samples = 0;
	static uint8_t ch3_sensor_samples = 0;

	// Temperature on channel 0-3
	if (ch_mask == ADC_CH0) {
		// TODO: Read sample

		ch0_sensor_samples++;
		if (ch0_sensor_samples == 1) {
			adc_sensor_sample_ch0 = result;
			adc_sensor_data_ready_ch0 = false;
		} else {
			adc_sensor_sample_ch0 += result;
			adc_sensor_sample_ch0 >>= 1;
		}
		if (ch0_sensor_samples == ADCB_CH0_MAX_SAMPLES) {
			ch0_sensor_samples = 0;
			adc_sensor_data_ready_ch0 = true;
		} else {
			adcb_ch0_measure();
		}

	} else if (ch_mask == ADC_CH1) {
		ch1_sensor_samples++;
		if (ch1_sensor_samples == 1) {
			adc_sensor_sample_ch1 = result;
			adc_sensor_data_ready_ch1 = false;
			} else {
			adc_sensor_sample_ch1 += result;
			adc_sensor_sample_ch1 >>= 1;
		}
		if (ch1_sensor_samples == ADCB_CH1_MAX_SAMPLES) {
			ch1_sensor_samples = 0;
			adc_sensor_data_ready_ch1 = true;
			} else {
			adcb_ch1_measure();
		}
	} else if (ch_mask == ADC_CH2) {
		ch2_sensor_samples++;
		if (ch2_sensor_samples == 1) {
			adc_sensor_sample_ch2 = result;
			adc_sensor_data_ready_ch2 = false;
			} else {
			adc_sensor_sample_ch2 += result;
			adc_sensor_sample_ch2 >>= 1;
		}
		if (ch2_sensor_samples == ADCB_CH2_MAX_SAMPLES) {
			ch2_sensor_samples = 0;
			adc_sensor_data_ready_ch2 = true;
			} else {
			adcb_ch2_measure();
		}
	} else if (ch_mask == ADC_CH3) {
		ch3_sensor_samples++;
		if (ch3_sensor_samples == 1) {
			adc_sensor_sample_ch3 = result;
			adc_sensor_data_ready_ch3 = false;
			} else {
			adc_sensor_sample_ch3 += result;
			adc_sensor_sample_ch3 >>= 1;
		}
		if (ch3_sensor_samples == ADCB_CH1_MAX_SAMPLES) {
			ch3_sensor_samples = 0;
			adc_sensor_data_ready_ch3 = true;
			} else {
			adcb_ch3_measure();
		}
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
	adcch_read_configuration(&ADCB, ADC_CH1, &adc_ch_conf); //TODO Needed?
	adcch_read_configuration(&ADCB, ADC_CH2, &adc_ch_conf); //TODO Needed?
	adcch_read_configuration(&ADCB, ADC_CH3, &adc_ch_conf); //TODO Needed?

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
	adcch_set_input(&adc_ch_conf, ADCCH_POS_PIN3, ADCCH_NEG_NONE, 1);
	adcch_set_interrupt_mode(&adc_ch_conf, ADCCH_MODE_COMPLETE);
	adcch_enable_interrupt(&adc_ch_conf);
	adcch_write_configuration(&ADCB, ADC_CH0, &adc_ch_conf);

	// Configure ADC B channel 1 (test source):
	adcch_set_input(&adc_ch_conf, ADCCH_POS_PIN1, ADCCH_NEG_NONE, 1);
	adcch_write_configuration(&ADCB, ADC_CH1, &adc_ch_conf);

	// Configure ADC B channel 2 (test source):
	adcch_set_input(&adc_ch_conf, ADCCH_POS_PIN2, ADCCH_NEG_NONE, 1);
	adcch_write_configuration(&ADCB, ADC_CH2, &adc_ch_conf);

	// Configure ADC B channel 3 (test source):
	adcch_set_input(&adc_ch_conf, ADCCH_POS_PIN12, ADCCH_NEG_NONE, 1);
	adcch_write_configuration(&ADCB, ADC_CH3, &adc_ch_conf);

	// Enable ADC
	adc_enable(&ADCB);
}
