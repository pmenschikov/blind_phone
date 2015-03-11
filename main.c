#define F_CPU 128000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

// rows PINB0-3
// cols PIND2-5


#define ROW_1	PINB0
#define ROW_2	PINB1
#define ROW_3	PINB2
#define ROW_4	PINB3

#define COL_1	PIND3
#define COL_2	PIND4
#define COL_3	PIND5
#define COL_4	PIND6

// пауза между нажатиями клавиш. в мс
#define INTERDIGIT_INTERVAL 100
// длительность нажатия клавиши, в мс
#define KEY_PRESSED_TIME	40
// Максимальное количество цифр в номере
#define MAX_DIGITS	16

#define OCTOTORP	('C'-0x30)

// кнопка набора сохранненого номера
#define CALL_PIN	PIND2
// контакт трубки
#define HOOKUP_PIN	PINB4

// значения пинов матрицы
#define COLS_VAL()	((PIND & 0x78) >> 3)
#define ROWS_VAL() (PINB & 0x0F)

#define ROW_MASK	(_BV(ROW_1) | _BV(ROW_2) | \
	        _BV(ROW_3) | _BV(ROW_4))
#define COL_MASK	(_BV(COL_1) | _BV(COL_2) | \
			_BV(COL_3) | _BV(COL_4))

#define ROW_PINS_OUT()	\
	DDRB |= ROW_MASK
#define ROW_PINS_IN()	\
	DDRB &= ~ROW_MASK

#define COL_PINS_OUT() \
	DDRD |= COL_MASK
#define COL_PINS_IN() \
	DDRD &= ~COL_MASK

#define CLR_R(row)	(DDRB |= _BV(row))
#define CLR_C(col)	(DDRD |= _BV(col))

#define HOOK_UP() (bit_is_clear(PINB, HOOKUP_PIN))
#define CALL_PRESSED() (!(PIND & _BV(CALL_PIN)))

uint8_t EEMEM phone[] = "89618888888C";
uint32_t timer;

static void delay(uint16_t delay)
{
	while(delay--)
	{
		_delay_ms(1);
	}
}

static void init(void)
{
	// выключаем аналоговый компаратор
	ACSR |= _BV(ACD);
	UBRRL = 12;
	UBRRH = 0;
//	DDRD = 3;
//	UCSRB = _BV(TXEN);
//	UCSRC = _BV(UCSZ1) | _BV(UCSZ0); 
	// pull-up для кнопки набона номера
	PORTD = _BV(CALL_PIN);

	// int0 on low level
	GIMSK |= _BV(PCIE);
	PCMSK = _BV(PCINT4);
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sei();
}

static uint8_t get_digit(void)
{
	
	uint8_t cols, rows;

	// cols normal - high
	cols = COLS_VAL();
	rows = ROWS_VAL();

	uint8_t i;

	if( cols != 0x0F)
	{
		i=10;
		while(COLS_VAL()!=0 && i--)
			delay(1);
		rows = ROWS_VAL();
		if( rows == 0 )
			return 255;
	}
	else
		return 255;

	if( cols == 0 )
	{
		return 255;
	}


	uint8_t digit = 255;
	switch(cols)
	{
		case 0x0E:
			switch(rows)
			{
				case 0x0E:
					digit = 1;
					break;
				case 0x0D:
					digit = 4;
					break;
				case 0x0B:
					digit = 7;
					break;
				case 0x07:
					digit = '*'-0x30;
					break;
				default:
					digit = 255;
			}
			break;
		case 0x0D:
			switch(rows)
			{
				case 0x0E:
					digit = 2;
					break;
				case 0x0D:
					digit = 5;
					break;
				case 0x0B:
					digit = 8;
					break;
				case 0x07:
					digit = 0;
					break;
				default:
					digit = 255;
			}
			break;
		case 0x0B:
			switch(rows)
			{
				case 0x0E:
					digit = 3;
					break;
				case 0x0D:
					digit = 6;
					break;
				case 0x0B:
					digit = 9;
					break;
				case 0x07:
					digit = OCTOTORP;
					break;
				default:
					digit = 255;
			}
			break;
		default:
			return 255;

	}
	while(COLS_VAL()!= 0x0f);
	return digit;
}

