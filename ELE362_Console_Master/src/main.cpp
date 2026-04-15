#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#define BAUD 38400
#define MYUBRR (F_CPU / 16 / BAUD - 1)
#define TILT_FULL_SCALE 56
#define TILT_DEADZONE 1
#define SERVO_LIFE_BASE 300
#define SERVO_LIFE_STEP 60

// OLED display object (128x64, I2C address 0x3C)
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Communication variables
volatile uint8_t rx_state = 0;
volatile uint8_t temp_x, temp_y, temp_btn;
volatile uint8_t game_x = 127;
volatile uint8_t game_y = 127;
volatile uint8_t game_btn = 0;
volatile uint8_t frame_ready = 0;
uint8_t display_ready = 0;

// Game variables
uint8_t player_lives = 3;
uint8_t game_over = 0;
uint8_t buzzer_timer = 0;
uint8_t fire_cooldown = 0;
uint8_t spawn_timer = 0;
uint16_t score = 0;
uint16_t enemy_speed_percent = 100;
uint16_t enemy_speed_extra_accum = 0;

// Projectile arrays (max 3)
int16_t bullet_x[3];
int8_t bullet_y[3];
uint8_t bullet_active[3];

// Enemy arrays (max 3)
int16_t enemy_x[3];
int8_t enemy_y[3];
uint8_t enemy_active[3];

void Servo_Set_Lives(uint8_t lives) {
    uint16_t pulse = SERVO_LIFE_BASE + ((3 - lives) * SERVO_LIFE_STEP);

    if (pulse < 250) {
        pulse = 250;
    }
    if (pulse > 500) {
        pulse = 500;
    }

    OCR1A = pulse;
}

uint8_t OLED_Detect_Address(void) {
    Wire.beginTransmission(0x3C);
    if (Wire.endTransmission() == 0) {
        return 0x3C;
    }

    Wire.beginTransmission(0x3D);
    if (Wire.endTransmission() == 0) {
        return 0x3D;
    }

    return 0;
}

// UART functions (bare-metal)
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

// Receive interrupt - 5 byte packet
ISR(USART_RX_vect) {
    uint8_t data = UDR0;

    switch (rx_state) {
        case 0:
            if (data == 0xFF) {
                rx_state = 1;
            }
            break;
        case 1:
            temp_x = data;
            rx_state = 2;
            break;
        case 2:
            temp_y = data;
            rx_state = 3;
            break;
        case 3:
            temp_btn = data;
            rx_state = 4;
            break;
        case 4:
            if (data == 0xFE) {
                game_x = temp_x;
                game_y = temp_y;
                game_btn = temp_btn;
                frame_ready = 1;
            }
            rx_state = 0;
            break;
    }
}

// Servo setup (bare-metal Timer1)
void Servo_Init(void) {
    DDRB |= (1 << PB1);
    TCCR1A = (1 << COM1A1) | (1 << WGM11);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11) | (1 << CS10);
    ICR1 = 4999;
    OCR1A = 500;
}

// Buzzer setup (bare-metal Timer2)
void Buzzer_Init(void) {
    DDRB |= (1 << PB3);
    TCCR2A = (1 << COM2A0) | (1 << WGM21);
    TCCR2B = 0x00;
    OCR2A = 80;
    PORTB &= ~(1 << PB3);
}

void Buzzer_Play(uint8_t duration) {
    TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);
    buzzer_timer = duration;
}

void Buzzer_Stop(void) {
    TCCR2B = 0x00;
    PORTB &= ~(1 << PB3);
}

// ADC setup (bare-metal)
void ADC_Init(void) {
    ADMUX = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t ADC_Read(void) {
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC)) {
    }
    return ADC;
}

// Game initialization
void Game_Init(void) {
    // Clear all bullets
    for (uint8_t i = 0; i < 3; i++) {
        bullet_active[i] = 0;
    }

    // Spawn enemies at different positions
    enemy_x[0] = 110;
    enemy_y[0] = 15;
    enemy_active[0] = 1;

    enemy_x[1] = 130;
    enemy_y[1] = 35;
    enemy_active[1] = 1;

    enemy_x[2] = 150;
    enemy_y[2] = 50;
    enemy_active[2] = 1;
}

// Fire a bullet from ship position
void Fire_Bullet(uint8_t ship_x, uint8_t ship_y) {
    if (fire_cooldown > 0) {
        return;
    }

    for (uint8_t i = 0; i < 3; i++) {
        if (bullet_active[i] == 0) {
            bullet_x[i] = ship_x + 3;
            bullet_y[i] = ship_y + 1;
            bullet_active[i] = 1;
            fire_cooldown = 6;
            return;
        }
    }
}

// Update bullet positions
void Update_Bullets(void) {
    for (uint8_t i = 0; i < 3; i++) {
        if (bullet_active[i]) {
            bullet_x[i] += 4;

            if (bullet_x[i] >= 128) {
                bullet_active[i] = 0;
            }
        }
    }
}

