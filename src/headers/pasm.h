#include <stdio.h>
#define EXIT_ASM_ERROR 2
#define MAXLINES 0xFFFF
#define MAXLABELSIZE 0xFFF
#define __PASM_VERSION__ "0.1"

typedef struct {
    unsigned int address;
    char label[MAXLABELSIZE];
} Label;

Label lookup[MAXLINES];
size_t LOOKUP_PT = 0;

FILE *fpasm, *fpbin;
char *words[MAXLINES];
char* PROGNAME = NULL;

unsigned int linenum = 0;
