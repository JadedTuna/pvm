#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "headers/pasm.h"

char *USAGE = 
"usage: pasm [-hv] file.asm [file.bin]\n"
"options:\n"
"   -h              print this help message\n"
"   -v              print version\n";

void print_usage() {
    fprintf(stderr, USAGE);
    exit(1);
}

void print_version() {
    printf("%s: pasm version %s\n", PROGNAME,
        __PASM_VERSION__);
    exit(EXIT_SUCCESS);
}

void expected(char* what, char* inst) {
    fprintf(stderr, "%s: *** LINE %i: EXPECTED %s AFTER `%s'\n",
            PROGNAME,
            linenum, what, inst);
    exit(EXIT_ASM_ERROR);
}

void argument_size(char* inst, char* size) {
    fprintf(stderr,
            "%s: *** LINE: %i: `%s' ARGUMENT "
            "CANNOT BE GREATER THAN %s\n", PROGNAME,
            linenum, inst, size);
    exit(EXIT_ASM_ERROR);
}

void label_not_found(char* label) {
    fprintf(stderr, "%s: *** LINE %i: LABEL %s NOT FOUND",
            PROGNAME,
            linenum, label);
    exit(EXIT_ASM_ERROR);
}

void inst_unknown(char* inst) {
    fprintf(stderr, "%s: *** LINE %i: UNKNOWN INSTRUCTION: `%s'\n",
            PROGNAME,
            linenum, inst);
    exit(EXIT_ASM_ERROR);
}

char* get_string(char* token) {
    int c, i;
    char do_count;
    size_t size = 0;
    size_t start = 0;
    size_t last_quote = 0;

    for (i=0;(c = token[i]) != '\0'; i++) {
        size++;
        if (c == '"') {
            if (do_count) {
                last_quote = size;
            }
            else {
                start = size;
                do_count = 1;
            }
        }
    }
    token += start;
    char* string = malloc(last_quote - start);
    strncpy(string, token, last_quote - start - 1);
    
    return string;
}

size_t quotelen(char* token) {
    return strlen(get_string(token));
}

unsigned int get_label_addr(char* token) {
    token++; // Skip @
    unsigned int addr = -1;
    size_t i;
    Label label;

    for (i=0;;i++) {
        label = lookup[i];
        if (!label.label) break;

        if (!strcmp(label.label, token)) {
            addr = label.address;
            break;
        }
    }

    return addr;
}

unsigned int char2hex(char c) {
    char string[2] = {c, '\0'};
    unsigned int val;
    sscanf(string, "%x", &val);

    return val;
}

unsigned int base16_decode(char* token) {
    token++; // Skip #
    unsigned int result = 0;
    for (;*token != '\0';token++) {
        result = result * 16 + char2hex(*token);
    }

    return result;
}

/* Read file line by line;
 * Generate labels' lookup table
 */
void pass1() {
    int index = 0;
    unsigned int i = 0;
    linenum = 0;

    char *line = NULL;
    char *linecp = malloc(0);
    char *comment = {0};

    size_t address = 0;
    size_t size = 0;
    size_t addr = 0;
    size_t toksize = 0;

    for (;;size=0, linenum++) {
        getline(&line, &size, fpasm);
        addr = strlen(line) - 1;

        if (!line || line[0] == '\0') // EOF
            break;

        if (line[addr] == '\n')
            line[addr] = '\0'; // Get rid of '\n'

        // Strip the comments
        comment = strchr(line, ';');
        if (comment) {
            index = (int)(comment - line);
            line[index] = '\0';
        }

        words[i++] = line;

        linecp = realloc(linecp, strlen(line) + 1);
        memset(linecp, '\0', strlen(line) + 1);
        strcpy(linecp, line);
        char* token = strtok(linecp, " \t");
        if (!token) continue;

        toksize = strlen(token);
        if (token[toksize - 1] == ':') {
            // Label
            token[strlen(token) - 1] = '\0';
            Label label;
            strcpy(label.label, token);
            label.address = address;

            lookup[LOOKUP_PT++] = label;

            token = strtok(NULL, " \t");
            if (token) address += 3;

        } else if (!strcmp(token, "string")) {
            // We insert a string here
            token = strtok(NULL, "\0");
            if (!token)
                expected("\"...\"", "string");

            size_t stringlen = quotelen(token);
            if (stringlen == -1)
                expected("\"...\"", "string");

            // extra byte needed for \0 char
            address += stringlen + 1;

        } else if (!strcmp(token, "stringn")) {
            // We insert a string here
            token = strtok(NULL, "\0");
            if (!token)
                expected("\"...\"", "stringn");

            size_t stringlen = quotelen(token);
            if (stringlen == -1)
                expected("\"...\"", "stringn");

            address += stringlen;

        } else if (!strcmp(token, "stringl")) {
            // We insert a string here
            token = strtok(NULL, "\0");
            if (!token)
                expected("\"...\"", "stringl");

            size_t stringlen = quotelen(token);
            if (stringlen == -1)
                expected("\"...\"", "stringl");

            // extra bytes needed for \n and \0 chars
            address += stringlen + 2;

        } else if (!strcmp(token, "fill")) {
            address += 6;

        } else if (!strcmp(token, "store")) {
            address += 6;

        } else if (!strcmp(token, "char")) {
            address += 1;

        } else {
            address += 3;
        }
    }
    free(linecp);
    free(line);
}