// Draw all active bullets
void Draw_Bullets(void) {
    for (uint8_t i = 0; i < 3; i++) {
        if (bullet_active[i]) {
            display.drawPixel(bullet_x[i], bullet_y[i], SSD1306_WHITE);
            display.drawPixel(bullet_x[i] + 1, bullet_y[i], SSD1306_WHITE);
        }
    }
}

// Update enemy positions
void Update_Enemies(void) {
    uint8_t enemies_alive = 0;
    uint8_t enemy_step = 1;
    uint8_t jitter_amplitude = 0;

    if (enemy_speed_percent > 325) {
        uint16_t extra_speed = enemy_speed_percent - 325;
        jitter_amplitude = (extra_speed + 19) / 20;
        if (jitter_amplitude > 4) {
            jitter_amplitude = 4;
        }
    }

    enemy_speed_extra_accum += (enemy_speed_percent - 100);
    while (enemy_speed_extra_accum >= 100) {
        enemy_step++;
        enemy_speed_extra_accum -= 100;
    }

    for (uint8_t i = 0; i < 3; i++) {
        if (enemy_active[i]) {
            enemy_x[i] -= enemy_step;

            if (jitter_amplitude > 0) {
                int16_t x_phase = enemy_x[i];
                int16_t next_y = enemy_y[i];
                if (x_phase < 0) {
                    x_phase = -x_phase;
                }

                if ((x_phase % 20) < 10) {
                    next_y += jitter_amplitude;
                } else {
                    next_y -= jitter_amplitude;
                }

                if (next_y < 0) {
                    next_y = 0;
                }
                if (next_y > 58) {
                    next_y = 58;
                }

                enemy_y[i] = (int8_t)next_y;
            }

            if (enemy_x[i] < -6) {
                enemy_active[i] = 0;
            } else {
                enemies_alive++;
            }
        }
    }

    // Respawn all enemies when none are left
    if (enemies_alive == 0) {
        spawn_timer++;
        if (spawn_timer > 45) {
            enemy_speed_percent += 20;
            Game_Init();
            spawn_timer = 0;
        }
    }
}

// Draw all active enemies
void Draw_Enemies(void) {
    for (uint8_t i = 0; i < 3; i++) {
        if (enemy_active[i] == 0) {
            continue;
        }

        // Draw 5x6 enemy rectangle
        if (enemy_x[i] >= -5 && enemy_x[i] < 128) {
            display.fillRect(enemy_x[i], enemy_y[i], 5, 6, SSD1306_WHITE);
        }
    }
}

// Check bullet-enemy collision
uint8_t Check_Bullet_Hit(void) {
    for (uint8_t i = 0; i < 3; i++) {
        if (bullet_active[i] == 0) {
            continue;
        }

        for (uint8_t j = 0; j < 3; j++) {
            if (enemy_active[j] == 0) {
                continue;
            }

            // AABB collision
            if (bullet_x[i] >= enemy_x[j] &&
                bullet_x[i] < enemy_x[j] + 5 &&
                bullet_y[i] >= enemy_y[j] &&
                bullet_y[i] < enemy_y[j] + 6) {
                bullet_active[i] = 0;
                enemy_active[j] = 0;
                return 1;
            }
        }
    }

    return 0;
}

// Check ship-enemy collision
uint8_t Check_Ship_Hit(uint8_t ship_x, uint8_t ship_y) {
    for (uint8_t i = 0; i < 3; i++) {
        if (enemy_active[i] == 0) {
            continue;
        }

        // AABB collision - ship is 3x3, enemy is 5x6
        if (ship_x < enemy_x[i] + 5 &&
            ship_x + 3 > enemy_x[i] &&
            ship_y < enemy_y[i] + 6 &&
            ship_y + 3 > enemy_y[i]) {
            enemy_active[i] = 0;
            return 1;
        }
    }

    return 0;
}

void setup() {
    // Initialize OLED display
    Wire.begin();
    Wire.setClock(400000);
    delay(40);

    uint8_t oled_addr = OLED_Detect_Address();

    if (oled_addr == 0) {
        delay(40);
        oled_addr = OLED_Detect_Address();
    }

    if (oled_addr != 0 && display.begin(SSD1306_SWITCHCAPVCC, oled_addr)) {
        display_ready = 1;
    }

    if (display_ready) {
        display.clearDisplay();
        display.display();
    }

    // Initialize bare-metal peripherals
    UART_Init();
    Servo_Init();
    Servo_Set_Lives(player_lives);
    Buzzer_Init();
    Buzzer_Stop();
    ADC_Init();
    Game_Init();

    sei();
}

