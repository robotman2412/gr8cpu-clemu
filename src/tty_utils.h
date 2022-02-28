
#ifndef TTY_UTILS_H
#define TTY_UTILS_H

#include <stdbool.h>

// Ansi escape codes.
#define ANSI_RESET    "\033[0m"
#define ANSI_BOLD     "\033[1m"
#define ANSI_DIM      "\033[2m"
#define ANSI_INV      "\033[7m"
#define ANSI_CLRLN    "\033[0J"

// Common groups of escape codes
#define ANSI_BOLD_INV "\033[1;7m"

// To represent certain inputs to the GR8CPU keyboard.
#define PRE_SYM   ANSI_DIM
#define POST_SYM  ANSI_RESET
#define SYM_UP    PRE_SYM "↑" POST_SYM
#define SYM_DOWN  PRE_SYM "↓" POST_SYM
#define SYM_LEFT  PRE_SYM "←" POST_SYM
#define SYM_RIGHT PRE_SYM "→" POST_SYM
#define SYM_BKSP  PRE_SYM "⇤" POST_SYM
#define SYM_ENTER PRE_SYM "↩" POST_SYM
#define SYM_TAB   PRE_SYM "⇥" POST_SYM
#define SYM_DOT   PRE_SYM "·" POST_SYM

// To do some ASCII art.
#define UTF_CORNER_TL "╔"
#define UTF_CORNER_TR "╗"
#define UTF_CORNER_BL "╚"
#define UTF_CORNER_BR "╝"
#define UTF_PIPE_H    "═"
#define UTF_PIPE_V    "║"

// Handy list of all escape codes.
#define CTRL_A 0x01
#define CTRL_B 0x02
#define CTRL_C 0x03
#define CTRL_D 0x04
#define CTRL_E 0x05
#define CTRL_F 0x06
#define CTRL_G 0x07
#define CTRL_H 0x08
#define CTRL_I 0x09
#define CTRL_J 0x0A
#define CTRL_K 0x0B
#define CTRL_L 0x0C
#define CTRL_M 0x0D
#define CTRL_N 0x0E
#define CTRL_O 0x0F
#define CTRL_P 0x10
#define CTRL_Q 0x11
#define CTRL_R 0x12
#define CTRL_S 0x13
#define CTRL_T 0x14
#define CTRL_U 0x15
#define CTRL_V 0x16
#define CTRL_W 0x17
#define CTRL_X 0x18
#define CTRL_Y 0x19
#define CTRL_Z 0x1A
#define CTRL_LBRAC   0x1B
#define CTRL_BKSLASH 0x1C
#define CTRL_RBRAC   0x1D
#define CTRL_CARET   0x1E
#define CTRL_DASH    0x1F

#define ESC    0x1B

// GR8CPU direction key substitutions.
#define GR8CPU_UP    0x11
#define GR8CPU_DOWN  0x12
#define GR8CPU_LEFT  0x13
#define GR8CPU_RIGHT 0x14

// Control groups.
typedef struct ctrl_group {
    int len;
    int *map;
    char **desc;
} ctrl_group_t;

#define GROUP_LEN_KEYB 1
#define GROUP_LEN_RUN  4
#define GROUP_LEN_STOP 8
#define GROUP_LEN_SHOW 5
#define GROUPS_LEN 4

extern int group_map_keyb[GROUP_LEN_KEYB];
extern int group_map_run[GROUP_LEN_RUN];
extern int group_map_stop[GROUP_LEN_STOP];
extern int group_map_show[GROUP_LEN_SHOW];

extern char *group_desc_keyb[GROUP_LEN_KEYB];
extern char *group_desc_run[GROUP_LEN_RUN];
extern char *group_desc_stop[GROUP_LEN_STOP];
extern char *group_desc_show[GROUP_LEN_SHOW];

extern ctrl_group_t group_keyb;
extern ctrl_group_t group_run;
extern ctrl_group_t group_stop;
extern ctrl_group_t group_show;
extern ctrl_group_t *groups[GROUPS_LEN];

// State in relation to control groups.
extern int state;

#define STATE_KEYB 0
#define STATE_RUN  1
#define STATE_STOP 2
#define STATE_SHOW 3

// Stuff to show.
#define SHOW_INDEX_STAT  0
#define SHOW_INDEX_PC    1
#define SHOW_INDEX_REGS  2
#define SHOW_INDEX_SREGS 3
extern bool showing[4];

// Control mappings.
#define MAP_UNKEYB  ESC
#define MAP_BACK    ESC
#define MAP_KEYB    CTRL_K
#define MAP_PAUSE   CTRL_P
#define MAP_UNPAUSE CTRL_P
#define MAP_CSTEP       '1'
#define MAP_ISTEP       '2'
#define MAP_MSTEP       '3'
#define MAP_XSTEP       '4'
#define MAP_RESET   CTRL_R
#define MAP_SAVEST  CTRL_S
#define MAP_LOADST  CTRL_O
#define MAP_SHOW    CTRL_V
#define MAP_SHOW_STAT   '1'
#define MAP_SHOW_PC     '2'
#define MAP_SHOW_REGS   '3'
#define MAP_SHOW_SREGS  '4'

// Control descriptions
#define DESC_UNKEYB     "Exit keyboard"
#define DESC_BACK       "Back"
#define DESC_KEYB       "Use keyboard"
#define DESC_PAUSE      "Pause CPU"
#define DESC_UNPAUSE    "Unpause CPU"
#define DESC_CSTEP      "Single cycle"
#define DESC_ISTEP      "Step in"
#define DESC_MSTEP      "Step over"
#define DESC_XSTEP      "Step out"
#define DESC_RESET      "Reset"
#define DESC_SAVEST     "Save state"
#define DESC_LOADST     "Load state"
#define DESC_SHOW       "Show..."
#define DESC_SHOW_STAT  "Show statistics"
#define DESC_SHOW_PC    "Show PC"
#define DESC_SHOW_REGS  "Show regs"
#define DESC_SHOW_SREGS "Show sregs"

// Redraws the things to show, keyboard and control mappings.
void redraw();
// Describe a group of controls.
void ctrldesc(ctrl_group_t *group);
// Describe characters without the GR8CPU substitutions.
// Returns the number of characters used to represent c.
int chardesc(char c);
// Describe characters with the GR8CPU substitutions.
// Returns the number of characters used to represent c.
int chardesc_gr8cpu(char c);
// Calculate the TTY's size.
bool tty_getsize(int *x, int *y);
// Get the cursor position.
bool tty_getpos(int *x, int *y);
// Set the cursor position.
void tty_setpos(int x, int y);
// Print a character to the virtual TTY.
void vtty_putc(char val);
// Print a string to the virtual TTY.
void vtty_puts(char *val);

#endif //TTY_UTILS_H
