
#include "tty_utils.h"
#include "utf_utils.h"
#include "main.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

int group_map_keyb[GROUP_LEN_KEYB] = {
	MAP_UNKEYB
};
int group_map_run[GROUP_LEN_RUN] = {
	MAP_KEYB, MAP_PAUSE,
	MAP_RESET, MAP_SHOW
};
int group_map_stop[GROUP_LEN_STOP] = {
	MAP_KEYB, MAP_UNPAUSE,
	MAP_CSTEP, MAP_ISTEP, MAP_MSTEP, MAP_XSTEP,
	MAP_RESET, MAP_SHOW
};
int group_map_show[GROUP_LEN_SHOW] = {
	MAP_BACK, MAP_SHOW_STAT, MAP_SHOW_PC, MAP_SHOW_REGS, MAP_SHOW_SREGS
};

char *group_desc_keyb[GROUP_LEN_KEYB] = {
	DESC_UNKEYB
};
char *group_desc_run[GROUP_LEN_RUN] = {
	DESC_KEYB, DESC_PAUSE,
	DESC_RESET, DESC_SHOW
};
char *group_desc_stop[GROUP_LEN_STOP] = {
	DESC_KEYB, DESC_UNPAUSE,
	DESC_CSTEP, DESC_ISTEP, DESC_MSTEP, DESC_XSTEP,
	DESC_RESET, DESC_SHOW
};
char *group_desc_show[GROUP_LEN_SHOW] = {
	DESC_BACK, DESC_SHOW_STAT, DESC_SHOW_PC, DESC_SHOW_REGS, DESC_SHOW_SREGS
};

ctrl_group_t group_keyb = {
	.len = GROUP_LEN_KEYB,
	.map = group_map_keyb,
	.desc = group_desc_keyb
};
ctrl_group_t group_run = {
	.len = GROUP_LEN_RUN,
	.map = group_map_run,
	.desc = group_desc_run
};
ctrl_group_t group_stop = {
	.len = GROUP_LEN_STOP,
	.map = group_map_stop,
	.desc = group_desc_stop
};
ctrl_group_t group_show = {
	.len = GROUP_LEN_SHOW,
	.map = group_map_show,
	.desc = group_desc_show
};
ctrl_group_t *groups[GROUPS_LEN] = {
	&group_keyb,
	&group_run,
	&group_stop,
	&group_show
};

bool showing[4] = { 0, 1, 1, 0 };
static int keyb_x = 1;
static int vtty_x = 1;
static char vtty_lastchar;
static char vtty_buf[64];
static size_t vtty_buf_len = 0;

// Print the text part of the keyboard.
static void printkeyb() {
	int screen_width;
	if (!tty_getsize(&screen_width, NULL)) {
		screen_width = 60;
	}
	// Create a copy of the keyboard buffer.
	char copy_buf[KEYB_BUF_LEN + 1];
	keybbuf_copy(copy_buf, KEYB_BUF_LEN);
	// This is how wide in visible character the string is.
	size_t buf_width = 0;
	// Convert it to a formatted string.
	char *buf = malloc(1);
	*buf = 0;
	for (size_t i = 0; i < strlen(copy_buf); i++) {
		char c = copy_buf[i];
		char *cat = NULL;
		if (c == '\r' || c == '\n') {
			cat = SYM_ENTER;
		} else if (c == '\t') {
			cat = SYM_TAB;
		} else if (c == 0x7f) {
			cat = SYM_BKSP;
		} else if (c == GR8CPU_UP) {
			cat = SYM_UP;
		} else if (c == GR8CPU_DOWN) {
			cat = SYM_DOWN;
		} else if (c == GR8CPU_LEFT) {
			cat = SYM_LEFT;
		} else if (c == GR8CPU_RIGHT) {
			cat = SYM_RIGHT;
		} else if (c == ' ') {
			cat = SYM_DOT;
		} else if (c < ' ') {
			// No specific string for this.
			c |= 0x40;
			buf = realloc(buf, strlen(buf) + 11);
			strcat(buf, PRE_SYM "^");
			strncat(buf, &c, 1);
			strcat(buf, POST_SYM);
			buf_width ++;
		} else {
			// Just a character.
			buf = realloc(buf, strlen(buf) + 2);
			strncat(buf, &c, 1);
		}
		if (cat) {
			buf = realloc(buf, strlen(buf) + strlen(cat) + 1);
			strcat(buf, cat);
		}
		buf_width ++;
	}
	// We can now shorten the formatted string to fit.
	if (buf_width > screen_width - 3) {
		char *offs;
		size_t len;
		visible_substring(buf, buf_width - screen_width + 5, 0xFFFFFFFF, &offs, &len);
		fputs(ANSI_DIM "..." ANSI_RESET, stdout);
		fwrite(offs, 1, len, stdout);
		keyb_x = screen_width;
	} else {
		fputs(buf, stdout);
		int n = screen_width - 2 - buf_width;
		while (n--) {
			fputc(' ', stdout);
		}
		keyb_x = buf_width + 2;
	}
	free(buf);
}

