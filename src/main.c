
#include "main.h"
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "tty_utils.h"
#include "utf_utils.h"
#include "common/default_isa.h"
#include "ibm437.h"

int state = STATE_STOP;

// Current state.
bool gr8cpu_running = false;
static uint8_t ram_reserve[65536];
gr8cpurev3_t cpu;
size_t stats_mmio_r;
size_t stats_mmio_w;

// Keyboard buffer.
char keyb_buf[KEYB_BUF_LEN];
size_t keyb_buf_start = 0;
size_t keyb_buf_end = 0;

// Frequencies.
static uint64_t delay;
static uint64_t cycles;
double current_hertz = 0;
double target_hertz  = 1000000.0;
//  10.0 MHz
//   1.0 MHz
// 100.0 KHz
//  10.0 KHz
//   1.0 KHz
// 500.0  Hz
// 250.0  Hz
// 100.0  Hz
//  50.0  Hz
//  10.0  Hz
//   5.0  Hz
//   1.0  Hz
//   0.5  Hz
static int freq_sel = 2;
char freq_sel_desc[16]  = "1.0 MHz";
char freq_real_desc[16] = "0.0 Hz";
uint32_t freq_delays[N_FREQS] = {
	20000,   // 50x /s: 100MHz
	20000,   // 50x /s: 10MHz
	20000,   // 50x /s: 1MHz
	20000,   // 50x /s: 100KHz
	20000,   // 50x /s: 10KHz
	20000,   // 50x /s: 1KHz
	20000,   // 50x /s: 500Hz
	20000,   // 50x /s: 250Hz
	20000,   // 50x /s: 100Hz
	20000,   // 50x /s: 50Hz
	100000,  // 10x /s: 10Hz
	200000,  // 5x  /s: 5Hz
	1000000, // 1x  /s: 1Hz
	2000000, // .5x /s: 0.5Hz
};
uint32_t freq_cycles[N_FREQS] = {
	2000000,// 100MHz
	200000, // 10MHz
	20000,  // 1MHz
	2000,   // 100KHz
	200,    // 10KHz
	20,     // 1KHz
	10,     // 500Hz
	5,      // 250Hz
	2,      // 100Hz
	1,      // 50Hz
	1,      // 10Hz
	1,      // 5Hz
	1,      // 1Hz
	1,      // 0.5Hz
};

static void set_blocking();
static void set_nonblocking();
static void handle_keyb(char c);
static void handle_run(char c);
static void handle_stop(char c);
static void handle_show(char c);
static void change_freq(char c);

uint8_t helloworld_rom[] = {
	//entry:
	0x7e, 0xff,			// VST $ff
	0x7b, 0x24, 0x00,	// GPTR [sometext]
	0x2a, 0x00, 0x01,	// MOV [ptr], X
	0x2b, 0x01, 0x01,	// MOV [ptr_hi], Y
	0x02, 0x0f, 0x00,	// CALL print
	0x7f,				// HLT
	
	//print:
	0x25, 0x00, 0x01,	// MOV A, (ptr)
	0x3c, 0x00,			// CMP A, $00
	0x0f, 0x23, 0x00,	// BEQ .exit
	0x29, 0xfd, 0xfe,	// MOV [$fefd], A
	0x3f, 0x00, 0x01,	// INC [ptr]
	0x4b, 0x01, 0x01,	// INCC [ptr_hi]
	0x0e, 0x0f, 0x00,	// JMP print
	//.exit:
	0x03,
	
	//sometext:
	0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2c, 0x20,	// "Hello, "
	0x77, 0x6f, 0x72, 0x6c, 0x64, 0x21, 0x0a,	// "World!\n"
	0x00,	// "\0"
};

options_t options;
static bool dirty;

