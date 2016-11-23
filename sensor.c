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

//----------------------------------------------------------------------------------global variables
/*
0x1234 5678
//send
usart 0: 1 = beam break, 1 = break, 0 = not break
usart 0: 2 = pid motion, 1 = motion, 0 = no motion

//receive
//alarm
1
2
3
4

usart 0: 5 = motor1, 1 = on, 0 = off
usart 0: 6 = motor1, 1 = spin right, 0 = spin left
usart 0: 7 = motor2, 1 = on, 0 = off
usart 0: 8 = motor2, 1 = spin right, 0 = spin left
*/
char recvsig = 0;
char sendsig = 0;

char temperature = 0;
char speaker = 0;
char tone = 0;
//----------------------------------------------------------------------------------global functions
//----------------------------------------------------------------------------------------state machines
//sets sensor and sets leds
enum Sensor_sm1 { sm1_on } sensorstate;
//sends and recieves usart
enum Usart_sm2 { sm2_on, sm2_send, sm2_recieve } usartstate;
//speaker code turns on speaker and given tone
enum Speaker_sm3 { sm3_on, sm3_off } speakerstate;
//manages the speaker sets tone and speaker
enum SpeakerC_sm4 { sm4_on } speakercstate;	
//manages the motors 
enum Moters_sm5 { sm5_on } motorstate;

//sends sensors ir and motion and sets led 
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
			//output set bit to 1 if beam broken 0 if not
			//sendsig set 0x1234 5678
			//set 1 to 1 if beam broken 0 if not
			if (!A0) {
				output = output | 0x01;
				sendsig = sendsig | 0x80;
			} else {
				output = output & 0xFE;
				sendsig = sendsig & 0x7F;
			}
			
			//motion
			//output set bit to 1 if  motion detected 0 if not
			//sendsig set 0x1234 5678
			//set 2 to 1 if motion 0 if not
			A1 = (PINA & 0x02) >> 1;
			if (A1) {
				output = output | 0x02;
				sendsig = sendsig | 0x40;
			} else {
				output = output & 0xFD;
				sendsig = sendsig & 0xBF;
			}
			
			//fast slow and fire for leds
			char fast = (recvsig >> 5) & 0x01;
			char slow = (recvsig >> 4) & 0x01;
			char fire = (recvsig >> 3) & 0x01;
			//fast door
			if (fast) {
				output = output | 0x04;
			} else {
				output = output & 0xFB;
			}
			//slow door
			if (slow) {
				output = output | 0x08;
			} else {
				output = output & 0xF7;
			}
			//fire 
			if (fire) {
				output = output | 0x10;
			} else {
				output = output & 0xEF;
			}
			PORTC = output;
			break; 
		default:
			break;
			
	}
}

//sends and receives usart
void USART_Tick() {
	//transition
	switch(usartstate) {
		case sm2_on:
			usartstate = sm2_send;
			break;
		case sm2_send:
			usartstate = sm2_recieve;
			break;
		case sm2_recieve:
			usartstate = sm2_send;
			break;
		default:
			break;
	}
	//action
	switch(usartstate) {
		case sm2_on:
			break;
		case sm2_send:
			if (USART_IsSendReady(0)) {
				USART_Send(sendsig,0);
			}
			break;
		case sm2_recieve:
			if (USART_HasReceived(0)) {
				recvsig = USART_Receive(0);
			}
			break;
		default:
			break;
	}
}

