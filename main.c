#include "Resource.h"

volatile unsigned int* const PPU_BG1_SCROLL_X = (int*)0x2810;
volatile unsigned int* const PPU_BG1_SCROLL_Y = (int*)0x2811;
volatile unsigned int* const PPU_BG1_ATTR = (int*)0x2812;
volatile unsigned int* const PPU_BG1_CTRL = (int*)0x2813;
volatile unsigned int* const PPU_BG1_TILE_ADDR = (int*)0x2814;

volatile unsigned int* const PPU_BG2_CTRL = (int*)0x2819;

volatile unsigned int* const PPU_BG1_SEGMENT_ADDR = (int*)0x2820;

volatile unsigned int* const PPU_SPRITE_CTRL = (int*)0x2842;
volatile unsigned int* const PPU_COLOR = (int*)0x2B00;

volatile unsigned int* const SYSTEM_CTRL = (int*)0x3d20;
volatile unsigned int* const INT_CTRL = (int*)0x3d21;
volatile unsigned int* const INT_CLEAR = (int*)0x3d22;
volatile unsigned int* const WATCHDOG_CLEAR = (int*)0x3d24;

volatile unsigned int* const IO_MODE = (int*)0x3D00;
volatile unsigned int* const PORTC_DATA = (int*)0x3D0B;
volatile unsigned int* const PORTC_DIR = (int*)0x3D0D;
volatile unsigned int* const PORTC_ATTR = (int*)0x3D0E;
volatile unsigned int* const PORTC_SPECIAL = (int*)0x3D0F;

volatile unsigned int* const UART_CONTROL = (int*)0x3D30;
volatile unsigned int* const UART_STATUS = (int*)0x3D31;
volatile unsigned int* const UART_BAUDRATE_LOW = (int*)0x3D33;
volatile unsigned int* const UART_BAUDRATE_HIGH = (int*)0x3D34;
volatile unsigned int* const UART_TXBUF = (int*)0x3D35;
volatile unsigned int* const UART_RXBUF = (int*)0x3D36;

static int irq3_rx_count = 0;
static int irq3_tx_count = 0;
static int irq5_lo_count = 0;
static int irq5_hi_count = 0;

static int colors_val = 0;
static int buttons_val = 0;
static int joyx_val = 0;
static int joyy_val = 0;
static int last_rx = 0;
static int idle_counter = 0;

static const char *hextable = "0123456789abcdef";

static int tilemap[2048];

#define make_color(r, g, b) ((r << 10) | (g << 5) | (b << 0))

void print_dec(int y, int x, unsigned int value)
{
	int digits[5];
	int i, j, len = 0;

	do {
		digits[len++] = value % 10;
	} while(value /= 10);

	for (i = len - 1, j = 0; i >= 0; i--, j++) {
		tilemap[64*y + x + i] = hextable[digits[j]];
	}
}

void print_hex(int y, int x, int value)
{
	int i;
	for(i = 3; i >= 0; i--, value >>= 4){
		tilemap[64*y + x + i] = hextable[value & 0x0f];
	}
}

void print_hex2(int y, int x, int value)
{
	int i;
	for(i = 1; i >= 0; i--, value >>= 4){
		tilemap[64*y + x + i] = hextable[value & 0x0f];
	}
}

void print_string(int y, int x, const char *str) {
	int i = 0;
	while(str[i]){
		tilemap[64*y + (x++)] = str[i++];
	}
}

void clear_tilemap(){
	int i;
	for (i = 0; i < sizeof(tilemap); i++)
		tilemap[i] = 0x20;
}

void IRQ3(void) __attribute__ ((ISR));
void IRQ5(void) __attribute__ ((ISR));

void IRQ3(void)  {
	*INT_CLEAR = 0x0100;
	if(*UART_STATUS & 1){
		int data = *UART_RXBUF;
		*UART_STATUS = 1;
		last_rx = data;
		irq3_rx_count++;
		switch(data & 0xf0){
			case 0x90:
				colors_val = data;
				break;
			case 0xa0:
				buttons_val = data;
				break;
			case 0xc0:
				joyx_val = data;
				break;
			case 0x80:
				joyy_val = data;
				break;
			case 0x50:
				if(data == 0x55) {
					idle_counter++;
				}
				break;
		}

	}
	else if(*UART_STATUS & 2) {
		*UART_STATUS = 2;
		irq3_tx_count++;
		//transmit done, do more here later
	}
}

inline void enable_cts() {
	*PORTC_DATA |= 0x0100;
}

inline void disable_cts() {
	*PORTC_DATA &= ~0x0100;
}

inline int read_rts(){
	return *PORTC_DATA & 0x0400;
}

void IRQ5(void) {
	int data = read_rts();
	*INT_CLEAR = 0x0200;
	if(data) {
		irq5_hi_count++;
		disable_cts();
	}
	else {
		irq5_lo_count++;
		enable_cts();
	}
}

int main()
{
	*SYSTEM_CTRL = 0;
	*WATCHDOG_CLEAR = 0x55aa;

	// Configure PPU
	*PPU_BG1_SCROLL_X = 0;
	*PPU_BG1_SCROLL_Y = 0;
	*PPU_BG1_ATTR = 0;
	*PPU_BG1_CTRL = 0x0a;
	*PPU_BG2_CTRL = 0;
	*PPU_SPRITE_CTRL = 0;

	*PPU_BG1_TILE_ADDR = (int) tilemap;
	*PPU_BG1_SEGMENT_ADDR = RES_FONT_BIN_SA >> 6;

	// Configure port C
	*IO_MODE = 0x09;
	*PORTC_DATA = 0x8180;
	*PORTC_DIR = 0xcbc0;
	*PORTC_ATTR = 0xcbc0;
	*PORTC_SPECIAL = 0x6000;

	// Configure UART
	*UART_BAUDRATE_LOW = 0xA0;
	*UART_BAUDRATE_HIGH = 0xFE;
	*UART_CONTROL = 0xc3;
	*UART_STATUS = 3;

	PPU_COLOR[0] = make_color(29, 26, 15);
	PPU_COLOR[1] = make_color(0, 8, 16);

	// Enable interrupts
	*INT_CTRL |= 0x0200; // External interrupt enable (IRQ5)
	*INT_CTRL |= 0x0100; // UART interrupt enable (IRQ3)
	__asm__("irq on");

	clear_tilemap();

	// Send start command to controller
	enable_cts();
	*UART_TXBUF = 0x71;
	while (*UART_STATUS & 0x40); // Wait synchronously for tx to finish
	disable_cts();

	print_string(2, 2, "Controller test");

	// Print some interesting data
	for(;;){
		print_hex2(4, 2, colors_val);
		print_hex2(4, 6, buttons_val);
		print_hex2(4, 10, joyx_val);
		print_hex2(4, 14, joyy_val);
		print_string(4, 19, "#idle:");
		print_dec(4, 26, idle_counter);

		print_string(7, 2, "IRQ3");
		print_string(7, 8, "#rx:");
		print_dec(7, 13, irq3_rx_count);
		print_string(7, 19, "#tx:");
		print_dec(7, 24, irq3_tx_count);
		print_string(8, 8, "last rx:");
		print_hex2(8, 17, last_rx);

		print_string(11, 2, "IRQ5");
		print_string(11, 8, "#0:");
		print_dec(11, 13, irq5_lo_count);
		print_string(11, 19, "#1:");
		print_dec(11, 24, irq5_hi_count);
	}

	return 0;
}
