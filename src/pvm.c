// Pythonista Virtual Machine - source file
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <stdio.h>
#include "headers/pvm.h"

char *USAGE = 
"usage: pvm [-hv] file.bin\n"
"options:\n"
"   -h              print this help message\n"
"   -v              print version\n";

void print_usage() {
    fprintf(stderr, USAGE);
    exit(1);
}

void print_version() {
    printf("pvm version %s\n", __PVM_VERSION__);
    exit(0);
}

char load(FILE* fp) {
    int c;
    unsigned int i;
    for (i=0; (c = fgetc(fp)) != EOF; i++) {
        if (i >= MEMSIZE) return 1;
        memory[i] = c;
    }

    return 0;
}

/* read line, return size */
size_t readline(char line[], size_t size) {
    size_t i;
    int c;
    for (i=0; (c = getchar()) != EOF &&
                c != '\n' && i < size; i++)
        line[i] = c;

    line[i] = '\0';
    return i;
}

void debug(FLAG dflag, FLAG mflag) {
    if (dflag) {
        unsigned char i;
        for (i=0; i<=0xF; i++) printf("0x%X --> %i\n", i, reg[i]);
        printf("\nX: %i, %i\n", (int)(X - arrayX), *X);
        printf("pc: %i\n", pc);
        if (mflag) printf("\n");
    }

    if (mflag) {
        int j;
        for (j=0; j<MEMSIZE; j++) {
            if (memory[j] != 0)
                printf("0x%04X: 0x%02X\n", j, memory[j]);
        }
    }
}

void ctrl_c(int x) {
    printf("\n");
    halt = 1;
    exit_code = 0;
}