void loop() {
    // Ship position
    static uint8_t ship_x = 10;
    static uint8_t ship_y = 28;
    static uint8_t prev_buttons = 0;
    static uint8_t calib_x = 127;
    static uint8_t calib_y = 127;
    static uint32_t last_update_ms = 0;

    if ((uint32_t)(millis() - last_update_ms) < 16) {
        return;
    }
    last_update_ms = millis();

    if (frame_ready) {
        frame_ready = 0;
    }

    // Map accelerometer to screen coordinates
    int16_t delta_x = (int16_t)game_x - calib_x;
    int16_t delta_y = (int16_t)game_y - calib_y;

    if (delta_x > TILT_DEADZONE) {
        delta_x -= TILT_DEADZONE;
    } else if (delta_x < -TILT_DEADZONE) {
        delta_x += TILT_DEADZONE;
    } else {
        delta_x = 0;
    }

    if (delta_y > TILT_DEADZONE) {
        delta_y -= TILT_DEADZONE;
    } else if (delta_y < -TILT_DEADZONE) {
        delta_y += TILT_DEADZONE;
    } else {
        delta_y = 0;
    }

    int16_t target_x = 62 + (delta_y * 62 / TILT_FULL_SCALE);
    int16_t target_y = 30 + (delta_x * 30 / TILT_FULL_SCALE);

    // Clamp to boundaries (ship is 3x3)
    if (target_x < 0) {
        target_x = 0;
    }
    if (target_x > 124) {
        target_x = 124;
    }
    if (target_y < 0) {
        target_y = 0;
    }
    if (target_y > 60) {
        target_y = 60;
    }

    int16_t err_x = target_x - (int16_t)ship_x;
    int16_t err_y = target_y - (int16_t)ship_y;

    uint8_t ship_step_x = 1;
    uint8_t ship_step_y = 1;

    if (err_x > 24 || err_x < -24) {
        ship_step_x = 3;
    } else if (err_x > 8 || err_x < -8) {
        ship_step_x = 2;
    }

    if (err_y > 16 || err_y < -16) {
        ship_step_y = 3;
    } else if (err_y > 6 || err_y < -6) {
        ship_step_y = 2;
    }

    // Smooth movement
    if (err_x > 0) {
        if (err_x > ship_step_x) {
            ship_x += ship_step_x;
        } else {
            ship_x = target_x;
        }
    } else if (err_x < 0) {
        if (-err_x > ship_step_x) {
            ship_x -= ship_step_x;
        } else {
            ship_x = target_x;
        }
    }

    if (err_y > 0) {
        if (err_y > ship_step_y) {
            ship_y += ship_step_y;
        } else {
            ship_y = target_y;
        }
    } else if (err_y < 0) {
        if (-err_y > ship_step_y) {
            ship_y -= ship_step_y;
        } else {
            ship_y = target_y;
        }
    }

    uint8_t fire_pressed = (game_btn & 0x01) && !(prev_buttons & 0x01);
    uint8_t calib_pressed = (game_btn & 0x02) && !(prev_buttons & 0x02);

    // Check fire button (rising edge)
    if (fire_pressed && !game_over) {
        Fire_Bullet(ship_x, ship_y);
        Buzzer_Play(3);
    }

    // Check calibration button (rising edge)
    if (calib_pressed && !game_over) {
        calib_x = game_x;
        calib_y = game_y;
        Buzzer_Play(8);
    }
    prev_buttons = game_btn;

    if (game_over) {
        if (fire_pressed) {
            player_lives = 3;
            game_over = 0;
            ship_x = 10;
            ship_y = 28;
            fire_cooldown = 0;
            spawn_timer = 0;
            score = 0;
            enemy_speed_percent = 100;
            enemy_speed_extra_accum = 0;
            Servo_Set_Lives(player_lives);
            Game_Init();
            Buzzer_Play(6);
        }

        if (display_ready) {
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 0);
            display.print("SCORE: ");
            display.print(score);
            display.setTextSize(2);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(16, 14);
            display.print("GAME");
            display.setCursor(16, 34);
            display.print("OVER");
            display.setTextSize(1);
            display.setCursor(6, 56);
            display.print("FIRE to restart");
            display.display();
        }

        if (buzzer_timer > 0) {
            buzzer_timer--;
            if (buzzer_timer == 0) {
                Buzzer_Stop();
            }
        }

        return;
    }

    // Update cooldown
    if (fire_cooldown > 0) {
        fire_cooldown--;
    }

    // Update game objects
    Update_Bullets();
    Update_Enemies();

    // Check collisions
    if (Check_Bullet_Hit()) {
        score++;
        Buzzer_Play(6);
    }

    if (Check_Ship_Hit(ship_x, ship_y)) {
        if (player_lives > 0) {
            player_lives--;
        }
        Servo_Set_Lives(player_lives);
        UART_Transmit(0xAA);
        Buzzer_Play(12);

        if (player_lives == 0) {
            game_over = 1;
        }
    }

    // Update buzzer pitch from potentiometer
    uint16_t pot_value = ADC_Read();
    OCR2A = (pot_value / 8) + 30;

    // Render frame
    if (display_ready) {
        display.clearDisplay();
        display.fillRect(ship_x, ship_y, 3, 3, SSD1306_WHITE);
        Draw_Bullets();
        Draw_Enemies();
        display.display();
    }

    // Handle buzzer timing
    if (buzzer_timer > 0) {
        buzzer_timer--;
        if (buzzer_timer == 0) {
            Buzzer_Stop();
        }
    }
}