// Redraws the things to show, keyboard and control mappings.
void redraw() {
	int width, height;
	if (tty_getsize(&width, &height)) {
		// Set TTY cursor position.
		tty_setpos(1, height - 5);
	} else {
		width = 60;
		// Redraw some of the GR8CPU output.
	}
	// Draw the keyboard.
	char buf[1024];
	sprintf(buf, "[%s / %s]", freq_real_desc, freq_sel_desc);
	fputs(UTF_CORNER_TL UTF_PIPE_H "Keyboard", stdout);
	for (int i = 10; i < width - utflen(buf) - 1; i++) fputs(UTF_PIPE_H, stdout);
	fputs(buf, stdout);
	fputs(UTF_CORNER_TR "\n" ANSI_CLRLN UTF_PIPE_V, stdout);
	// Keyboard contents.
	printkeyb();
	// More keyboard.
	fputs(UTF_PIPE_V "\n" UTF_CORNER_BL, stdout);
	// Show stuff.
	*buf = 0;
	if (showing[SHOW_INDEX_PC] || showing[SHOW_INDEX_REGS] || showing[SHOW_INDEX_SREGS]) {
		strcat(buf, "[");
	}
	if (showing[SHOW_INDEX_PC]) {
		sprintf(buf + 1, "PC:" ANSI_BOLD "%04x" ANSI_RESET, cpu.regPC);
		if (showing[SHOW_INDEX_REGS] || showing[SHOW_INDEX_SREGS]) {
			strcat(buf, " ");
		}
	}
	if (showing[SHOW_INDEX_REGS]) {
		sprintf(buf + strlen(buf), "A:" ANSI_BOLD "%02x" ANSI_RESET " B:" ANSI_BOLD "%02x"
				ANSI_RESET " X:" ANSI_BOLD "%02x" ANSI_RESET " Y:" ANSI_BOLD "%02x"
				ANSI_RESET " ST:" ANSI_BOLD "%04x" ANSI_RESET,
				cpu.regA, cpu.regB, cpu.regX, cpu.regY, cpu.stackPtr
		);
		if (showing[SHOW_INDEX_SREGS]) {
			strcat(buf, " ");
		}
	}
	if (showing[SHOW_INDEX_SREGS]) {
		sprintf(buf + strlen(buf), "IR:" ANSI_BOLD "%02x" ANSI_RESET " AR:" ANSI_BOLD "%04x"
				ANSI_RESET " NMI:" ANSI_BOLD "%04x" ANSI_RESET " IRQ:" ANSI_BOLD "%04x"
				ANSI_RESET " F:%02x" ANSI_RESET " CU:" ANSI_BOLD "%1x/%1x" ANSI_RESET,
				cpu.regIR, cpu.regAR, cpu.regNMI, cpu.regIRQ, gr8cpurev3_readflags(&cpu), cpu.mode, cpu.stage
		);
	}
	if (showing[SHOW_INDEX_PC] || showing[SHOW_INDEX_REGS] || showing[SHOW_INDEX_SREGS]) {
		strcat(buf, "]");
	}
	fputs(buf, stdout);
	for (int i = visiblelen(buf) + 1; i < width - 1; i++) fputs(UTF_PIPE_H, stdout);
	fputs(UTF_CORNER_BR "\n", stdout);
	if (showing[SHOW_INDEX_STAT]) {
		if (width >= 80)
		printf(" [MMIO R:" ANSI_BOLD "%9lu" ANSI_RESET "   MMIO W:" ANSI_BOLD "%9lu" ANSI_RESET
				"   CYC:" ANSI_BOLD "%9lu" ANSI_RESET "   INS:" ANSI_BOLD "%9lu" ANSI_RESET "   JSR:" ANSI_BOLD "%9lu" ANSI_RESET "]",
				stats_mmio_r, stats_mmio_w, cpu.numCycles, cpu.numInsns, cpu.numSubs
		);
	}
	putc('\n', stdout);
	// Draw the groups.
	ctrldesc(groups[state]);
	// Set the cursor position.
	if (state == STATE_KEYB) {
		tty_setpos(keyb_x, height - 4);
	} else {
		tty_setpos(vtty_x, height - 6);
	}
}

