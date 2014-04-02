/*
 * sensors.h
 *
 * Created: 05.03.2014 10:07:39
 *  Author: ErlendLS
 */


#ifndef SENSORS_H_
#define SENSORS_H_

uint8_t tempscale_img[];
struct gfx_mono_bitmap tempscale;
int16_t temperature0;
int16_t temperature1;
int16_t temperature2;
int16_t temperature3;
double bar_pressure;
//char temperature_string[15];
char temp1_string[15];
char temp2_string[15];
char temp3_string[15];
char pressure_string[15];
uint8_t temp_scale;

// Calibration struct for temperature channels
struct temp_ch_calibration;

int16_t adcb_ch0_get_raw_value(void);

void update_internal_voltage_offset(double);

double temp_pol_rec(double* coeff, double v, int n);

double internal_temp_pol_rec(double* coeff, double temperature, int n);

double pressureval_to_bar(int16_t val);

/************************************************************************/
/* This function converts a thermoelectric temperature to a temperature
 * in the range -200C to +500C                                          */
/************************************************************************/
int16_t thermoel_to_temp(double v);

/**
 * \brief Translate raw value into temperature
 *
 *
 */
int16_t adcb_chX_get_temperature(int);

/************************************************************************/
/* Reads the ADCB CH0 - CH3                                                                   */
/************************************************************************/
void adcb_ch0_measure(void);

void adcb_ch1_measure(void);

void adcb_ch2_measure(void);

void adcb_ch3_measure(void);

/**
 * \brief Check of there is ADCB data ready to be read
 *
 * When data is ready to be read this function will return true, and assume
 * that the data is going to be read so it sets the ready flag to false.
 *
 * \retval true if the ADCB value is ready to be read
 * \retval false if data is not ready yet
 */
bool adcb_data_is_ready(void);

/************************************************************************/
/*  Temperature display function. Applies to ADCA as well as this test  */
/************************************************************************/

void temp_disp_init();


/**
 * \brief Callback for the ADC conversion complete
 *
 * The ADC module will call this function on a conversion complete.
 *
 * \param adc the ADC from which the interrupt came
 * \param ch_mask the ch_mask that produced the interrupt
 * \param result the result from the ADC
 */
void adcb_handler(ADC_t *adc, uint8_t ch_mask, adc_result_t result);

/************************************************************************/
/* Initializes the adc_b for reading external sensors.
   Should be moved to own file with headers etc. when completed and tested
                                                                        */
/************************************************************************/
void adc_b_sensors_init();


#endif /* SENSORS_H_ */