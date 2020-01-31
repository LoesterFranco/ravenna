#define RAVEN2_BOARD  // comment is using Raven version 1 testboard

#include "../raven_defs.h"

// --------------------------------------------------------
// Firmware routines
// --------------------------------------------------------

// Copy the flash worker function to SRAM so that the SPI can be
// managed without having to read program instructions from it.

void flashio(uint32_t *data, int len, uint8_t wrencmd)
{
	uint32_t func[&flashio_worker_end - &flashio_worker_begin];

	uint32_t *src_ptr = &flashio_worker_begin;
	uint32_t *dst_ptr = func;

	while (src_ptr != &flashio_worker_end)
		*(dst_ptr++) = *(src_ptr++);

	((void(*)(uint32_t*, uint32_t, uint32_t))func)(data, len, wrencmd);
}

//--------------------------------------------------------------
// NOTE: Volatile write *only* works with command 01, making the
// above routing non-functional.  Must write all four registers
// status, config1, config2, and config3 at once.
//--------------------------------------------------------------
// (NOTE: Forces quad/ddr modes off, since function runs in single data pin mode)
// (NOTE: Also sets quad mode flag, so run this before entering quad mode)
//--------------------------------------------------------------

void set_flash_latency(uint8_t value)
{
	reg_spictrl = (reg_spictrl & ~0x007f0000) | ((value & 15) << 16);

	uint32_t buffer_wr[2] = {0x01000260, ((0x70 | value) << 24)};
	flashio(buffer_wr, 5, 0x50);
}

//--------------------------------------------------------------

void putchar(char c)
{
	if (c == '\n')
		putchar('\r');
	reg_uart_data = c;
}

void print(const char *p)
{
	while (*p)
		putchar(*(p++));
}

void print_hex(uint32_t v, int digits)
{
	for (int i = digits - 1; i >= 0; i--) {
		char c = "0123456789abcdef"[(v >> (4*i)) & 15];
		putchar(c);
	}
}

void print_dec(uint32_t v)
{
	if (v >= 2000) {
		print("OVER");
		return;
	}
	else if (v >= 1000) { putchar('1'); v -= 1000; }
	else putchar(' ');

	if 	(v >= 900) { putchar('9'); v -= 900; }
	else if	(v >= 800) { putchar('8'); v -= 800; }
	else if	(v >= 700) { putchar('7'); v -= 700; }
	else if	(v >= 600) { putchar('6'); v -= 600; }
	else if	(v >= 500) { putchar('5'); v -= 500; }
	else if	(v >= 400) { putchar('4'); v -= 400; }
	else if	(v >= 300) { putchar('3'); v -= 300; }
	else if	(v >= 200) { putchar('2'); v -= 200; }
	else if	(v >= 100) { putchar('1'); v -= 100; }
	else putchar('0');
		
	if 	(v >= 90) { putchar('9'); v -= 90; }
	else if	(v >= 80) { putchar('8'); v -= 80; }
	else if	(v >= 70) { putchar('7'); v -= 70; }
	else if	(v >= 60) { putchar('6'); v -= 60; }
	else if	(v >= 50) { putchar('5'); v -= 50; }
	else if	(v >= 40) { putchar('4'); v -= 40; }
	else if	(v >= 30) { putchar('3'); v -= 30; }
	else if	(v >= 20) { putchar('2'); v -= 20; }
	else if	(v >= 10) { putchar('1'); v -= 10; }
	else putchar('0');

	if 	(v >= 9) { putchar('9'); v -= 9; }
	else if	(v >= 8) { putchar('8'); v -= 8; }
	else if	(v >= 7) { putchar('7'); v -= 7; }
	else if	(v >= 6) { putchar('6'); v -= 6; }
	else if	(v >= 5) { putchar('5'); v -= 5; }
	else if	(v >= 4) { putchar('4'); v -= 4; }
	else if	(v >= 3) { putchar('3'); v -= 3; }
	else if	(v >= 2) { putchar('2'); v -= 2; }
	else if	(v >= 1) { putchar('1'); v -= 1; }
	else putchar('0');
}

// ----------------------------------------------------------------------

void cmd_read_flash_regs_print(uint32_t addr, const char *name)
{
    uint32_t buffer[2] = {0x65000000 | addr, 0x0};
    flashio(buffer, 6, 0);

    print("0x");
    print_hex(addr, 6);
    print(" ");
    print(name);
    print(" 0x");
    print_hex(buffer[1], 2);
    print("  ");
}

// ----------------------------------------------------------------------

void cmd_read_flash_regs()
{
    cmd_read_flash_regs_print(0x800000, "SR1V");
    cmd_read_flash_regs_print(0x800002, "CR1V");
    cmd_read_flash_regs_print(0x800003, "CR2V");
    cmd_read_flash_regs_print(0x800004, "CR3V");
}
    

// ----------------------------------------------------------------------
// Raven demo:  Demonstrate various capabilities of the Raven testboard.
//
// 1. Flash:  Set the flash to various speed modes
// 2. GPIO:   Drive the LEDs in a loop
// 3. UART:   Print text to the UART LCD display
// 4. RC Osc: Enable the 100MHz RC oscillator and output on a GPIO line
// 5. DAC:    Enable the DAC and ramp
// 6. ADC:    Enable both ADCs and periodically read and display the DAC
//	      and bandgap values on the UART display.
// ----------------------------------------------------------------------
// NOTE:  There are two versions of this demo.  Demo 1 runs only up to
//	  2X speed (DSPI + CRM).  The higher speeds cause some timing
//	  trouble, which may be due to reflections on the board wires
//	  for IO2 and IO3.  Not sure of the mechanism.  To keep the
//	  boards running without failure, do not use QSPI modes.

void main()
{
	uint32_t i, j, m, r, mode;

	/* Note that it definitely does not work in simulation because	*/
	/* the behavioral verilog for the SPI flash does not support	*/
	/* the configuration register read/write functions.		*/

    //	set_flash_latency(8);

	// Start in standard (1x speed) mode

	// Set m for speed multiplier:
	// 1 standard (1x), 2 for DSPI (2x)
//	mode = 1;
//	m = 1;

	// Enable GPIO (all output, ena = 0)
	reg_gpio_ena = 0x0000;
	reg_gpio_pu = 0x0000;
	reg_gpio_pd = 0x000f;
	reg_gpio_data = 0x000f;

	// Set UART clock to 9600 baud
//    #ifdef RAVEN2_BOARD
//        reg_uart_clkdiv = 8333;
//    #else
//        reg_uart_clkdiv = 10417;
//    #endif

    for (j = 0; j < 170000; j++);

	while (1) {
        reg_gpio_data = 0x0000;
        for (j = 0; j < 70000; j++);
        reg_gpio_data = 0x0001;
        for (j = 0; j < 70000; j++);
        reg_gpio_data = 0x0002;
        for (j = 0; j < 70000; j++);
        reg_gpio_data = 0x0004;
        for (j = 0; j < 70000; j++);
        reg_gpio_data = 0x0008;
        for (j = 0; j < 70000; j++);
        reg_gpio_data = 0x0004;
        for (j = 0; j < 70000; j++);
        reg_gpio_data = 0x0002;
        for (j = 0; j < 70000; j++);
        reg_gpio_data = 0x0001;
        for (j = 0; j < 70000; j++);
	}
}

