#include <Adafruit_NeoPixel.h>

#define BUT1    A4
#define BUT2    A3
#define BUT3    A2
#define BUT4    A1
#define BUTOK   A0
#define WS       8
#define OE       9
#define LAT     10
#define SER     11
#define CLK     13
#define ROWS    10
#define COLS     4
#define LEDS    (ROWS * COLS)
#define LEVELS  18


Adafruit_NeoPixel code_leds = Adafruit_NeoPixel(LEDS, WS, NEO_GRB + NEO_KHZ800);

uint8_t buttons[] = {BUT1, BUT2, BUT3, BUT4};
uint8_t brightness = 10;
uint8_t code[COLS];
uint8_t input[ROWS][COLS];
uint8_t tries;
uint8_t level;
uint8_t correct[ROWS];
const uint32_t colors[] = {
	0xFF0000, 
	0x0000FF,
	0x00FF00,
	0xFFFF00,
	0xFF00FF,
	0x00FF88,
	0xFF4400,
	0xFFFFFF,
};

void setup() {
	pinMode(LAT, OUTPUT);
	pinMode(OE, OUTPUT);
	pinMode(CLK, OUTPUT);
	pinMode(SER, OUTPUT);
	pinMode(BUT1, INPUT_PULLUP);
	pinMode(BUT2, INPUT_PULLUP);
	pinMode(BUT3, INPUT_PULLUP);
	pinMode(BUT4, INPUT_PULLUP);
	pinMode(BUTOK, INPUT_PULLUP);
	analogWrite(OE, 0xFF);
	for (uint8_t r = 0; r < COLS; r++) {
		shiftOut(SER, CLK, MSBFIRST, 0xFF);
	}
	digitalWrite(LAT, HIGH);
	digitalWrite(LAT, LOW);
	analogWrite(OE, 255 - brightness);
	code_leds.begin();
	// Animation while not pressed OK
	while (digitalRead(BUTOK)) {
		for (uint8_t i = 0; i < ROWS; i++) {
			correct[i] = 0b10100101 * ((millis() / 100) % 11 < i) + 0b01011010 * ((millis() / 100) % 11 > i);
			for (uint8_t l = 0; l < COLS; l++) {
				code_leds.setPixelColor(i * COLS + l, brightness * ((millis() / 100) % 11 >= i), brightness * ((millis() / 100) % 11 <= i), 0);
			}
		}
		for (uint8_t r = 0; r < ROWS; r++) {
			shiftOut(SER, CLK, MSBFIRST, correct[ROWS - 1 - r]);
		}
		code_leds.show();
		digitalWrite(LAT, HIGH);
		digitalWrite(LAT, LOW);
		delay(10);
	}
}

uint32_t dim(uint32_t color, uint8_t factor) {
	uint32_t r, g, b;
	r = (color >> 16) & 0xFF;
	g = (color >> 8)  & 0xFF;
	b =	color         & 0xFF;
	r = (r * factor) >> 8;
	g = (g * factor) >> 8;
	b = (b * factor) >> 8;
	color = (r << 16) + (g << 8) + b;
	return color;
}

boolean check() {
	uint8_t corr, checked, pos_corr;
	for (uint8_t c = 0; c < COLS; c++) {
		if (input[tries][c] == code[c]) {
			pos_corr++;
			checked |= 1 << c;
		}
	}
	for (uint8_t c = 0; c < COLS; c++) {
		for (uint8_t col = 0; col < COLS; col++) {
			if (!(checked & (1 << c))) {
				if (input[tries][col] == code[c]) {
					corr++;
					checked |= 1 << c;
				}
			}
		}
	}
	// Green 0b01 for all correct positioned colors
	for (uint8_t i = 0; i < pos_corr; i++) {
		correct[tries] ^= 0b01 << (2 * (COLS - 1 - i) + (i > 1));
		// + (i > 1) makes 0b01 to 0b10 if displayed on the right side (R/G inverted)
	}
	// Yellow 0b11 for all correct colors
	for (uint8_t i = pos_corr; i < corr + pos_corr; i++) {
		correct[tries] ^= 0b11 << (2 * (COLS - 1 - i));
	}
	// Write to LEDs
	for (uint8_t r = 0; r < ROWS; r++) {
		shiftOut(SER, CLK, MSBFIRST, correct[ROWS - 1 - r]);
	}
	digitalWrite(LAT, HIGH);
	digitalWrite(LAT, LOW);
	// Return if all colors are correct positioned
	return pos_corr == 4;
}

