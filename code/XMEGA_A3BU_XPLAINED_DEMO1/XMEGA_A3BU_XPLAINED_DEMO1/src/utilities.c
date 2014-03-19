/*
 * utilities.c
 *
 * Created: 05.03.2014 10:08:50
 *  Author: ErlendLS
 */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "cdc.h"
#include "utilities.h"

char * int8_tostr(int8_t intval) {
	char * char_int;
	itoa(intval, char_int, 10);

	return char_int;
}

char * int16_tostr(int16_t intval) {
	char * char_int;
	itoa(intval, char_int, 10);

	return char_int;
}

char * double_tostr(double dval)
{
	char * char_double;
	sprintf(char_double, "%f", dval);

	return char_double;
}

void double_tostrAlt(char * string, double dval)
{
	sprintf(char_double, "%d", dval);
}

void cdc_putstr(char * string) {
	char val;
	int i = 0;
	while ((val = string[i])) {
		udi_cdc_putc(val);
		i++;
	}
}

void cdc_putuint32(uint32_t value) {
	char* buffer = (char*)calloc(10,sizeof(char));
	sprintf(buffer,"%u", value);
	cdc_putstr(buffer);
	free(buffer);
}

