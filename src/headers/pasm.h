#include <stdio.h>
#define  MAXLINES 65535
#define __PASM_VERSION__ "0.1"

typedef struct {
    unsigned int address;
    char *label;
} Label;

Label lookup[MAXLINES];
size_t LOOKUP_PT = 0;

FILE *fpasm, *fpbin;
char *words[MAXLINES];

void die(char*);
void pass1();
void pass2();
