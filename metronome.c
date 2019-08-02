#ifndef F_CPU
#define F_CPU 16000000UL // 16.0000 MHz clock speed
#endif

//Include AVR Libraries
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
//Include LCD Libraries
#include "lcdpin.h"
#include "lcd.h"

#define MAX_SPEED				250		//Max metronome speed in BPM
#define MIN_SPEED				20		//Min metronome speed in BPM

#define OUTPUT_PORT				PORTA	//The Buzzer is connected to pin PA4/ADC4, LED is connected to PA3
#define INPUT					PINA	//Buttons are on PA0 - PA2
#define BUZZER_PIN				4		//PA4
#define LED_PIN					3		//PA3

#define DEBOUNCE_COUNT_MAX		1500		//Number used for debouncing the buttons

//Enumerated Values for use in programming
enum {
	BUTTON_UP,
	BUTTON_DOWN,
	BUTTON_TIMESIG,
	NUM_BUTTONS
} buttons;

enum {
	TWO_TWO,
	TWO_FOUR,
	FOUR_FOUR,
	THREE_EIGHT,
	FIVE_EIGHT,
	SIX_EIGHT,
	NUM_TIMESIG
} timeSigs;
	
//State Variables
uint8_t speed = 50; //Metronome speed, initialized to 50 BPM
uint16_t button_counter[NUM_BUTTONS]; //Array containing the amount of time each button is held down for

//Tone Generations Variables
uint16_t toneFreq = 2000; //Frequency of the tone sent to the buzzer, in Hz
uint8_t buzzPeriod = 40; //The length of the tone, toneFreq * 20ms
uint16_t iCounterMax; //Used for sounding the tone, see the timer1 ISR
uint16_t iCounter = 0; //interrupt counter
uint8_t buzzerflag = 0;

//Time Signature Count Tone Change Variables
uint8_t current_TimeSig = FOUR_FOUR;
uint8_t beatCounter = 0; //Counts the number of beats
uint8_t divisor = 4; //Determined based on time signature

//LCD display Variables
char tempo[4]; //convert values metronome speed to string variable
char timeSig[4] = "4/4"; //timeSigs in string variable

//Function Prototypes
void setup(); //Setup I/O and timers
void outLCD(); // LCD output tempo values to display
void setTimeSig(char a,char b); //set timeSig by new values

int main(void)
{
	setup(); //call setup function to get everything ready
	outLCD(); //call outLCD function for show metronome speed and time signature
	while (1) {
		//The main program loop will handle input and debouncing of the buttons
		for (int i = 0; i < NUM_BUTTONS; i++) {
			if ((INPUT & (1<<i)) == (1<<i))
				button_counter[i]++;
			else
				button_counter[i] = 0;
		}
		
		if (button_counter[BUTTON_UP] >= DEBOUNCE_COUNT_MAX) {
			//metronome speed, but not too fast - that's what the delay is for.
			while ((INPUT & (1<<BUTTON_UP)) == 0x01) {
				TIMSK = 0;
				if (speed>=MAX_SPEED)
					speed = MAX_SPEED;
				else
					speed++;
				outLCD(); //call outLCD function for show new metronome speed
				_delay_ms(75);
			}
			TCNT1 = 0; //Restart the timer at 0
			TIMSK = (1<<OCIE1A); //Re-enable the interrupt on timer1
		}
		else if (button_counter[BUTTON_DOWN] >= DEBOUNCE_COUNT_MAX) {
			//Same as above except now we decrement speed
			while ((INPUT & (1<<BUTTON_DOWN)) == 0x02) {
				TIMSK = 0;
				if (speed <= MIN_SPEED)
					speed = MIN_SPEED;
				else
					speed--;
				outLCD(); //call outLCD function for show new metronome speed
				_delay_ms(75);
			}
			TCNT1 = 0; //Restart the timer at 0
			TIMSK = (1<<OCIE1A); //Re-enable the interrupt on timer1
		}
		else if (button_counter[BUTTON_TIMESIG] >= DEBOUNCE_COUNT_MAX) {
			TIMSK = 0; //disable the timer1 interrupt
			_delay_ms(150);
			if ((current_TimeSig + 1) >= NUM_TIMESIG)
				current_TimeSig = TWO_TWO;
			else
				current_TimeSig++;
			
			switch(current_TimeSig)
			{
				case FOUR_FOUR :	divisor = 4;
									setTimeSig('4','4');
					break;
					
				case TWO_TWO :		divisor = 2;
									setTimeSig('2','2');
					break;
					
				case TWO_FOUR :		divisor = 2;
									setTimeSig('2','4');
					break;
					
				case THREE_EIGHT :	divisor = 3;
									setTimeSig('3','8');
					break;
					
				case FIVE_EIGHT :	divisor = 5;
									setTimeSig('5','8');
					break;
					
				case SIX_EIGHT :	divisor = 6;
									setTimeSig('6','8');
					break;
					
				default :			divisor = 4;
									setTimeSig('4','4');
			}
			outLCD(); //call outLCD function for show new time signature
			TIMSK = (1<<OCIE1A); //Re-enable the interrupt on timer1
		}
	}
	
	return 0;
}