// Describe a group of controls.
void ctrldesc(ctrl_group_t *group) {
	// Get size.
	int width;
	if (!tty_getsize(&width, 0)) width = 60;
	// Space between the control thingies.
	int spacing = (width - 1) / ((group->len + 1) / 2) - 1;
	char buf[spacing + 1];
	memset(buf, ' ', spacing);
	buf[spacing] = 0;
	// Put initial space.
	fputs(ANSI_CLRLN " ", stdout);
	for (int i = 0; i < group->len; i += 2) {
		char *desc = group->desc[i];
		// Show mapping.
		printf(ANSI_BOLD_INV);
		int no = chardesc(group->map[i]);
		// Show name.
		fputs(ANSI_RESET " ", stdout);
		fputs(desc, stdout);
		// Add some spaces.
		fputs(buf + strlen(desc) + no, stdout);
	}
	// Next line.
	fputs("\n" ANSI_CLRLN " ", stdout);
	for (int i = 1; i < group->len; i += 2) {
		char *desc = group->desc[i];
		// Show mapping.
		printf(ANSI_BOLD_INV);
		int no = chardesc(group->map[i]);
		// Show name.
		fputs(ANSI_RESET " ", stdout);
		fputs(desc, stdout);
		// Add some spaces.
		fputs(buf + strlen(desc) + no, stdout);
	}
}

// Describe characters without the GR8CPU substitutions.
// Returns the number of characters used to represent c.
int chardesc(char c) {
	if (c < ' ') {
		fputc('^', stdout);
		fputc(c | 0x40, stdout);
		return 2;
	} else {
		fputc(c, stdout);
		return 1;
	}
}

// Describe characters with the GR8CPU substitutions.
// Returns the number of characters used to represent c.
int chardesc_gr8cpu(char c) {
	if (c == '\r' || c == '\n') {
		fputs(SYM_ENTER, stdout);
	} else if (c == '\t') {
		fputs(SYM_TAB, stdout);
	} else if (c == 0x7f) {
		fputs(SYM_BKSP, stdout);
	} else if (c == GR8CPU_UP) {
		fputs(SYM_UP, stdout);
	} else if (c == GR8CPU_DOWN) {
		fputs(SYM_DOWN, stdout);
	} else if (c == GR8CPU_LEFT) {
		fputs(SYM_LEFT, stdout);
	} else if (c == GR8CPU_RIGHT) {
		fputs(SYM_RIGHT, stdout);
	} else if (c == ' ') {
		fputs("\033[2m·\033[0m", stdout);
	} else if (c < ' ') {
		fputs(PRE_SYM "^", stdout);
		fputc(c | 0x40, stdout);
		fputs(POST_SYM, stdout);
		return 2;
	} else {
		fputc(c, stdout);
	}
	return 1;
}

