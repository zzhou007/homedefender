/*	Partner(s) Name & E-mail: Kenneth Chan (kchan049@ucr.edu) and Zihang Zhou(zzhou007@ucr.edu)
 *	Lab Section: 21 
 *	Assignment: Lab #2  Exercise #4 
 *	Exercise Description: [optional - include for your own benefit]
 *	
 *	I acknowledge all content contained herein, excluding template or example
 *	code, is my own original work.
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/portpins.h>
#include <avr/pgmspace.h>

//FreeRTOS include files
#include "FreeRTOS.h"
#include "task.h"
#include "croutine.h"

//other includes
#include "usart_atmega1284.h"

//----------------------------------------------------------------------------------global functions
//analog to digital
void A2D_init() {
	ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
	// ADEN: Enables analog-to-digital conversion
	// ADSC: Starts analog-to-digital conversion
	// ADATE: Enables auto-triggering, allowing for constant
	//	    analog to digital conversions.
}
void Set_A2D_Pin(unsigned char pinNum) {
	ADMUX = (pinNum <= 0x07) ? pinNum : ADMUX;
	// Allow channel to stabilize
	static unsigned char i = 0;
	for ( i=0; i<15; i++ ) { asm("nop"); }
}
//----------------------------------------------------------------------------------global variables
/*
usart 0: 0x1234 5678 sendsig

usart 0: 1 = window alarm, 1 = on, 0 = off
usart 0: 2 = room alarm, 1 = on, 0 = off
usart 0: 3 = motion alarm fast, 1 = on, 0 = off
usart 0: 4 = motion alarm slow, 1 = on, 0 = off

usart 0: 5 = motor1, 1 = on, 0 = off
usart 0: 6 = motor1, 1 = spin right, 0 = spin left
usart 0: 7 = motor2, 1 = on, 0 = off
usart 0: 8 = motor2, 1 = spin right, 0 = spin left
*/
char sendsig = 0;
/*
usart 0: 0x1234 5678 temperature
usart 1: 0x1234 5678 recvsig

usart 0: 1-8 = temp analog

usart 1: 1 = beam break, 1 = break, 0 = not break
usart 1: 2 = pid motion, 1 = motion, 0 = no motion
usart 1: 3 = accelerometer, 1 = fast motion, 0 = no motion 
usart 1: 4 = accelerometer, 1 = slow motion, 0 = no motion
usart 1: 5-8 = photo resistor
*/
char recvsig = 0;
char temperature = 0;
enum Sensor_sm1 { sm1_on } sensorstate;
		 
void Sensor_Tick() {
	//transition
	switch (sensorstate) {
		case sm1_on:
			sensorstate = sm1_on;
			break;
		default:
			break;
	}
	//action
	switch (sensorstate) {
		char A0, A1;
		char output;
		case sm1_on:
			//ir beam
			A0 = PINA & 0x01;
			if (A0)
				output = output | 0x01;
			else
				output = output & 0xFE;
			//motion
			A1 = (PINA & 0x02) >> 1;
			if (A1)
				output = output | 0x02;
			else
				output = output & 0xFD;
			PORTC = output | (ADC << 2);
			break; 
		default:
			break;
			
	}
}

//-------------------------------------------------------------------------------state machine inits
//print
void SM1_INIT() {
	sensorstate = sm1_on;
}

//print
void SM1Task() {
	SM1_INIT();
	for(;;) {
		Sensor_Tick();
		vTaskDelay(50);
	}
}

void StartSecPulse1(unsigned portBASE_TYPE Priority) {
	xTaskCreate(SM1Task, (signed portCHAR *)"SM1Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}
int main(void) {
	// initialize ports
	//everything input
	DDRA = 0x00; PORTA = 0xFF;
	DDRB = 0x00; PORTB = 0xFF;
	DDRC = 0xFF; PORTC = 0x00;
	//inits
	//usart
	initUSART(0);
	initUSART(1);
	//adc
	A2D_init();
	Set_A2D_Pin(0x02);
	//Start Tasks
	StartSecPulse1(1);
	//RunSchedular
	vTaskStartScheduler();
	
	return 0;
}