/* Parse assembly line by line;
 * Generate bytecode
 */
void pass2() {
    int i;
    size_t _i;
    size_t toksize;
    unsigned int label_addr;
    char *label = 0;
    char *string = 0;
    unsigned int linenum = 0;
    unsigned char x;

    char* line;
    char* token;

    unsigned char byte;

    for (_i=0;_i<MAXLINES;_i++,linenum++) {
        line = words[_i];
        if (!line) break;
        token = strtok(line, " \t");
        if (!token) continue;
        toksize = strlen(token);
        if (token[toksize - 1] == ':') {
            token = strtok(NULL, " \t");
            if (!token) continue;
        }

        /************************************************
                              halt
         ************************************************/


        if (!strcmp(token, "halt")) {
            // 0000
            token = strtok(NULL, " ,\t");
            if (token) {
                i = base16_decode(token);
                if (i > 0xFFFF)
                    argument_size("halt #", "#FFFF");
            }
            else i = 0x0000;
            fputc(0x00, fpbin);
            fputc(i >> 8, fpbin);
            fputc(i & 0xFF, fpbin);

        }

        /************************************************
                              load
         ************************************************/


        else if (!strcmp(token, "load")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                expected("rx OR [X]", "load");

            if (!strcmp(token, "[X]")) {
                // 5mmm
                token = strtok(NULL, " \t");
                if (!token)
                    expected("#NUM OR @LABEL", "load [X], ");

                switch (*token) {
                    case '@':
                        // Label address
                        label = malloc(strlen(token) + 1);
                        strcpy(label, token);

                        label_addr = get_label_addr(token);

                        if (label_addr == -1)
                            label_not_found(++label);

                        break;

                    case '#':
                        // A hex number
                        label_addr = base16_decode(token);
                        if (label_addr > 0xFFFF)
                            argument_size("load [X], #", "#FFFF");

                        break;

                    default:
                        expected("#ADDR OR @LABEL", "load [X], ");

                        break;
                }
                fputc(0x03, fpbin);
                fputc(label_addr >> 8, fpbin);
                fputc(label_addr & 0xFF, fpbin);

            } else if (*token == 'r') {
                // 1xnn
                x = token[1];
                if (!x)
                    expected("rx", "load r");

                // Make sure user input is correct
                byte = base16_decode(token);
                if (byte > 0xF)
                    argument_size("load r", "#F");

                token = strtok(NULL, " \t");

                if (!token)
                    expected("ry, #NUM OR [X]", "load rx, ");

                if (*token == '#') {
                    i = base16_decode(token);
                    if (i > 0xFFF)
                        argument_size("load rx, #", "#FFF");

                    fputc(0x01, fpbin);
                    byte <<= 4;
                    fputc(byte | (i >> 8), fpbin);
                    fputc(i & 0xFF, fpbin);

                } else if (*token == 'r') {
                    i = base16_decode(token);
                    if (i > 0xF)
                        argument_size("load rx, r", "#F");

                    fputc(0x10, fpbin);
                    byte <<= 4;
                    fputc(byte | i, fpbin);
                    fputc(0x04, fpbin);

                } else if (!strcmp(token, "[X]")) {
                    fputc(0x02, fpbin);
                    fputc(byte << 4, fpbin);
                    fputc(0x02, fpbin);

                }
            } else
                expected("rx OR [X]", "load");

        }

        /************************************************
                              fill
         ************************************************/


        else if (!strcmp(token, "fill")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                expected("rx", "fill");

            i = base16_decode(token);
            if (i > 0xF)
                argument_size("fill r", "#F");

            token = strtok(NULL, " \t");
            if (!token)
                expected("@LABEL", "fill rx, ");

            if (*token != '@')
                expected("@LABEL", "fill rx, ");

            label_addr = get_label_addr(token);
            if (label_addr == -1)
                label_not_found(++token);

            fputc(0x03, fpbin);
            fputc(label_addr >> 8, fpbin);
            fputc(label_addr & 0xFF, fpbin);

            fputc(0x02, fpbin);
            fputc(i << 4, fpbin);
            fputc(0x00, fpbin);

        }

        /************************************************
                              store
         ************************************************/


        else if (!strcmp(token, "store")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                expected("rx", "store");

            i = base16_decode(token);
            if (i > 0xF)
                argument_size("store r", "#F");

            token = strtok(NULL, " \t");
            if (!token)
                expected("@LABEL", "store rx, ");

            if (*token != '@')
                expected("@LABEL", "store rx, ");

            label_addr = get_label_addr(token);
            if (label_addr == -1)
                label_not_found(++token);

            fputc(0x03, fpbin);
            fputc(label_addr >> 8, fpbin);
            fputc(label_addr & 0xFF, fpbin);

            fputc(0x02, fpbin);
            fputc(i << 4, fpbin);
            fputc(0x01, fpbin);

        }

        /************************************************
                              memput
         ************************************************/


        else if (!strcmp(token, "memput")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                expected("rx", "memput");

            i = base16_decode(token);
            if (i > 0xF)
                argument_size("memput r", "#F");

            fputc(0x02, fpbin);
            fputc(i << 4, fpbin);
            fputc(0x03, fpbin);

        }

        /************************************************
                              jump
         ************************************************/


        else if (!strcmp(token, "jump")) {
            // 04mmmm
            token = strtok(NULL, " \t");
            if (!token)
                expected("#ADDR OR @LABEL", "jump");

            switch (*token) {
                case '#':
                    label_addr = base16_decode(token);
                    if (i > 0xFFFF)
                        argument_size("jump #", "#FFFF");

                    break;
                case '@':
                    label_addr = get_label_addr(token);
                    if (label_addr == -1)
                        label_not_found(++token);

                    break;
                default:
                    expected("#ADDR OR @LABEL", "jump");
                    break;
            }
            fputc(0x04, fpbin);
            fputc(label_addr >> 8, fpbin);
            fputc(label_addr & 0xFF, fpbin);

        }

        /************************************************
                              print0
         ************************************************/


        else if (!strcmp(token, "print0")) {
            // 05000
            token = strtok(NULL, " \t");
            if (token)
                expected("NOTHING", "print0");

            fputc(0x05, fpbin);
            fputc(0x00, fpbin);
            fputc(0x00, fpbin);

        }

        /************************************************
                              print
         ************************************************/


        else if (!strcmp(token, "print")) {
            // 05000
            token = strtok(NULL, " \t");
            if (!token)
                expected("#NUM", "print");

            if (*token != '#')
                expected("#NUM", "print");

            i = base16_decode(token);
            if (i > 0xFFF)
                argument_size("print #", "#FFF");

            fputc(0x05, fpbin);
            fputc(0x10 | (i >> 8), fpbin);
            fputc(i & 0xFF, fpbin);

        }

        /************************************************
                              printi
         ************************************************/


        else if (!strcmp(token, "printi")) {
            // 05000
            token = strtok(NULL, " \t");
            if (token)
                expected("NOTHING", "printi");

            fputc(0x05, fpbin);
            fputc(0x30, fpbin);
            fputc(0x00, fpbin);

        }

        /************************************************
                              putchar
         ************************************************/


        else if (!strcmp(token, "putchar")) {
            // 052nnn
            token = strtok(NULL, " \t");
            if (!token)
                expected("#NUM", "putchar");

            if (*token != '#')
                expected("#NUM", "putchar");

            i = base16_decode(token);
            if (i > 0xFF)
                argument_size("putchar #", "#FF");

            fputc(0x05, fpbin);
            fputc(0x20, fpbin);
            fputc(i, fpbin);

        }
        
        /************************************************
                              input
         ************************************************/


        else if (!strcmp(token, "input")) {
            // 060000
            token = strtok(NULL, " \t");
            if (token)
                expected("NOTHING", "input");

            fputc(0x06, fpbin);
            fputc(0x00, fpbin);
            fputc(0x00, fpbin);

        }
        
        /************************************************
                              ifeq
         ************************************************/


        else if (!strcmp(token, "ifeq")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                expected("rx", "ifeq");

            if (*token != 'r')
                expected("rx", "ifeq");

            x = token[1];
            if (!x)
                expected("rx", "ifeq");

            // Make sure user input is correct
            byte = base16_decode(token);
            if (byte > 0xF)
                argument_size("ifeq r", "#F");

            token = strtok(NULL, " \t");

            if (!token)
                expected("#NUM OR ry", "ifeq rx, ");

            switch (*token) {
                case '#':
                    i = base16_decode(token);
                    if (i > 0xFFF)
                        argument_size("ifeq rx, #", "#FFF");

                    fputc(0x08, fpbin);
                    byte <<= 4;
                    fputc(byte | (i >> 8), fpbin);
                    fputc(i & 0xFF, fpbin);
                    break;

                case 'r':
                    i = base16_decode(token);
                    if (i > 0xF)
                        argument_size("ifeq rx, r", "#F");

                    fputc(0x09, fpbin);
                    byte <<= 4;
                    fputc(byte | i, fpbin);
                    fputc(0x01, fpbin);
                    break;

                default:
                    expected("#NUM OR ry", "ifeq rx, ");
                    break;
            }

        }

        /************************************************
                              ifneq
         ************************************************/


        else if (!strcmp(token, "ifneq")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                expected("rx", "ifneq");

            if (*token != 'r')
                expected("rx", "ifneq");

            x = token[1];
            if (!x)
                expected("rx", "ifneq");

            // Make sure user input is correct
            byte = base16_decode(token);
            if (byte > 0xF)
                argument_size("ifneq #", "#F");

            token = strtok(NULL, " \t");

            if (!token)
                expected("#NUM OR ry", "ifneq rx, ");

            switch (*token) {
                case '#':
                    i = base16_decode(token);
                    if (i > 0xFFF)
                        argument_size("ifneq rx, #", "#FFF");

                    fputc(0x07, fpbin);
                    byte <<= 4;
                    fputc(byte | (i >> 8), fpbin);
                    fputc(i & 0xFF, fpbin);
                    break;

                case 'r':
                    i = base16_decode(token);
                    if (i > 0xF)
                        argument_size("ifneq rx, r", "#F");

                    fputc(0x09, fpbin);
                    byte <<= 4;
                    fputc(byte | i, fpbin);
                    fputc(0x00, fpbin);
                    break;

                default:
                    expected("#NUM OR ry", "ifneq rx, ");
                    break;
            }

        }

        /************************************************
                              add
         ************************************************/


        else if (!strcmp(token, "add")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                expected("rx or [X]", "add");

            if (*token == 'r') {
                i = base16_decode(token);
                if (i > 0xF)
                    argument_size("add r", "#F");

                token = strtok(NULL, " \t");
                if (!token)
                    expected("ry or #NUM", "add rx, ");

                byte = i;

                switch (*token) {
                    case '#':
                        i = base16_decode(token);
                        if (i > 0xFFF)
                            argument_size("add rx, #", "#FFF");

                        fputc(0x0C, fpbin);
                        byte <<= 4;
                        fputc(byte | (i >> 8), fpbin);
                        fputc(i & 0xFF, fpbin);
                        break;

                    case 'r':
                        i = base16_decode(token);
                        if (i > 0xF)
                            argument_size("add rx, r", "#F");

                        fputc(0x10, fpbin);
                        byte <<= 4;
                        fputc(byte | i, fpbin);
                        fputc(0x00, fpbin);
                        break;

                    default:
                        expected("ry or #NUM", "add rx, ");
                        break;
                }

            } else if (!strcmp(token, "[X]")) {
                token = strtok(NULL, " \t");
                if (!token)
                    expected("rx or #NUM", "add [X], ");

                if (*token == '#') {
                    i = base16_decode(token);
                    if (i > 0xFFFF)
                        argument_size("add [X], #", "#FFFF");

                    fputc(0x0A, fpbin);
                    fputc(i >> 8, fpbin);
                    fputc(i & 0xFF, fpbin);
                }
            } else
                expected("rx or [X]", "add");

        }
        
        /************************************************
                              sub
         ************************************************/


        else if (!strcmp(token, "sub")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                expected("rx or [X]", "sub");

            if (*token == 'r') {
                i = base16_decode(token);
                if (i > 0xF)
                    argument_size("sub r", "#F");

                token = strtok(NULL, " \t");
                if (!token)
                    expected("ry or #NUM", "sub rx, ");

                byte = i;

                switch (*token) {
                    case '#':
                        i = base16_decode(token);
                        if (i > 0xFFF)
                            argument_size("sub rx, #", "#FFF");

                        fputc(0x0D, fpbin);
                        byte <<= 4;
                        fputc(byte | (i >> 8), fpbin);
                        fputc(i & 0xFF, fpbin);
                        break;

                    case 'r':
                        i = base16_decode(token);
                        if (i > 0xF)
                            argument_size("sub rx, r", "#F");

                        fputc(0x10, fpbin);
                        byte <<= 4;
                        fputc(byte | i, fpbin);
                        fputc(0x01, fpbin);
                        break;

                    default:
                        expected("ry or #NUM", "sub rx, ");
                        break;
                }

            } else if (!strcmp(token, "[X]")) {
                token = strtok(NULL, " \t");
                if (!token)
                    expected("rx or #NUM", "sub [X], ");

                if (*token == '#') {
                    i = base16_decode(token);
                    if (i > 0xFFFF)
                        argument_size("sub [X], #", "#FFFF");
                    fputc(0x0B, fpbin);
                    fputc(i >> 8, fpbin);
                    fputc(i & 0xFF, fpbin);
                }
            } else
                expected("rx or [X]", "sub");

        }
        
        /************************************************
                              mul
         ************************************************/


        else if (!strcmp(token, "mul")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                expected("rx", "mul");

            if (*token == 'r') {
                i = base16_decode(token);
                if (i > 0xF)
                    argument_size("mul r", "#F");

                token = strtok(NULL, " \t");
                if (!token)
                    expected("ry OR #NUM", "mul rx, ");

                byte = i;

                switch (*token) {
                    case '#':
                        i = base16_decode(token);
                        if (i > 0xFFF)
                            argument_size("mul rx, #", "#FFF");

                        fputc(0x0E, fpbin);
                        byte <<= 4;
                        fputc(byte | (i >> 8), fpbin);
                        fputc(i & 0xFF, fpbin);
                        break;

                    case 'r':
                        i = base16_decode(token);
                        if (i > 0xF)
                            argument_size("mul rx, r", "#F");

                        fputc(0x10, fpbin);
                        byte <<= 4;
                        fputc(byte | i, fpbin);
                        fputc(0x02, fpbin);
                        break;

                    default:
                        expected("ry OR #NUM", "mul rx, ");
                        break;
                }

            } else
                expected("rx", "mul");

        }
        
        /************************************************
                              div
         ************************************************/


        else if (!strcmp(token, "div")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                expected("rx", "div");

            if (*token == 'r') {
                i = base16_decode(token);
                if (i > 0xF)
                    argument_size("div r", "#F");

                token = strtok(NULL, " \t");
                if (!token)
                    expected("ry OR #NUM", "div rx, ");

                byte = i;

                switch (*token) {
                    case '#':
                        i = base16_decode(token);
                        if (i > 0xFFF)
                            argument_size("div rx, #", "#FFF");

                        fputc(0x0F, fpbin);
                        byte <<= 4;
                        fputc(byte | (i >> 8), fpbin);
                        fputc(i & 0xFF, fpbin);
                        break;

                    case 'r':
                        i = base16_decode(token);
                        if (i > 0xF)
                            argument_size("div rx, r", "#F");

                        fputc(0x10, fpbin);
                        byte <<= 4;
                        fputc(byte | i, fpbin);
                        fputc(0x03, fpbin);
                        break;

                    default:
                        expected("ry OR #NUM", "div rx, ");
                        break;
                }

            } else
                expected("rx", "div");

        }
        
        /************************************************
                              call
         ************************************************/


        else if (!strcmp(token, "call")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                expected("@LABEL OR #ADDR", "call");

            switch (*token) {
                case '@':
                    label_addr = get_label_addr(token);
                    if (label_addr == -1)
                        label_not_found(++token);

                    break;
                case '#':
                    label_addr = base16_decode(token);
                    if (label_addr > 0xFFFF)
                        argument_size("call #", "#FFFF");
                    break;

                default:
                    expected("@LABEL OR #ADDR", "call");
                    break;
            }
            fputc(0x11, fpbin);
            fputc(label_addr >> 8, fpbin);
            fputc(label_addr & 0xFF, fpbin);

        }

        /************************************************
                              ret
         ************************************************/


        else if (!strcmp(token, "ret")) {
            token = strtok(NULL, " ,\t");
            if (token)
                expected("NOTHING", "ret");

            fputc(0x12, fpbin);
            fputc(0x00, fpbin);
            fputc(0x00, fpbin);

        }

        /************************************************
                             switchx
         ************************************************/


        else if (!strcmp(token, "switchx")) {
            token = strtok(NULL, " ,\t");
            if (!token || *token != '#')
                expected("#NUM", "switchx");

            i = base16_decode(token);
            if (i > 0xF)
                argument_size("switchx #", "#F");

            fputc(0x13, fpbin);
            fputc(0x00, fpbin);
            fputc(i, fpbin);

        }

        /************************************************
                              string
         ************************************************/


        else if (!strcmp(token, "string")) {
            // store string as a series of bytes
            token = strtok(NULL, "\0");
            string = get_string(token);
            for (;*string != '\0';string++)
                fputc(*string, fpbin);
            fputc('\0', fpbin); // Mark string's end

        }

        /************************************************
                              stringn
         ************************************************/


        else if (!strcmp(token, "stringn")) {
            // store string as a series of bytes
            // but don't put \0 at the end
            token = strtok(NULL, "\0");
            string = get_string(token);
            for (;*string != '\0';string++)
                fputc(*string, fpbin);

        }

        /************************************************
                              stringl
         ************************************************/


        else if (!strcmp(token, "stringl")) {
            // store string as a series of bytes
            token = strtok(NULL, "\0");
            string = get_string(token);
            for (;*string != '\0';string++)
                fputc(*string, fpbin);
            fputc('\n', fpbin); // New line
            fputc('\0', fpbin); // Mark string's end

        }

        /************************************************
                              char
         ************************************************/


        else if (!strcmp(token, "char")) {
            token = strtok(NULL, " \t");
            if (!token)
                expected("#NUM", "char");

            if (*token != '#')
                expected("#NUM", "char");

            i = base16_decode(token);
            if (i > 0xFF)
                argument_size("char #", "#FF");
            fputc(i, fpbin);

        }

        /************************************************
                          something else
         ************************************************/


        else
            inst_unknown(token);

    }
    free(label);
}