void setup()
{
	//Handle Pin Direction Settings
	DDRC = 0xFF; //I/O pins on port C, LCD, set all pins as output
	DDRA = 0x18; //I/O pins on port A, LED and Buzzer, set pins 0 and 1 as output
	PORTA = 0x07; //Set internal pull-ups on pins PA0, PA1, PA2
	
	//Initialize iCounterMax for tone generation
	iCounterMax = toneFreq*((float)60/(float)speed);
	
	//Initialize value all button counters to 0
	for (int i = 0; i < NUM_BUTTONS; i++)
		button_counter[i] = 0;
	
	//Set-up Timers and Interrupts
	//Timer1 for Generation a Tone
	TCCR1A = (1<<WGM10) | (1<<WGM11); //Fast PWM mode, start with output disabled
	TCCR1B = (1<<WGM12) | (1<<WGM13) | (1<<CS11); //clock divided by 8
	OCR1A = (F_CPU/8)/toneFreq - 1; //This calculation will generate a PWM with the frequency given by toneFreq
	TIMSK = (1<<OCIE1A); //Enable match interrupt

	//Initialize setting to LCD display
	Lcd4_Init(); //Initialize LCD for start manifest tempo values
	
	sei(); //Enable global interrupt
}

void outLCD() {
	itoa(speed,tempo,10);
	Lcd4_Clear(); //Clear monitor LCD display to Idle
	Lcd4_Set_Cursor(1,0); //set position LCD to row 1
	//set LCD display to show string in row 1
	Lcd4_Write_String("METRONOME ");
	Lcd4_Write_String(timeSig);
	Lcd4_Set_Cursor(2,0); //set position LCD to row 2
	//set LCD display to show string in row 2
 	Lcd4_Write_String("Tempo = ");
	Lcd4_Write_String(tempo);
	Lcd4_Write_String(" bpm");
}

void setTimeSig(char a,char b) {
	timeSig[0] = a;
	timeSig[1] = '/';
	timeSig[2] = b;
	timeSig[3] = '\0';
}

ISR(TIMER1_COMPA_vect) {
	iCounter++; //increment interrupt counter
	TCCR1A &= ~(1<<COM1A0); //disable PWM output on pin PB1
	
	if ((iCounter == iCounterMax) || buzzerflag == 1) {
		buzzerflag = 1; //set buzzer flag to on
		OUTPUT_PORT ^= (1<<BUZZER_PIN) | (1<<LED_PIN); //Turn on/toggle LED and Buzzer
		TCCR1A |= (1<<COM1A0); //enable PWM output on pin PB1
		
		//if the buzzer has been on for the specified length of time
		if (iCounter == (iCounterMax + buzzPeriod)) {
			buzzerflag = 0; //set buzzer flag to off
			iCounter = 0; //reset increment counter
			OUTPUT_PORT &= ~((1<<BUZZER_PIN) | (1<<LED_PIN)); //Turn off/toggle LED and Buzzer
			
			//Check if reached the number of beats required to change frequencies
			if (beatCounter % divisor == 0) {
				toneFreq = 1000;
				buzzPeriod = 20;
				beatCounter++;
			}
			else {
				toneFreq = 2000;
				buzzPeriod = 40;
				beatCounter++;
			}
			iCounterMax = toneFreq*((float)60/(float)speed); //recalculate interrupt counter max
			OCR1A = (F_CPU/8)/toneFreq; //modify compare value so timer1 fires at proper rate
		}
	}
}