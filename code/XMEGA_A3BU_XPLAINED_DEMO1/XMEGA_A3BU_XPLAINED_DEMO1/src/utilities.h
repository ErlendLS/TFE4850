/*
 * utilities.h
 *
 * Created: 05.03.2014 10:09:06
 *  Author: ErlendLS
 */ 


#ifndef UTILITIES_H_
#define UTILITIES_H_

char * int8_tostr(int8_t intval);

char * int16_tostr(int16_t intval);

char * double_tostr(float dval);

void cdc_putstr(char * string);

void cdc_putuint32(uint32_t value);


#endif /* UTILITIES_H_ */