static uint8_t call_pressed(void)
{
	if( CALL_PRESSED() )
	{
		delay(15);
		if( CALL_PRESSED() )
		{
			return 1;
		}
	}
	return 0;
}

static void send_digit(uint8_t digit)
{
	switch(digit)
	{
		case 0:
			CLR_R(ROW_4);
			CLR_C(COL_2);
			break;
		case 1:
			CLR_R(ROW_1);
			CLR_C(COL_1);
			break;
		case 2:
			CLR_R(ROW_1);
			CLR_C(COL_2);
			break;
		case 3:
			CLR_R(ROW_1);
			CLR_C(COL_3);
			break;
		case 4:
			CLR_R(ROW_2);
			CLR_C(COL_1);
			break;
		case 5:
			CLR_R(ROW_2);
			CLR_C(COL_2);
			break;
		case 6:
			CLR_R(ROW_2);
			CLR_C(COL_3);
			break;
		case 7:
			CLR_R(ROW_3);
			CLR_C(COL_1);
			break;
		case 8:
			CLR_R(ROW_3);
			CLR_C(COL_2);
			break;
		case 9:
			CLR_R(ROW_3);
			CLR_C(COL_3);
			break;
		case OCTOTORP:
			CLR_R(ROW_4);
			CLR_C(COL_3);
			break;
		default:
			;
	}
	// длительность нажатия клавиши
	delay(KEY_PRESSED_TIME);
	ROW_PINS_IN();
	COL_PINS_IN();

}

static void send_number(void)
{
	const uint8_t *p = &phone[0];
	char digit = 1;

	PORTB &= ~ROW_MASK;
	PORTD &= ~COL_MASK;

	while(digit)
	{
		digit = eeprom_read_byte(p++);
		if( digit == 0x00 )
		{
			break;
		}
		send_digit(digit-0x30);

		delay(INTERDIGIT_INTERVAL);
		if( digit == 'C' )
			break;
	}
}

static void save_phone(void)
{
	uint8_t digits[MAX_DIGITS];
	uint8_t *p = &digits[0];
	uint8_t digit_count = 0;
	uint8_t digit;
	timer = 0;
	while(++timer <100000UL)
	{
		digit = get_digit();
		if( digit == 255 )
		{

			// error
			// не знам что с этим делать %(
			// просто не будем сохранять
		}
		else
		{
			*p++ = digit + 0x30;
			digit_count++;
			if( digit_count >= MAX_DIGITS )
			{
				// error
				// не знам что с этим делать %(
				// просто не будем сохранять
				break;
			}
			if( digit == OCTOTORP )
			{
				// по октоторпу сохраняем номер
				digits[digit_count-1] = 0;
				eeprom_write_block( digits, &phone[0], digit_count);
				// и уходим
				break;
			}
		}
	}
}

ISR(PCINT_vect)
{
}

static void sleep(void)
{
sleep:
	sleep_enable();
	sleep_cpu();
	sleep_disable();
	if( !HOOK_UP() )
	{
		goto sleep;
	}
}

int main(void)
{
	// инициализируем периферию
	init();

	// отправляемся спать
	// пробуждение наcтроено по изменению
	// уровня на ноге подключеной к HKS
	while(1)
	{
		sleep();
		// если кнопка вызова нажата в момент
		// снятия трубки 
		if( call_pressed() )
		{
			//то запоминаем набранный номер
			save_phone();
			// и идем спать
		}
		else
		{
			timer = 0;
			while(HOOK_UP())
			{
				if( call_pressed() )
				{
					send_number();
					// go to sleep!
					break;
				}
				if( ++timer > 72000UL )
				{
					break;
				}
			}
		}
	}
	return 0;
}
