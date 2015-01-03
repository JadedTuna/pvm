// P Virtual Machine - header file
#define MEMSIZE 65535
#define REGISTERS 16
#define DEBUG 0
#define __PVM_VERSION__ "0.1"

unsigned int  memory[MEMSIZE] = {0};
unsigned int  reg[REGISTERS] = {0};
unsigned char mode, halt, inst, x, y, k, psp=0;
unsigned int  pc, mmmm, nnn, exit_code = EXIT_SUCCESS;
unsigned long opcode;

unsigned int* X;
unsigned int pc_stack[0xFF] = {0};
unsigned int arrayX[0xF] = {0};
char* PROGNAME = NULL;

typedef char FLAG;