int main(int argc, char **argv) {
	// Add the exit handler.
	if (atexit(exithandler)) {
		fputs("Could not register exit handler; aborting!\n", stderr);
		return -1;
	}
	
	// Set TTY mode to disable line buffering and echoing.
	system("stty cbreak -echo -isig");
	
	// Parse the options.
	options.disk_file = NULL;
	options.exec_file = NULL;
	options.run_immediately = false;
	int i;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--disk-file")) {
			if (i < argc - 1 && *argv[i + 1] != '-') {
				i ++;
				options.disk_file = argv[i];
			} else {
				fprintf(stderr, "No path provided for '%s'", argv[i]);
				return 1;
			}
		} else if (!strcmp(argv[i], "-x") || !strcmp(argv[i], "--exec")) {
			options.run_immediately = true;
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			options.show_help = true;
		} else if (*argv[i] == '-') {
			fprintf(stderr, "No such option '%s'", argv[i]);
			return 1;
		} else {
			break;
		}
	}
	if (options.show_help) {
		printf("%s [options] exec-file\n\n", *argv);
		printf("    -h  --help\n");
		printf("                Show this options list.\n\n");
		printf("    -x  --exec\n");
		printf("                Start running the program immediately.\n\n");
		printf("    -d file\n");
		printf("    --disk-file file\n");
		printf("                Select the disk image file.\n\n");
		return 0;
	}
	if (i < argc - 1) {
		fprintf(stderr, "Too many files specified");
		return 1;
	} else if (i < argc) {
		options.exec_file = argv[i];
	}
	
	// Print some lines.
	fputs("\n\n\n\n\n\n", stdout);
	
	// Reset the CPU.
	cpu_reset();
	cpu.rom = helloworld_rom;
	cpu.romLen = sizeof(helloworld_rom);
	// Load the file if possible.
	if (options.exec_file) {
		uint8_t *buf;
		size_t len;
		if (load_file(options.exec_file, &buf, &len)) {
			if (len > 0xfdff) {
				free(buf);
			} else {
				cpu.rom = buf;
				cpu.romLen = len;
			}
		}
	}
	
	if (options.run_immediately) {
		// Set stdin mode to non-blocking so we can read and get EOF instead of waiting.
		set_nonblocking();
	}
	redraw();
	vtty_puts("\n" ANSI_BOLD_INV "GR8EMU v1.0" ANSI_RESET "\n");
	// Some loop.
	uint64_t last_redraw = millis();
	uint64_t last_time   = micros();
	delay                = freq_delays[freq_sel];
	cycles               = freq_cycles[freq_sel];
	dirty                = false;
	gr8cpu_running       = options.run_immediately;
	while (1) {
		uint64_t now = micros();
		char c = getc(stdin);
		if (state != STATE_KEYB && c == CTRL_C) {
			printf("\033[65535;65535H\nStopping...\n");
			break;
		} else {
			switch (state) {
				case STATE_KEYB:
					handle_keyb(c);
					break;
				case STATE_RUN:
					handle_run(c);
					break;
				case STATE_STOP:
					handle_stop(c);
					break;
				case STATE_SHOW:
					handle_show(c);
					break;
			}
			change_freq(c);
		}
		if (last_time + delay <= now && gr8cpu_running) {
			// Tick it.
			int res = gr8cpurev3_tick(&cpu, cycles, TICK_MODE_NORMAL);
			
			// Find real hertz frequency.
			uint64_t spent = now - last_time;
			double real_freq = 1000000.0 / (double) spent * (double) cycles;
			desc_freq(freq_real_desc, real_freq);
			
			// Some housekeeping.
			if (res) gr8cpu_running = false;
			last_time = now;
			dirty = true;
		}
		if (last_redraw + 50 < now && dirty) {
			dirty = false;
			redraw();
		}
	}
}

static void set_blocking() {
	// Set stdin mode to blocking so we wait when paused.
	int flags = fcntl(stdin->_fileno, F_GETFL, 0);
	fcntl(stdin->_fileno, F_SETFL, flags & ~O_NONBLOCK);
}

static void set_nonblocking() {
	// Set stdin mode to non-blocking so we can read and get EOF instead of waiting.
	int flags = fcntl(stdin->_fileno, F_GETFL, 0);
	fcntl(stdin->_fileno, F_SETFL, flags | O_NONBLOCK);
}

static void handle_keyb(char c) {
	bool q = 1;
	// Set to nonblock temporarily.
	int flags = fcntl(stdin->_fileno, F_GETFL, 0);
	fcntl(stdin->_fileno, F_SETFL, flags | O_NONBLOCK);
	if (c == 0x1B && getc(stdin) == '[') {
		// Yes, two consecutive reads from STDIN.
		// And yes, D is left and C is right.
		switch (getc(stdin)) {
			case 'A':
				keybbuf_add(GR8CPU_UP);
				break;
			case 'B':
				keybbuf_add(GR8CPU_DOWN);
				break;
			case 'D':
				keybbuf_add(GR8CPU_LEFT);
				break;
			case 'C':
				keybbuf_add(GR8CPU_RIGHT);
				break;
		}
	} else if (c == MAP_UNKEYB) {
		state = gr8cpu_running ? STATE_RUN : STATE_STOP;
	} else if (c == 0x7F) {
		// Translate expectation of backspace.
		keybbuf_add('\b');
	} else if (c > 0) {
		keybbuf_add(c);
	} else {
		q = 0;
	}
	// Revert changes.
	fcntl(stdin->_fileno, F_SETFL, flags);
	dirty |= q;
}

