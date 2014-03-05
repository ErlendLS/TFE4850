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
int16_t temperature;
char temperature_string[15];
uint8_t temp_scale;

int16_t adcb_ch0_get_raw_value(void);

double temp_pol_rec(double* coeff, double v, int n);

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
int16_t adcb_ch0_get_temperature(void);

/************************************************************************/
/* Reads the ADCB CH0                                                                     */
/************************************************************************/
void adcb_ch0_measure(void);

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