char* get_bin_name(char* input) {
    char* c = strrchr(input, '.');
    char* output;
    int index;

    if (!c) {
        index = strlen(input);
        output = malloc(strlen(input) + 2);

    }
    else {
        index = (int)(c - input);
        output = malloc(strlen(input) + 1);
    }

    strncpy(output, input, index + 1);
    if (!c) output[index] = '.';
    strcat(output, "bin");

    return output;
}

int main(int argc, char* argv[]) {
    PROGNAME = argv[0];
    char* fnasm = NULL;
    char* fnbin = NULL;
    int c;

    opterr = 0;

    while ((c = getopt(argc, argv, "hv")) != -1)
        switch (c) {
            case 'h':
                print_usage();
                break;
            case 'v':
                print_version();
                break;
            case '?':
                if (isprint(optopt))
                    fprintf(stderr,
                        "%s: unknown option: `-%c'.\n", PROGNAME,
                        optopt);
                else
                    fprintf(stderr,
                        "%s: unknown option character: `\\x%x'.\n",
                        PROGNAME,
                        optopt);
                return 1;
                break;
            default:
                abort();
        }

    argc -= optind;
    switch (argc) {
        case 1:
            fnasm = argv[optind];
            fnbin = get_bin_name(fnasm);
            break;
        case 2:
            fnasm = argv[optind++];
            fnbin = malloc(strlen(argv[optind]) + 1);
            fnbin = argv[optind];
            break;
        default:
            print_usage();
            break;
    }

    fpasm = fopen(fnasm, "r");
    if (!fpasm || ferror(fpasm)) {
        fprintf(stderr, "%s: failed to open "
            "file `%s' for reading.\n",
            PROGNAME,
            fnasm);
        return 1;
    }

    fpbin = fopen(fnbin, "wb");
    if (!fpbin || ferror(fpbin)) {
        fprintf(stderr, "%s: failed to open "
            "file `%s' for writing.\n",
            PROGNAME,
            fnbin);
        return 1;
    }

    pass1();
    pass2();

    fclose(fpasm);
    fclose(fpbin);

    free(fnbin);

    return 0;
}