static void handle_run(char c) {
	bool q = 1;
	if (c == MAP_PAUSE) {
		gr8cpu_running = false;
		set_blocking();
		strcpy(freq_real_desc, "0.0 Hz");
	} else if (c == MAP_KEYB) {
		state = STATE_KEYB;
	} else if (c == MAP_RESET) {
		gr8cpu_running = false;
		set_blocking();
		strcpy(freq_real_desc, "0.0 Hz");
		cpu_reset();
		redraw();
	} else if (c == MAP_SHOW) {
		state = STATE_SHOW;
	} else {
		q = 0;
	}
	if (!gr8cpu_running) {
		state = STATE_STOP;
		set_blocking();
		strcpy(freq_real_desc, "0.0 Hz");
		dirty = 1;
	} else {
		dirty |= q;
	}
}

static void handle_stop(char c) {
	bool q = 1;
	if (c == MAP_UNPAUSE) {
		set_nonblocking();
		gr8cpu_running = true;
	} else if (c == MAP_KEYB) {
		state = STATE_KEYB;
	} else if (c == MAP_CSTEP) {
		int res = gr8cpurev3_tick(&cpu, 1, TICK_MODE_NORMAL);
		redraw();
	} else if (c == MAP_ISTEP) {
		int res = gr8cpurev3_tick(&cpu, 1, TICK_MODE_STEP_IN);
		redraw();
	} else if (c == MAP_MSTEP) {
		int res = gr8cpurev3_tick(&cpu, 8192, TICK_MODE_STEP_OVER);
		redraw();
	} else if (c == MAP_XSTEP) {
		int res = gr8cpurev3_tick(&cpu, 8192, TICK_MODE_STEP_OUT);
		redraw();
	} else if (c == MAP_RESET) {
		gr8cpu_running = false;
		cpu_reset();
		redraw();
		vtty_puts("\n" ANSI_BOLD_INV "RESET" ANSI_RESET "\n");
	} else if (c == MAP_SHOW) {
		state = STATE_SHOW;
	} else {
		q = 0;
	}
	if (gr8cpu_running) {
		state = STATE_RUN;
		dirty = 1;
	} else {
		dirty |= q;
	}
}

static void handle_show(char c) {
	bool q = 1;
	if (c == MAP_BACK) {
		state = gr8cpu_running ? STATE_RUN : STATE_STOP;
	} else if (c == MAP_SHOW_STAT) {
		showing[SHOW_INDEX_STAT] ^= 1;
	} else if (c == MAP_SHOW_PC) {
		showing[SHOW_INDEX_PC] ^= 1;
	} else if (c == MAP_SHOW_REGS) {
		showing[SHOW_INDEX_REGS] ^= 1;
	} else if (c == MAP_SHOW_SREGS) {
		showing[SHOW_INDEX_SREGS] ^= 1;
	} else {
		q = 0;
	}
	dirty |= q;
}

static void change_freq(char c) {
	if (c == '-' || c == '_') {
		// Decrease freq.
		if (freq_sel < N_FREQS - 1) {
			freq_sel ++;
			delay    = freq_delays[freq_sel];
			cycles   = freq_cycles[freq_sel];
			dirty = true;
		}
	} else if (c == '=' || c == '+') {
		// Increase freq.
		if (freq_sel > 0) {
			freq_sel --;
			delay    = freq_delays[freq_sel];
			cycles   = freq_cycles[freq_sel];
			dirty = true;
		}
	}
	target_hertz = 1000000.0 / delay * cycles;
	desc_freq(freq_sel_desc, target_hertz);
}

void desc_freq(char *buf, double freq) {
	const static char *names[4] = {
		"Hz", "KHz", "MHz", "GHz"
	};
	int i;
	for (i = 0; i < 3 && freq > 750.0; i++) {
		freq /= 1000.0;
	}
	sprintf(buf, "%.1f %s", freq, names[i]);
}