void execute(FLAG iflag) {
    unsigned char i;
    unsigned int j;
    size_t linesize;

    char line[MEMSIZE] = {0};
    X = &arrayX[0];

    for (pc=0; halt != 1 && pc < MEMSIZE;) {
        // Getting opcode
        opcode = memory[pc++];
        opcode <<= 8;
        opcode |= memory[pc++];
        opcode <<= 8;
        opcode |= memory[pc++];

        inst = opcode >> 16;
        x    = (opcode >> 12) & 0xF;
        y    = (opcode >>  8) & 0xF;
        mmmm = opcode & 0xFFFF;
        nnn  = opcode & 0xFFF;
        k    = opcode & 0xF;

        if (iflag)
            printf("@%04X: 0x%06lX\n", pc - 3, opcode);

        switch (inst) {
            case 0x0:
                // 00mmmm
                // halt
                halt = 1;
                exit_code = mmmm;
                break;
            case 0x1:
                // 01xnnn
                // rx = nnn
                reg[x] = nnn;
                reg[x] &= 0xFFF;
                break;
            case 0x2:
                // 02x00k
                switch (k) {
                    case 0x0:
                        // fill r0 to rx with values from memory
                        // starting at address [X]
                        for (i=0; i<=x; i++) {
                            reg[i] = memory[*X + i];
                            reg[i] &= 0xFFF;
                        }
                        break;

                    case 0x1:
                        // stores r0 to rx in memory starting
                        // at address [X]
                        for (i=0; i<=x; i++) {
                            reg[i] &= 0xFFF;
                            memory[*X + i] = reg[i];
                        }
                        break;
                        break;

                    case 0x2:
                        // load value from address [X] into
                        // register x
                        reg[x] = memory[*X];
                        reg[x] &= 0xFFF;
                        break;

                    case 0x3:
                        // store rx into memory address [X]
                        reg[x] &= 0xFFF;
                        memory[*X] = reg[x];
                        break;

                    default:
                        fprintf(stderr,
                            "unknown opcode at @%04X: 0x%06lX\n",
                            pc - 3, opcode);
                        return;
                        break;
                }
                break;
            case 0x3:
                // 03mmmm
                // load mmmm into [X]
                *X = mmmm;
                *X &= 0xFFFF;
                break;
            case 0x4:
                // 04mmmm
                // jump to address mmmm
                pc = mmmm;
                break;
            case 0x5:
                // 05xnnn
                switch (x) {
                    case 0x0:
                        // print values from address [X]
                        // until 0x0 is found
                        memset(line, '\0', MEMSIZE);
                        for (j=0; j + *X<MEMSIZE &&
                                memory[j + *X] != 0x0; j++)
                            line[j] = memory[j + *X];
                        printf("%s", line);
                        break;
                    case 0x1:
                        // print nnn values from address [X]
                        memset(line, '\0', MEMSIZE);
                        for (j=0; j + *X < MEMSIZE && j < nnn; j++)
                            line[j] = memory[j + *X];
                        printf("%s", line);
                        break;
                    case 0x2:
                        // print one character
                        putchar(nnn & 0xFF);
                        break;
                    case 0x3:
                        // print one integer from address [X]
                        printf("%i", memory[*X]);
                        break;
                }
                break;
            case 0x6:
                // 060000
                // get input from user and store it at address [X]
                linesize = readline(line, MEMSIZE);
                for (j=0; j <= linesize; j++) {
                    memory[j + *X] = line[j];
                }
                break;
            case 0x7:
                // 07xnnn
                // skip next opcode if rx == nnn
                if (reg[x] == nnn) pc += 3;
                break;
            case 0x8:
                // 08xnnn
                // skip next opcode if rx != nnn
                if (reg[x] != nnn) pc += 3;
                break;
            case 0x9:
                // 09xy0k
                switch (k) {
                    case 0x0:
                        // skip next opcode if rx == ry
                        if (reg[x] == reg[y]) pc += 3;
                        break;
                    case 0x1:
                        // skip next opcode if rx != ry
                        if (reg[x] != reg[y]) pc += 3;
                        break;
                }
                break;
            case 0xA:
                // 0Ammmm
                // add mmmm to [X]
                *X += mmmm;
                *X &= 0xFFFF;
                break;
            case 0xB:
                // 0Bmmmm
                // sub mmmm from [X]
                *X -= mmmm;
                *X &= 0xFFFF;
                break;
            case 0xC:
                // 0Cxnnn
                // add nnn to rx
                reg[x] += nnn;
                reg[x] &= 0xFFF;
                break;
            case 0xD:
                // 0Dxnnn
                // sub nnn from rx
                reg[x] -= nnn;
                reg[x] &= 0xFFF;
                break;
            case 0xE:
                // 0Exnnn
                // mul rx by nnn
                reg[x] *= nnn;
                reg[x] &= 0xFFF;
                break;
            case 0xF:
                // 0Fxnnn
                // div rx by nnn
                reg[x] /= nnn;
                reg[x] &= 0xFFF;
                break;
            case 0x10:
                // 10xy0k
                switch (k) {
                    case 0x0:
                        // add ry to rx
                        reg[x] += reg[y];
                        reg[x] &= 0xFFF;
                        break;
                    case 0x1:
                        // sub ry from rx
                        reg[x] -= reg[y];
                        reg[x] &= 0xFFF;
                        break;
                    case 0x2:
                        // mul rx by ry
                        reg[x] *= reg[y];
                        reg[x] &= 0xFFF;
                        break;
                    case 0x3:
                        // div rx by ry
                        reg[x] /= reg[y];
                        reg[x] &= 0xFFF;
                        break;
                    case 0x4:
                        // rx = ry
                        reg[y] &= 0xFF;
                        reg[x] = reg[y];
                        break;
                    default:
                        fprintf(stderr,
                            "unknown opcode at @%04X: 0x%06lX\n",
                            pc - 3, opcode);
                        return;
                        break;
                }
                break;
            case 0x11:
                // 11mmmm
                // call subroutine at address mmmm
                pc_stack[psp++] = pc;
                pc = mmmm;
                break;
            case 0x12:
                // 120000
                // return from a subroutine
                pc = pc_stack[--psp];
                break;
            case 0x13:
                // 13000k
                // switch X to &arrayX[k]
                X = &arrayX[k & 0xF];
                break;
            default:
                fprintf(stderr,
                    "unknown opcode at @%04X: 0x%06lX\n",
                    pc - 3, opcode);
                return;
                break;
        }
    }
}

int main(int argc, char* argv[]) {
    FLAG  dflag = 0;
    FLAG  mflag = 0;
    FLAG  iflag = 0;
    char* fn = NULL;
    int c;

    opterr = 0;

    while ((c = getopt(argc, argv, "hdmvi")) != -1)
        switch (c) {
            case 'h':
                print_usage();
                break;
            case 'd':
                dflag = 1;
                break;
            case 'm':
                mflag = 1;
                break;
            case 'v':
                print_version();
                break;
            case 'i':
                iflag = 1;
                break;
            case '?':
                if (isprint(optopt))
                    fprintf(stderr,
                        "Unknown option: `-%c'.\n",
                        optopt);
                else
                    fprintf(stderr,
                        "Unknown option character: `\\x%x'.\n",
                        optopt);
                return 1;
                break;
            default:
                abort();
        }

    argc -= optind;
    switch (argc) {
        case 1:
            fn = argv[optind];
            break;
        default:
            print_usage();
            break;
    }

    FILE* fp = fopen(fn, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open file: `%s'.\n", fn);
        return 1;
    }
    if (load(fp)) {
        fprintf(stderr, "Memory overflow (file too big).\n");
        return 1;
    }
    fclose(fp);

    signal(SIGINT, ctrl_c);
    execute(iflag);

    debug(dflag, mflag);

    exit(exit_code);
}
