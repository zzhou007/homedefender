/*********************************************************************

                 xBoard(TM) v2.0 Sample Programs

               ------------------------------------


Description :  Demonstrate the use of Accelerometer to sense tilt angle.
            Shows the raw X,Y,Z readings on LCD Disp.

Accelerometer: MMA7260 - 3 axis accelerometer breakout board.

Author      : Avinash Gupta

Web         : www.eXtremeElectronics.co.in

               
            

            
            Copyright 2008-2010 eXtreme Electronics, India
                   
**********************************************************************/

#include <avr/io.h>
#include <util/delay.h>


#include "lcd.h"

/*

Function To initialize the on-chip ADC

*/
void InitADC()
{
   ADMUX=(1<<REFS0);                                     // For Aref=AVcc;
   ADCSRA=(1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);    //Rrescalar div factor =128

}

/*

Function to read required analog channel and returns the value

Argument:
   Channel number from 0-7

Return Vale:
   Result of conversion on selected channel

   ranges from 0-1023

*/
uint16_t ReadADC(uint8_t ch)
{
   //Select ADC Channel ch must be 0-7
   ch=ch&0b00000111;
   ADMUX&=0b11100000;
   ADMUX|=ch;

   //Start Single conversion

   ADCSRA|=(1<<ADSC);

   //Wait for conversion to complete
   while(!(ADCSRA & (1<<ADIF)));

   //Clear ADIF by writing one to it

   //Note you may be wondering why we have write one to clear it

   //This is standard way of clearing bits in io as said in datasheets.
   //The code writes '1' but it result in setting bit to '0' !!!

   ADCSRA|=(1<<ADIF);

   return(ADC);
}

/*

Simple Delay function

*/
void Wait(uint8_t t)
{
   uint8_t i;
   for(i=0;i<t;i++)
      _delay_loop_2(0);
}


void main()
{
   int16_t x,y,z; //X,Y,Z axis values from accelerometer.

   //Wait for LCD to Startup
   _delay_loop_2(0);

   //Initialize LCD, cusror style = NONE(No Cursor)
   LCDInit(LS_NONE);
   LCDClear();

   //Initialize ADC
   InitADC();

   //Put some intro text into LCD

      LCDWriteString("  Accelerometer ");
      LCDWriteStringXY(0,1,"      Test      ");
      Wait(150);

   LCDClear();
      LCDWriteString("     eXtreme ");
      LCDWriteStringXY(0,1,"   Electronics  ");
   Wait(150);

   LCDClear();

   while(1)
   {
         x=ReadADC(0);           // Read Analog value from channel-0

      y=ReadADC(1);           // Read Analog value from channel-1
      z=ReadADC(2);           // Read Analog value from channel-2

       //Make it signed value (zero point is at 338)
      x=x-338;
      y=y-338;
      z=z-338;

      //Print it!

      LCDWriteStringXY(0,0,"X=");
      LCDWriteInt(x,4);

      LCDWriteString(" Y=");
      LCDWriteInt(y,4);

      LCDWriteStringXY(0,1,"Z=");
      LCDWriteInt(z,4);

      Wait(20);


   }


}