// Calculate the TTY's size.
bool tty_getsize(int *x, int *y) {
	static time_t last_refresh = 0;
	static int last_x, last_y;
	// A bit of caching because you don't constantly change the TTY's size.
	if (last_refresh == time(NULL)) {
		if (x) *x = last_x;
		if (y) *y = last_y;
		return true;
	}
	// This to restore the position of the cursor.
	int x0, y0;
	if (tty_getpos(&x0, &y0)) {
		int x1, y1;
		// Set pos to something very big.
		fputs("\033[65535;65535H", stdout);
		// Get the actual pos which will be bottom right corner.
		bool res = tty_getpos(&x1, &y1);
		// Reset the cursor's position.
		tty_setpos(x0, y0);
		// Return the result.
		if (res) {
			// A bit of caching because you don't constantly change the TTY's size.
			last_refresh = time(NULL);
			last_x = x1;
			last_y = y1;
			// The result.
			if (x) *x = x1;
			if (y) *y = y1;
		}
		return res;
	}
	return false;
}

// Get the cursor position.
bool tty_getpos(int *x0, int *y0) {
	static bool has_failed = false;
	if (!has_failed) {
		// Send the cursor very far away and ask for it's position.
		fputs("\033[6n", stdout);
		// Wait for a response for at absolute most two seconds.
		time_t start = time(NULL);
		long pos = ftell(stdin);
		while (time(NULL) < start + 2) {
			char c = fgetc(stdin);
			if (c == 0x1B) {
				// We might have found it.
				int x = -1;
				int y = -1;
				fscanf(stdin, "[%d;%dR", &y, &x);
				if (x > 0 && y > 0) {
					// We found it.
					if (x0) *x0 = x;
					if (y0) *y0 = y;
					return true;
				}
			}
		}
		// Reset the position on fail.
		// fseek(stdin, pos, SEEK_SET);
	}
	// We didn't find it.
	return false;
}

// Set the cursor position.
void tty_setpos(int x, int y) {
	printf("\033[%d;%dH", y, x);
}

// Print a character to the virtual TTY.
void vtty_putc(char val) {
	int width, height;
	tty_getsize(&width, &height);
	if (vtty_buf_len) {
		vtty_buf[vtty_buf_len] = val;
		vtty_buf_len ++;
		if (vtty_buf_len > 2 && val != ';' && (val < '0' || val > '9')) {
			// Unbuffer the ANSI.
			tty_setpos(vtty_x, height - 6);
			fwrite(vtty_buf, 1, vtty_buf_len, stdout);
			vtty_buf_len = 0;
		}
	} else if (val == 0x1b) {
		// Buffer the ANSI.
		*vtty_buf = val;
		vtty_buf_len = 1;
	} else if (val == 0x7f || val == '\b') {
		if (vtty_x > 1) {
			vtty_x --;
			tty_setpos(vtty_x, height - 6);
			fputc(' ', stdout);
			tty_setpos(vtty_x, height - 6);
		}
	} else if (val == '\r' || val == '\n' && vtty_lastchar != '\r') {
		tty_setpos(width, height);
		fputc('\n', stdout);
		vtty_x = 1;
		tty_setpos(vtty_x, height - 6);
		fputs(ANSI_CLRLN, stdout);
		redraw();
	} else {
		if (vtty_x >= width) vtty_putc('\n');
		tty_setpos(vtty_x, height - 6);
		fputc(val, stdout);
		vtty_x ++;
	}
	vtty_lastchar = val;
}

// Print a string to the virtual TTY.
void vtty_puts(char *val) {
	while (*val) {
		vtty_putc(*val);
		val ++;
	}
}