void showLevel() {
	// Reset
	for (uint8_t i = 0; i < LEDS; i++) {
		code_leds.setPixelColor(i, 0);
	}
	for (uint8_t i = 0; i < ROWS; i++) {
		correct[i] = 0xFF;
	}
	// Show available colors for this level
	for (uint8_t i = 0; i < (level % 3) + 6; i++) {
		code_leds.setPixelColor(i, dim(colors[i], brightness));
	}
	// Show red for all forbidden rows
	for (uint8_t i = 0; i < level / 3; i++) {
		correct[i] = 0b01011010;
	}
	// Write to LEDs
	for (uint8_t r = 0; r < ROWS; r++) {
		shiftOut(SER, CLK, MSBFIRST, correct[ROWS - 1 - r]);
	}
	digitalWrite(LAT, HIGH);
	digitalWrite(LAT, LOW);
	code_leds.show();
}

void newGame() {
	randomSeed(analogRead(A7) * random(1123));
	showLevel();
	while (!digitalRead(BUTOK)) delay(10);
	delay(10);
	while (digitalRead(BUTOK)) {
		// Adjust level with BUT1 and BUT4 until OK is pressed
		if (!digitalRead(BUT1)) {
			if (level == 0) level = LEVELS;
			level--;
			showLevel();
			while (!digitalRead(BUT1)) delay(10);
			delay(10);
		}
		if (!digitalRead(BUT4)) {
			level++;
			if (level == LEVELS) level = 0;
			showLevel();
			while (!digitalRead(BUT4)) delay(10);
			delay(10);
		}
	}
	// Reset
	for (uint8_t i = 0; i < LEDS; i++) {
		code_leds.setPixelColor(i, 0);
	}
	code_leds.show();
	tries = level / 3;
	// Generate code
	for (uint8_t i = 0; i < COLS;) {
		code[i] = random((level % 3) + 6);
		boolean doublette = false;
		// Do not allow duplicate colors
		for (uint8_t j = 0; j < i; j++) {
			if (code[i] == code[j]) {
				doublette = true;
				break;
			}
		}
		i += !doublette;
	}
}


void loop() {
	newGame();
	while (!digitalRead(BUTOK)) delay(10);
	delay(10);
	boolean solved = false;
	while (tries < 10) {
		// New try
		if (tries > level / 3) {
			// If not first try, copy last tried colors to current colors
			input[tries][0] = input[tries - 1][0];
			input[tries][1] = input[tries - 1][1];
			input[tries][2] = input[tries - 1][2];
			input[tries][3] = input[tries - 1][3];
		} else {
			input[tries][0] = 0;
			input[tries][1] = 0;
			input[tries][2] = 0;
			input[tries][3] = 0;
		}
		// Show colors
		for (uint8_t i = 0; i < COLS; i++) {
			// tries % 2 to detect if its a odd or even row (zigzag -> LEDs starting from left or right)
			if (tries % 2) code_leds.setPixelColor(tries * COLS + i, dim(colors[input[tries][i]], brightness));
			else code_leds.setPixelColor(tries * COLS + COLS - 1 - i, dim(colors[input[tries][i]], brightness));
		}
		code_leds.show();
		while (digitalRead(BUTOK)) {
			for (uint8_t b = 0; b < COLS; b++) {
				// Select next color
				if (!digitalRead(buttons[b])) {
					input[tries][b]++;
					input[tries][b] %= (level % 3) + 6;
					// Again show colors
					if (tries % 2) code_leds.setPixelColor(tries * COLS + b, dim(colors[input[tries][b]], brightness));
					else code_leds.setPixelColor(tries * COLS + COLS - 1 - b, dim(colors[input[tries][b]], brightness));
					code_leds.show();
					// Debounce
					while (!digitalRead(buttons[b])) delay(10);
					delay(10);
				}
			}
		}
		solved = check();
		while (!digitalRead(BUTOK)) delay(10);
		delay(10);
		if (solved) {
			break;
		}
		tries++;
	}
	if (solved) {
		while (digitalRead(BUTOK)) {
			correct[tries] = 0xFF ^ 0b01011010 * ((millis() / 500) % 2);
			for (uint8_t r = 0; r < ROWS; r++) {
				shiftOut(SER, CLK, MSBFIRST, correct[ROWS - 1 - r]);
			}
			digitalWrite(LAT, HIGH);
			digitalWrite(LAT, LOW);
		}
	} else {
		while (digitalRead(BUTOK)) delay(10);
		while (!digitalRead(BUTOK)) delay(10);
		// Animate game over screen
		for (tries = 0; tries < ROWS; tries++) {
			for (uint8_t i = 0; i < COLS; i++) {
				if (tries % 2) code_leds.setPixelColor(tries * COLS + i, dim(colors[code[i]], brightness));
				else code_leds.setPixelColor(tries * COLS + COLS - 1 - i, dim(colors[code[i]], brightness));
			}
			code_leds.show();
			correct[tries] = 0xFF ^ 0b10100101;
			for (uint8_t r = 0; r < ROWS; r++) {
				shiftOut(SER, CLK, MSBFIRST, correct[ROWS - 1 - r]);
			}
			digitalWrite(LAT, HIGH);
			digitalWrite(LAT, LOW);
			delay(500);
		}
	}
	while (digitalRead(BUTOK)) delay(10);
}
