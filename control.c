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
usart 1: 0x1234 5678 recvsig

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
//the string to print to the screen
//screen 6x14 char
char screen[6][14] = {
	{' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
	{' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
	{' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
	{' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
	{' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
	{' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '}
};
char prevscreen[6][14] = {
	{' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
	{' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
	{' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
	{' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
	{' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
	{' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '}
};

//if light > 57 turn alarm off
short light = 0;
//the temperature in C
short tempC = 0;
//----------------------------------------------------------------------------------global functions
/* compares screen with previous output
1 if different 0 if same
sets prevscreen to new screen if different*/
char comparescreen() {
	char changed = 0;
	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < 14; j++) {
			if (screen[i][j] != prevscreen[i][j]) {
				lcd_chr('c');
				changed = 1;
			}
		}
	}
	if (changed) {
		for (int i = 0; i < 6; i++) {
			for (int j = 0; j < 14; j++) {
				prevscreen[i][j] = screen[i][j];
			}
		}
		return 1;
	} else {
		return 0;
	}
}
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
	for ( i=0; i<100; i++ ) { asm("nop"); }
}
//writes an array to the screen
void screenarray(char* a) {
	int i = 0;
	int j = 0;
	int pos = 0;
	while (a[pos] != '\0') {
		if (a[pos] != '\n') {
			screen[i][j] = a[pos];
			j++;
		}
		else {
			j = 0;
			i++;
		}
		pos++;
	}
}
//-----------------------------------------------------------------------------------state machines
//sends and receives usart
enum USART_sm1 { sm1_on, sm1_send, sm1_recieve } usartstate;
//calculates which alarm bit to set and send
enum ALARM_sm2 { sm2_off, sm2_on } alarmstate;
//prints screen to screen
enum SCREEN_sm3 { sm3_on, sm3_wait } screenstate;
//changes screen variable
enum PRINT_sm4 { sm4_on } printstate;
//Reads analog parts
enum A2D_sm5 { sm5_on, sm5_temp, sm5_light } a2dstate;		 

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
	receive
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

void Screen_Tick() {
	//transition
	switch (screenstate) {
		case sm3_on:
			screenstate = sm3_wait;
			break;
		case sm3_wait:
			if (comparescreen()) {
				screenstate = sm3_on;
			}
			else {
				screenstate = sm3_wait;
			}
			break;
		default:
			break;
	}
	//action
	//screen 6x14
	switch (screenstate) {
		char input;
		case sm3_on:
			lcd_clear();
			for (int i = 0; i < 6; i++) {
				for (int j = 0; j < 14; j++) {
					lcd_chr(screen[i][j]);
				}
			}
			break;
		case sm3_wait:
			break;
		default:
			break;
		
	}
}

void Print_Tick() {
	//transition
	switch (printstate) {
		case sm4_on:
			printstate = sm4_on;
			break;
		default:
			break;
	}
	//action
	switch (printstate) {
		char input;
		case sm4_on:
			input = GetKeypadKey();
			if (input == 'A') {
				//text
				screenarray("Temp :  C\nLight :");
				//temp
				short tempout = tempC - 15;
				char ttens = tempout / 10;
				char tones = tempout - (ttens * 10);
				screen[0][6] = ttens + '0';
				screen[0][7] = tones + '0';
				//light
				char ltens = light / 10;
				char lones = light - (ltens * 10);
				screen[1][7] = ltens + '0';
				screen[1][8] = lones + '0';
			}
			break;
		default:
			break;
			
	}
}
//reads analog parts
void A2D_Tick() {
	//transition
	switch (a2dstate) {
		case sm5_on:
			a2dstate = sm5_temp;
			break;
		case sm5_temp:
			a2dstate = sm5_light;
			break;
		case sm5_light:
			a2dstate = sm5_temp;
			break;
		default:
			break;
	}
	//action
	switch (a2dstate) {
		case sm5_on:
			break;
		case sm5_temp:
			Set_A2D_Pin(0x00);
			float voltage = ADC * 5.0;
			voltage /= 1024.0;
			float tempc = (voltage - 0.5) * 100;
			//truncates the dec
			tempC = (short)round(tempc);
			/* testing
			int tempi = (int)tempc;
			int tens = tempi / 10;
			int ones = tempi - (tens * 10);
			if (tens >= 2)
				tens = tens - 2;
			else
				tens = 0;
			lcd_chr(tens + '0');
			lcd_chr(ones + '0');
			*/
			break;
		case sm5_light:
			Set_A2D_Pin(0x02);
			
			/* testing
			lcd_goto_xy(0,0);
			lcd_chr('0' + ((ADC>>9)&0x001));
			lcd_chr('0' + ((ADC>>8)&0x001));
			lcd_chr('0' + ((ADC>>7)&0x001));
			lcd_chr('0' + ((ADC>>6)&0x001));
			lcd_chr('0' + ((ADC>>5)&0x001));
			lcd_chr('0' + ((ADC>>4)&0x001));
			lcd_chr('0' + ((ADC>>3)&0x001));
			lcd_chr('0' + ((ADC>>2)&0x001));
			lcd_chr('0' + ((ADC>>1)&0x001));
			lcd_chr('0' + ((ADC>>0)&0x001));
			*/
			//turn alarm off if lights > 57
			light = ADC;
			break;
		default:
			break;	
	}
}

//-------------------------------------------------------------------------------state machine inits
//usart
void SM1_Init() {
	usartstate = sm1_on;
}
//alarm
void SM2_INIT() {
	alarmstate = sm2_off;
}
//screen
void SM3_INIT() {
	screenstate = sm3_on;
}
//print
void SM4_INIT() {
	printstate = sm4_on;
}
//a2d
void SM5_INIT() {
	a2dstate = sm5_on;
}
//usart
void SM1Task() {
	SM1_Init();
	for(;;) {
		USART_Tick();
		vTaskDelay(100);
	}
}
//alarm
void SM2Task() {
	SM2_INIT();
	for(;;) {
		ALARM_Tick();
		vTaskDelay(100);
	}
}
//screen
void SM3Task() {
	SM3_INIT();
	for(;;) {
		Screen_Tick();
		vTaskDelay(50);
	}
}
//print
void SM4Task() {
	SM4_INIT();
	for(;;) {
		Print_Tick();
		vTaskDelay(25);
	}
}
//a2d
void SM5Task() {
	SM5_INIT();
	for (;;) {
		A2D_Tick();
		vTaskDelay(1000);
	}
}

void StartSecPulse1(unsigned portBASE_TYPE Priority) {
	xTaskCreate(SM1Task, (signed portCHAR *)"SM1Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}
void StartSecPulse2(unsigned portBASE_TYPE Priority) {
	xTaskCreate(SM2Task, (signed portCHAR *)"SM2Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}
void StartSecPulse3(unsigned portBASE_TYPE Priority) {
	xTaskCreate(SM3Task, (signed portCHAR *)"SM3Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}
void StartSecPulse4(unsigned portBASE_TYPE Priority) {
	xTaskCreate(SM4Task, (signed portCHAR *)"SM4Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}
void StartSecPulse5(unsigned portBASE_TYPE Priority) {
	xTaskCreate(SM5Task, (signed portCHAR *)"SM5Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}
int main(void) {
	// initialize ports
	DDRA = 0x00; PORTA = 0xFF;
	DDRB = 0xFF; PORTB = 0x00;
	DDRC = 0xF0; PORTC = 0x0F;
	//inits
	//actuator
	initUSART(0);
	//sensors
	initUSART(1);
	//lcd
	lcd_init(&PORTB, PB0, &PORTB, PB1, &PORTB, PB2, &PORTB, PB3, &PORTB, PB4);
	//adc
	A2D_init();
	//Start Tasks
	StartSecPulse1(1);
	StartSecPulse2(1);
	StartSecPulse3(1);
	StartSecPulse4(1);
	StartSecPulse5(1);
	//RunSchedular
	vTaskStartScheduler();
	
	return 0;
}