//turns the speaker on and off and plays a tone depending on tone variable
void Speaker_Tick() {
	//transition
	static unsigned short timer = 0;
	switch(speakerstate) {
		case sm3_on:
			speakerstate = sm3_off;
			break;
		case sm3_off:
			if (speaker) {
				timer++;
				if (tone == 1) {
					timer = 0;
					speakerstate = sm3_on;
				} else if (tone == 2) {
					if (timer > 1) {
						timer = 0;
						speakerstate = sm3_on;
					}
				} else if (tone == 3) {
					if (timer > 2) {
						timer = 0;
						speakerstate = sm3_on;
					}
				} else if (tone == 4) {
					if (timer > 4) {
						timer = 0;
						speakerstate = sm3_on;
					}
				} else if (tone == 5) {
					if (timer > 8) {
						timer = 0;
						speakerstate = sm3_on;
					}
				}
			} else {
				speakerstate = sm3_off;
			}
			break;
		default:
			break;
	}
	switch(speakerstate) {
		case sm3_on:
			PORTB = 0xFF;
			break;
		case sm3_off:
			PORTB = 0x00;
			break;
		default:
			break;
	}
}

//controls the speaker
void SpeakerC_Tick() {
	char window = (recvsig >> 7) & 0x01;
	char room = (recvsig >> 6) & 0x01;
	char fast = (recvsig >> 5) & 0x01;
	char slow = (recvsig >> 4) & 0x01;
	char fire = (recvsig >> 3) & 0x01;
	switch(speakercstate) {
		case sm4_on:
			speakercstate = sm4_on;
			break;
		default:
			break;
	}
	switch (speakercstate) {
		case sm4_on:
			if (window) {
				if (room || fast || slow || fire) {
					tone++;
					if (tone > 5) {
						tone = 0;
					}
				} else {
					speaker = 1;
					tone = 1;
				}
			}
			if (room) {
				if (window || fast || slow || fire) {
					tone++;
					if (tone > 5) {
						tone = 0;
					}
				} else {
					speaker = 1;
					tone = 2;
				}
			}
			if (fast) {
				if (window || room || slow || fire) {
					tone++;
					if (tone > 5) {
						tone = 0;
					}
				} else {
					speaker = 1;
					tone = 3;
				}
			}
			if (slow) {
				if (window || room || fast || fire) {
					tone++;
					if (tone > 5) {
						tone = 0;
					}
				} else {
					speaker = 1;
					tone = 4;
				}
			}
			if (fire) {
				if (window || room || fast || slow) {
					tone++;
					if (tone > 5) {
						tone = 0;
					}
				} else {
					speaker = 1;
					tone = 5;
				}
			}
			if (!window && !room && !fast && !slow && !fire) {
				speaker = 0;
			}
			break;
		default:
			break;
	}
}

//controls the motor
void Motor_Tick() {
	
}
//-------------------------------------------------------------------------------state machine inits
//sensors and led
void SM1_INIT() {
	sensorstate = sm1_on;
}
//usart
void SM2_INIT() {
	usartstate = sm2_on;
}
//speaker
void SM3_INIT() {
	speakerstate = sm3_on;
}
//speaker control
void SM4_INIT() {
	speakercstate = sm4_on;
}
//motors
void SM5_INIT() {
	motorstate = sm5_on;
}

//sensor and leds
void SM1Task() {
	SM1_INIT();
	for(;;) {
		Sensor_Tick();
		vTaskDelay(50);
	}
}
//usart
void SM2Task() {
	SM2_INIT();
	for(;;) {
		USART_Tick();
		vTaskDelay(100);
	}
}
//speaker
void SM3Task() {
	SM3_INIT();
	for(;;) {
		Speaker_Tick();
		vTaskDelay(1);
	}
}
//speaker control
void SM4Task() {
	SM4_INIT();
	for(;;) {
		SpeakerC_Tick();
		vTaskDelay(100);
	}
}
//motor
void SM5Task() {
	SM5_INIT();
	for(;;) {
		Motor_Tick();
		vTaskDelay(500);
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
	//everything input
	DDRA = 0x00; PORTA = 0xFF;
	//output
	DDRB = 0xFF; PORTB = 0x00;
	DDRC = 0xFF; PORTC = 0x00;
	//motor
	DDRD = 0x0F; PORTD = 0xF0;
	
	//inits
	//usart
	initUSART(0);
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
