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
#include "5110.h"
#include "5110.cpp"
#include "keypad.h"

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
//turns the alarm on or off
char alarmpower = 1;
//which button is currently pressed on the keypad
char keypad = 0;
//-----------------------------------------------------------------------------------state machines
//sends and receives usart
enum USART_sm1 { sm1_on, sm1_send, sm1_recieve } usartstate;
//calculates which alarm bit to set and send
enum ALARM_sm2 { sm2_off, sm2_on } alarmstate;
//gets and sets golbal keypad key
enum SETKEYPAD_sm3 { sm3_on } keypadstate;
	 
void Keypad_Tick() {
	//transition
	switch(keypadstate) {
		case sm3_on:
			keypadstate = sm3_on;
			break;
		default:
			break;
	}
	//action
	switch(keypadstate) {
		case sm3_on:
			keypad = GetKeypadKey();
			break;
		default:
			break;
	}
}

//sends and receives usart
void USART_Tick() {
	//transition
	switch(usartstate) {
		case sm1_on:
			usartstate = sm1_send;
			break;
		case sm1_send:
			usartstate = sm1_recieve;
			break;
		case sm1_recieve:
			usartstate = sm1_send;
			break;
		default:
			break;
	}
	//action
	switch(usartstate) {
		case sm1_on:
			break;
		case sm1_send:
			if (USART_IsSendReady(0)) {
				USART_Send(sendsig,0);
			}
			break;
		case sm1_recieve:
			if (USART_HasReceived(1)) {
				recvsig = USART_Receive(1);
			}
			if (USART_HasReceived(0)) {
				temperature = USART_Receive(0);
			}
			break;
		default:
			break;
	}
}

//sets alarm signal depending 
void ALARM_Tick() {
	//transition
	switch(alarmstate) {
		case sm2_off:
			if (alarmpower == 1)
				alarmstate = sm2_on;
			break;
		case sm2_on:
			if (alarmpower == 0)
				alarmstate = sm2_off;
			break;
		default:
			break;
	}
	//action
	/*
	recieve
	usart 1: 0x1234 5678 recvsig

	usart 1: 1 = beam break, 1 = break, 0 = not break
	usart 1: 2 = pid motion, 1 = motion, 0 = no motion
	usart 1: 3 = accelerometer, 1 = fast motion, 0 = no motion
	usart 1: 4 = accelerometer, 1 = slow motion, 0 = no motion
	
	send
	usart 0: 0x1234 5678 sendsig

	usart 0: 1 = window alarm, 1 = on, 0 = off
	usart 0: 2 = room alarm, 1 = on, 0 = off
	usart 0: 3 = motion alarm fast, 1 = on, 0 = off
	usart 0: 4 = motion alarm slow, 1 = on, 0 = off
	*/
	char beam = (recvsig >> 7) & 0x01;
	char motion = (recvsig >> 6) & 0x01;
	char fast = (recvsig >> 5) & 0x01;
	char slow = (recvsig >> 4) & 0x01;
	
	switch(alarmstate) {
		case sm2_off:
			//turn everything off
			sendsig = sendsig & 0x0F;
			break;
		case sm2_on:
			//turn on different alarms depending
			if (beam) {
				sendsig = sendsig | 0x80;
			}
			if (motion) {
				sendsig = sendsig | 0x40;
			}
			if (fast) {
				sendsig = sendsig | 0x20;
			}
			if (slow) {
				sendsig = sendsig | 0x10;
			}
			break;
		default:
			break;
	}
}

//-------------------------------------------------------------------------------state machine inits
void SM1_Init() {
	usartstate = sm1_on;
}
void SM2_INIT() {
	alarmstate = sm2_off;
}

void SM1Task() {
	SM1_Init();
	for(;;) {
		USART_Tick();
		vTaskDelay(100);
	}
}
void SM2Task() {
	SM2_INIT();
	for(;;) {
		ALARM_Tick();
		vTaskDelay(100);
	}
}

void StartSecPulse1(unsigned portBASE_TYPE Priority) {
	xTaskCreate(SM1Task, (signed portCHAR *)"SM1Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}
void StartSecPulse2(unsigned portBASE_TYPE Priority) {
	xTaskCreate(SM2Task, (signed portCHAR *)"SM2Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

int main(void) {
	// initialize ports
	DDRA = 0xFF; PORTA = 0x00;
	DDRB = 0xFF; PORTB = 0x00;
	DDRD = 0xF0; PORTD = 0x0F;
	//inits
	initUSART(0);
	initUSART(1);
	lcd_init(&PORTB, PB0, &PORTB, PB1, &PORTB, PB2, &PORTB, PB3, &PORTB, PB4);
	while(1) {
		char a = GetKeypadKey();
		lcd_chr(a);
	}
	//Start Tasks
	StartSecPulse1(1);
	StartSecPulse2(1);
	//RunSchedular
	vTaskStartScheduler();
	
	return 0;
}