// Loads the file into an array and returns the size and pointer.
bool load_file(char *path, uint8_t **buf, size_t *size) {
	FILE *f = fopen(path, "rb");
	if (!f) return false;
	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);
	uint8_t *mem = malloc(len);
	if (!mem) {
		fclose(f);
		return false;
	}
	len = fread(mem, 1, len, f);
	*buf = mem;
	*size = len;
	fclose(f);
	return true;
}

// Returns time with millisecond precision.
uint64_t millis() {
	struct timespec res;
	timespec_get(&res, TIME_UTC);
	return res.tv_sec * 1000 + res.tv_nsec / 1000000;
}

// Returns time with microsecond precision.
uint64_t micros() {
	struct timespec res;
	timespec_get(&res, TIME_UTC);
	return res.tv_sec * 1000000 + res.tv_nsec / 1000;
}

// Returns time with nanosecond precision.
uint64_t nanos() {
	struct timespec res;
	timespec_get(&res, TIME_UTC);
	return res.tv_sec * 1000000000 + res.tv_nsec;
}

// Called when a character is added to the keyboard buffer.
bool keybbuf_add(char c) {
	int next = (keyb_buf_end + 1) % KEYB_BUF_LEN;
	if (next != keyb_buf_start) {
		keyb_buf[keyb_buf_end] = c;
		keyb_buf_end = next;
		return true;
	}
	return false;
}

// Called when a character is read from the keyboard buffer.
char keybbuf_read(bool notouchy) {
	if (keyb_buf_start != keyb_buf_end) {
		char c = keyb_buf[keyb_buf_start];
		if (!notouchy) {
			keyb_buf_start = (keyb_buf_start + 1) % KEYB_BUF_LEN;
		}
		return c;
	}
	return 0;
}

// Creates a copy of the keyboard buffer of at most len characters.
// Buffer must be at least len+1 characters.
void keybbuf_copy(char *dest, size_t len) {
	size_t start = keyb_buf_start;
	size_t index = 0;
	while (start != keyb_buf_end && index < len) {
		dest[index] = keyb_buf[start];
		start = (start + 1) % KEYB_BUF_LEN;
		index ++;
	}
	dest[index] = 0;
}

// Resets the CPU.
void cpu_reset() {
	// Flags.
	cpu.flagCout = false;
	cpu.flagZero = false;
	cpu.flagIRQ = false;
	cpu.flagNMI = false;
	cpu.flagHWI = false;
	cpu.wasHWI = false;
	// Busses.
	cpu.bus = 0;
	cpu.adrBus = 0;
	cpu.alo = 0;
	// Stage.
	cpu.stage = 0;
	cpu.mode = MODE_LOAD;
	cpu.schduledIRQ = 0;
	cpu.schduledNMI = 0;
	// Registers.
	cpu.regA = 0;
	cpu.regB = 0;
	cpu.regX = 0;
	cpu.regY = 0;
	cpu.regIR = 0;
	cpu.regPC = 0;
	cpu.regAR = 0;
	cpu.stackPtr = 0;
	cpu.regIRQ = 0;
	cpu.regNMI = 0;
	// Debugger.
	cpu.skipping = 0;
	cpu.skipDepth = 0;
	cpu.breakpoints = NULL;
	cpu.breakpointsLen = 0;
	cpu.debugIRQ = false;
	cpu.debugNMI = false;
	// Instruction set.
	cpu.isaRom = default_isa_rom;
	cpu.isaRomLen = DEFAULT_ISA_ROM_LEN;
	// Memory.
	cpu.ram = ram_reserve;
	memset(ram_reserve, 0, 65536);
	// Statistics.
	cpu.numCycles = 0;
	cpu.numInsns = 0;
	cpu.numSubs = 0;
}

// Handler for MMIO reading.
uint8_t gr8cpu_mmio_read(uint16_t address, bool notouchy) {
	if (address == 0xFEFC) {
		return keybbuf_read(notouchy);
	}
	return 0;
}

// Handler for MMIO writing.
void gr8cpu_mmio_write(uint16_t address, uint8_t value) {
	if (address == 0xFEFD) {
		if (value & 0x80) {
			char buf[6] = {0};
			utf_cat(buf, ibm437_table[value & 0x7f]);
			vtty_puts(buf);
		} else {
			vtty_putc(value);
		}
	}
}

// Handler for program exit.
void exithandler() {
	// Restore TTY to sane.
	system("stty sane");
}
