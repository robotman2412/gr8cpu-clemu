
#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "common/GR8EMUr3_2.h"

#define INSN_JSR 0x02
#define INSN_RET 0x03

#define TICK_MODE_NORMAL (TICK_NORMAL << 16)
#define TICK_MODE_STEP_IN (TICK_STEP_IN << 16)
#define TICK_MODE_STEP_OVER ((TICK_STEP_OVER << 16) | (INSN_JSR << 8) | INSN_RET)
#define TICK_MODE_STEP_OUT ((TICK_STEP_OUT << 16) | (INSN_JSR << 8) | INSN_RET)

extern bool gr8cpu_running;
extern gr8cpurev3_t cpu;
extern size_t stats_mmio_r;
extern size_t stats_mmio_w;

#define KEYB_BUF_LEN 32
extern char keyb_buf[KEYB_BUF_LEN];
extern size_t keyb_buf_start;
extern size_t keyb_buf_end;

// Options.
#define EXEC_TYPE_RAW 0
#define EXEC_TYPE_LHF 1
#define EXEC_TYPE_ASM 2
#define EXEC_TYPE_ASMV 3

typedef struct options {
	char    *disk_file;
	char    *exec_file;
	uint8_t  exec_type;
	bool     run_immediately;
	bool     show_help;
} options_t;
extern options_t options;

// Frequency options.
#define N_FREQS 14

extern char freq_sel_desc[16];
extern char freq_real_desc[16];
extern uint32_t freq_delays[N_FREQS];
extern uint32_t freq_cycles[N_FREQS];

// Loads the file into an array and returns the size and pointer.
bool load_file(char *path, uint8_t **buf, size_t *len);

// Returns time with millisecond precision.
uint64_t millis();
// Returns time with microsecond precision.
uint64_t micros();
// Returns time with nanosecond precision.
uint64_t nanos();

// Describes a frequency as text.
void desc_freq(char *buf, double freq);

// Called when a character is added to the keyboard buffer.
bool keybbuf_add(char c);
// Called when a character is read from the keyboard buffer.
char keybbuf_read(bool notouchy);
// Creates a copy of the keyboard buffer of at most len characters.
// Buffer must be at least len+1 characters.
void keybbuf_copy(char *dest, size_t len);

// Resets the CPU.
void cpu_reset();
// Handler for MMIO reading.
uint8_t gr8cpu_mmio_read(uint16_t address, bool notouchy);
// Handler for MMIO writing.
void gr8cpu_mmio_write(uint16_t address, uint8_t value);

// Handler for program exit.
void exithandler();

#endif //MAIN_H
