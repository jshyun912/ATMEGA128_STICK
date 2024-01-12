#include <avr/io.h>
#include <stdbool.h>
#define F_CPU 16000000UL
#define __DELAY_BACKWARD_COMPATIBLE__
#include <util/delay.h>
#include <avr/interrupt.h>

#define TRIG 6
#define ECHO 7
#define ss 340UL
#define shocked 350

typedef volatile unsigned char vuc;
typedef volatile unsigned long vul;
typedef unsigned int uint;
typedef unsigned char ui8;
typedef struct  
{
	uint x, y, z;
}coord;

vuc read = 0b100;
vul set_distance = 300;
uint dist_val, prev = 128;
bool use_voice, use_LED = 1;

const uint sounds[3] = {238, 318, 500}; // DO SOL DO
const uint div_dist[3] = {150, 300, 500};
const ui8 ADXL_loop[3] = {0x41, 0x42, 0x43};

ISR(USART0_RX_vect)
{
	// read data from APP
	read = UDR0;
}

void bt_set()
{
	// 100 : LED ON 000 : LED OFF
	
	if (read >= 0b100)
	{
		use_LED = 1;
		PORTB = 0x01;
	}

	else
	{
		use_LED = 0;
		PORTB = 0x00;
		DDRB = 0x00;
	}

	// 010 : VOICE ON 000 : VOICE OFF
	
	if (read & 0b010) use_voice = 1;
	
	else use_voice = 0;
	
	// 001 : distance 50cm 000 : distance 30cm
	
	if (read & 0b001) set_distance = 300;
	
	else set_distance = 500;
}

void bt_send(char val)
{
	while(!(UCSR0A & 0x20));
	UDR0 = val;
}

void buzzer(int scale)
{	
	for (uint i = 0; i < sounds[scale]; i++)
	{
		PORTA = 0x03;
		_delay_us(sounds[scale]);
		PORTA = 0x00;
		_delay_us(sounds[scale]);
	}
}

uint shock_measure(uint a)
{
	unsigned char adc_low, adc_high;
	uint val;
	
	ADMUX = ADXL_loop[a];
	ADCSRA |= ADXL_loop[a];
	while ((ADCSRA & 0x10) != 0x10);
	
	adc_low = ADCL;
	adc_high = ADCH;
	val = (adc_high << 8) | adc_low;
	
	return val;
}

uint light_measure()
{
	unsigned char adc_low, adc_high;
	uint val;
	
	ADMUX = 0x40;
	ADCSRA |= 0x40;
	while ((ADCSRA & 0x10) != 0x10);
	
	adc_low = ADCL;
	adc_high = ADCH;
	val = (adc_high << 8) | adc_low;
	
	return val;
}

void LED(int dist)
{
	uint val = light_measure();
	double temp = ((long)(1000 - val) * (long)255) / 1000;
	val = (uint)temp;
	
	// PWM MIN : 10%, MAX : 90%

	if (val < 25U)
	val = 25;
	
	else if (val > 230U)
	val = 230;

	// 30CM PB4 LED ON 50CM PB7 LED ON
	
	if (dist == 500)
	{
		DDRB = 0b00010001;
		OCR0 = val;
	}
	
	else
	{
		DDRB = 0b10000001;
		OCR2 = val;
	}
}

uint dist_measure()
{
	TCCR1B = 0x03;
	PORTC &= ~(1 << TRIG);
	_delay_us(10);
	PORTC |= (1 << TRIG);
	_delay_us(10);
	PORTC &= ~(1 << TRIG);
	
	while (!(PINC & (1 << ECHO)));
	TCNT1 = 0x0000;
	
	while (PINC & (1 << ECHO));
	TCCR1B = 0x00;
	
	return (uint)(ss * (TCNT1 * 4 / 2) / 1000);
}

int main()
{
	DDRA = 0x03; // buzzer pa0, pa1
	DDRB = 0x01; PORTB = 0x01;// led pb1, pb4, pb7, pb4
	DDRC = ((DDRC | (1 << TRIG)) & ~(1 << ECHO)); // sensor pc6, pc7
	
	// PIN SET
	
	UCSR0A = 0x00; 
	UCSR0B = 0x98;
	UCSR0C = 0x06;
	UBRR0H = 0;
	UBRR0L = 103;
	SREG = 0x80;
	
	// BLUTOOTH
	
	ADCSRA = 0x87;
	
	// ADC INIT
	
	TCCR2 = 0x6b;
	TCCR0 = 0x6b;
	
	// PWM FAST
	
	while (1)
	{
		coord ADXL;
		
		ADXL.x = shock_measure(0);
		ADXL.y = shock_measure(1);
		ADXL.z = shock_measure(2);
		
		if (ADXL.x < shocked)
		{
			DDRB = 0b10010001;
			PORTB = 0x01;
			OCR0 = 230;
			OCR2 = 230;
			
			bool key = 1;
			
			for (int i = 0; i < 10; i++)
			{
				_delay_ms(500);
				
				ADXL.x = shock_measure(0);
				ADXL.y = shock_measure(1);
				ADXL.z = shock_measure(2);
				
				if (ADXL.x > shocked)
				{
					key = 0;
					break;
				}
			}
			
			if (key) 
			{
				bt_send(4);
							
				while (ADXL.x < shocked)
				{
					ADXL.x = shock_measure(0);
					ADXL.y = shock_measure(1);
					ADXL.z = shock_measure(2);
								
					buzzer(0);
				}
			}

		}	
		
		bt_set();
		dist_val = dist_measure();
		bool key = 1;
		
		for (int i = 0; i < 3; i++)
			if (dist_val < div_dist[i])
			{
				if (use_voice && div_dist[i] <= set_distance) 
					if((div_dist[i] / 500) + 1 != prev) 
					{
						bt_send((div_dist[i] / 500) + 1);
						prev = (div_dist[i] / 500) + 1;
					}
				
				buzzer(i);
				if (use_LED) LED(div_dist[i]);
				key = 0;
				break;
			}
		
		if (key) DDRB = 0x01;
	}
}