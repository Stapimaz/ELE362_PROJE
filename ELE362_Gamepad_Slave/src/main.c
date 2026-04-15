#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#define BAUD 38400
#define MYUBRR (F_CPU / 16 / BAUD - 1)
#define MPU_ADDR_W 0xD0
#define MPU_ADDR_R 0xD1

// Global variables
volatile uint8_t send_frame = 0;
volatile uint8_t vibration_counter = 0;
uint8_t tx_frame[5] = {0xFF, 0, 0, 0, 0xFE};

// UART functions
void UART_Init(void) {
    UBRR0H = (MYUBRR >> 8);
    UBRR0L = MYUBRR;
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void UART_Transmit(uint8_t data) {
    while (!(UCSR0A & (1 << UDRE0))) {
    }
    UDR0 = data;
}

// Receive interrupt - listen for vibration command
ISR(USART_RX_vect) {
    uint8_t received = UDR0;

    if (received == 0xAA) {
        PORTD |= (1 << PD4);
        vibration_counter = 15;
    }
}

// I2C functions
void I2C_Init(void) {
    TWSR = 0x00;
    TWBR = 12;
    TWCR = (1 << TWEN);
}

uint8_t I2C_Wait(void) {
    uint16_t timeout = 60000;

    while (!(TWCR & (1 << TWINT))) {
        timeout--;
        if (timeout == 0) {
            return 0;
        }
    }

    return 1;
}

void I2C_Start(void) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    if (!I2C_Wait()) {
        return;
    }
}

void I2C_Stop(void) {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

void I2C_Write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (!I2C_Wait()) {
        return;
    }
}

uint8_t I2C_Read_Nack(void) {
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (!I2C_Wait()) {
        return 0;
    }
    return TWDR;
}

// Accelerometer functions
void MPU6050_Init(void) {
    I2C_Start();
    I2C_Write(MPU_ADDR_W);
    I2C_Write(0x6B);
    I2C_Write(0x00);
    I2C_Stop();
}

int16_t MPU6050_ReadAxis(uint8_t reg) {
    uint8_t high_byte;
    uint8_t low_byte;

    I2C_Start();
    I2C_Write(MPU_ADDR_W);
    I2C_Write(reg);
    I2C_Start();
    I2C_Write(MPU_ADDR_R);

    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
    if (!I2C_Wait()) {
        I2C_Stop();
        return 0;
    }
    high_byte = TWDR;

    low_byte = I2C_Read_Nack();
    I2C_Stop();

    return (int16_t)((high_byte << 8) | low_byte);
}

// Timer interrupt - 60Hz
ISR(TIMER1_COMPA_vect) {
    send_frame = 1;

    if (vibration_counter > 0) {
        vibration_counter--;
        if (vibration_counter == 0) {
            PORTD &= ~(1 << PD4);
        }
    }
}

int main(void) {
    UART_Init();
    I2C_Init();
    MPU6050_Init();

    // Button pins - input with pull-up
    DDRD &= ~((1 << PD2) | (1 << PD3));
    PORTD |= (1 << PD2) | (1 << PD3);

    // Vibration motor pin - output
    DDRD |= (1 << PD4);
    PORTD &= ~(1 << PD4);

    // Timer1 setup - CTC mode, 60Hz
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS12);
    OCR1A = 1041;
    TIMSK1 = (1 << OCIE1A);

    sei();

    while (1) {
        if (send_frame) {
            send_frame = 0;

            // Read accelerometer
            int16_t x_raw = MPU6050_ReadAxis(0x3B);
            int16_t y_raw = MPU6050_ReadAxis(0x3D);

            // Map to 0-253 range
            int16_t x_mapped = (x_raw / 128) + 127;
            int16_t y_mapped = (y_raw / 128) + 127;

            if (x_mapped < 0) {
                x_mapped = 0;
            }
            if (x_mapped > 253) {
                x_mapped = 253;
            }
            if (y_mapped < 0) {
                y_mapped = 0;
            }
            if (y_mapped > 253) {
                y_mapped = 253;
            }

            // Read buttons
            uint8_t btn_fire = !(PIND & (1 << PD2));
            uint8_t btn_power = !(PIND & (1 << PD3));

            // Build packet
            tx_frame[1] = x_mapped;
            tx_frame[2] = y_mapped;
            tx_frame[3] = btn_fire | (btn_power << 1);

            // Send packet
            for (uint8_t i = 0; i < 5; i++) {
                UART_Transmit(tx_frame[i]);
            }
        }
    }

    return 0;
}
