#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "headers/pasm.h"

char *USAGE = 
"usage: pasm [-hv] file.asm file.bin\n"
"options:\n"
"   -h              print this help message\n"
"   -v              print version\n";

void print_usage() {
    fprintf(stderr, USAGE);
    exit(1);
}

void print_version() {
    printf("pasm version %s\n", __PASM_VERSION__);
    exit(0);
}

void die(char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void assembly_error(char* msg, unsigned int linenum) {
    fprintf(stderr, "*** LINE %i: %s\n", linenum + 1, msg);
    exit(2);
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
    int index;
    unsigned int i = 0;
    char *line = NULL;
    char *linecp = malloc(1);
    unsigned int linenum = 0;
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
        linecp = strchr(line, ';');
        if (linecp) {
            index = (int)(linecp - line);
            line[index] = '\0';
        }

        words[i++] = line;

        linecp = malloc(strlen(line) + 1);
        strcpy(linecp, line);
        char* token = strtok(linecp, " \t");
        if (!token) continue;

        toksize = strlen(token);
        if (token[toksize - 1] == ':') {
            // Label
            token[strlen(token) - 1] = '\0';
            Label label;
            label.label = malloc(strlen(token) + 1);
            strcpy(label.label, token);
            label.address = address;

            lookup[LOOKUP_PT++] = label;

            token = strtok(NULL, " \t");
            if (token) address += 3;

        } else if (!strcmp(token, "string")) {
            // We insert a string here
            token = strtok(NULL, "\0");
            if (!token)
                assembly_error("EXPECTED \"...\" AFTER `string'",
                                linenum);
            size_t stringlen = quotelen(token);
            if (stringlen == -1)
                assembly_error("EXPECTED \"...\" AFTER `string'",
                                linenum);
            // extra byte needed for \0 char
            address += stringlen + 1;

        } else if (!strcmp(token, "stringn")) {
            // We insert a string here
            token = strtok(NULL, "\0");
            if (!token)
                assembly_error("EXPECTED \"...\" AFTER `stringn'",
                                linenum);
            size_t stringlen = quotelen(token);
            if (stringlen == -1)
                assembly_error("EXPECTED \"...\" AFTER `stringn'",
                                linenum);
            address += stringlen;

        } else if (!strcmp(token, "stringl")) {
            // We insert a string here
            token = strtok(NULL, "\0");
            if (!token)
                assembly_error("EXPECTED \"...\" AFTER `string'",
                                linenum);
            size_t stringlen = quotelen(token);
            if (stringlen == -1)
                assembly_error("EXPECTED \"...\" AFTER `string'",
                                linenum);
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
    char *errorstr = 0;
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
                    assembly_error("`halt #' ARGUMENT CANNOT "
                        "BE GREATER THAN #FFFF",
                        linenum);
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
                assembly_error("EXPECTED rx OR [X]"
                                " AFTER `load'", linenum);

            if (!strcmp(token, "[X]")) {
                // 5mmm
                token = strtok(NULL, " \t");
                if (!token) 
                    assembly_error("EXPECTED #NUM OR @LABEL "
                                    "AFTER `load [X], '", linenum);
                switch (*token) {
                    case '@':
                        // Label address
                        label = malloc(strlen(token) + 1);
                        strcpy(label, token);

                        label_addr = get_label_addr(token);

                        if (label_addr == -1) {
                            errorstr = malloc(strlen(token) + 17);
                            sprintf(errorstr,
                                "LABEL %s NOT FOUND",
                                ++label);
                            assembly_error(errorstr, linenum);
                        }

                        break;

                    case '#':
                        // A hex number
                        label_addr = base16_decode(token);
                        if (label_addr > 0xFFFF)
                            assembly_error("`load [X], #' ARGUMENT "
                                "CANNOT BE GREATER THAN #FFFF",
                                linenum);
                        break;

                    default:
                        assembly_error("EXPECTED #ADDR OR @LABEL "
                                     "AFTER `load [X], '", linenum);
                        break;
                }
                fputc(0x03, fpbin);
                fputc(label_addr >> 8, fpbin);
                fputc(label_addr & 0xFF, fpbin);

            } else if (*token == 'r') {
                // 1xnn
                x = token[1];
                if (!x)
                    assembly_error("EXPECTED rx AFTER `load r'",
                        linenum);

                // Make sure user input is correct
                byte = base16_decode(token);
                if (byte > 0xF)
                    assembly_error("`load r' ARGUMENT CANNOT"
                        " BE GREATER THAN #F", linenum);

                token = strtok(NULL, " \t");

                if (!token)
                    assembly_error("EXPECTED ry, #NUM OR "
                        "[X] AFTER `load rx, '",
                        linenum);

                if (*token == '#') {
                    i = base16_decode(token);
                    if (i > 0xFFF)
                        assembly_error("`load rx, #' ARGUMENT "
                            "CANNOT BE GREATER THAN #FFF",
                            linenum);
                    fputc(0x01, fpbin);
                    byte <<= 4;
                    fputc(byte | (i >> 8), fpbin);
                    fputc(i & 0xFF, fpbin);

                } else if (*token == 'r') {
                    i = base16_decode(token);
                    if (i > 0xF)
                        assembly_error("`load rx, r' ARGUMENT "
                            "CANNOT BE GREATER THAN #F",
                            linenum);

                    fputc(0x10, fpbin);
                    byte <<= 4;
                    fputc(byte | i, fpbin);
                    fputc(0x04, fpbin);

                } else if (!strcmp(token, "[X]")) {
                    fputc(0x02, fpbin);
                    fputc(byte << 4, fpbin);
                    fputc(0x02, fpbin);

                }
            }

        }

        /************************************************
                              fill
         ************************************************/


        else if (!strcmp(token, "fill")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                assembly_error("EXPECTED rx AFTER `fill'",
                    linenum);

            i = base16_decode(token);
            if (i > 0xF)
                assembly_error("`fill r' ARGUMENT CANNOT "
                    "BE GREATER THAN #F",
                    linenum);

            token = strtok(NULL, " \t");
            if (!token)
                assembly_error("EXPECTED @ADDR AFTER `fill rx, '",
                    linenum);

            if (*token != '@')
                assembly_error("EXPECTED @ADDR AFTER `fill rx, '",
                    linenum);

            label_addr = get_label_addr(token);
            if (label_addr == -1) {
                errorstr = malloc(strlen(token) + 17);
                sprintf(errorstr,
                    "LABEL %s NOT FOUND",
                    ++token);
                assembly_error(errorstr, linenum);
            }

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
                assembly_error("EXPECTED rx AFTER `store'",
                    linenum);

            i = base16_decode(token);
            if (i > 0xF)
                assembly_error("`store r' ARGUMENT CANNOT "
                    "BE GREATER THAN #F",
                    linenum);

            token = strtok(NULL, " \t");
            if (!token)
                assembly_error("EXPECTED @ADDR AFTER `store rx, '",
                    linenum);

            if (*token != '@')
                assembly_error("EXPECTED @ADDR AFTER `store rx, '",
                    linenum);

            label_addr = get_label_addr(token);
            if (label_addr == -1) {
                errorstr = malloc(strlen(token) + 17);
                sprintf(errorstr,
                    "LABEL %s NOT FOUND",
                    ++token);
                assembly_error(errorstr, linenum);
            }

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
                assembly_error("EXPECTED rx AFTER `memput'",
                    linenum);

            i = base16_decode(token);
            if (i > 0xF)
                assembly_error("`memput r' ARGUMENT CANNOT "
                    "BE GREATER THAN #F",
                    linenum);

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
                assembly_error("EXPECTED #ADDR OR "
                    "@LABEL AFTER `jump'", linenum);
            switch (*token) {
                case '#':
                    label_addr = base16_decode(token);
                    if (i > 0xFFFF)
                        assembly_error("`jump #' ARGUMENT CANNOT "
                            "BE GREATER THAN #FFFF",
                            linenum);
                    break;
                case '@':
                    label_addr = get_label_addr(token);
                    if (label_addr == -1) {
                        errorstr = malloc(strlen(token) + 17);
                        sprintf(errorstr,
                            "LABEL %s NOT FOUND",
                            ++token);
                        assembly_error(errorstr, linenum);
                    }
                    break;
                default:
                    assembly_error("EXPECTED #ADDR OR "
                        "@LABEL AFTER `jump'", linenum);
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
                assembly_error("INSTRUCTION `print0' "
                    "TAKES NO ARGUMENTS", linenum);
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
                assembly_error("EXPECTED #NUM AFTER `print'",
                    linenum);

            if (*token != '#')
                assembly_error("EXPECTED #NUM AFTER `print'",
                    linenum);

            i = base16_decode(token);
            if (i > 0xFFF)
                assembly_error("`print #' ARGUMENT "
                    "CANNOT BE GREATER THAN #FFF",
                    linenum);

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
                assembly_error("INSTRUCTION `printi' "
                    "TAKES NO ARGUMENTS", linenum);
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
                assembly_error("EXPECTED #NUM AFTER `putchar'",
                    linenum);

            if (*token != '#')
                assembly_error("EXPECTED #NUM AFTER `putchar'",
                    linenum);
            i = base16_decode(token);
            if (i > 0xFF)
                assembly_error("`putchar #' ARGUMENT CANNOT "
                    "BE GREATER THAN #FF", linenum);
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
                assembly_error("INSTRUCTION `input' "
                    "TAKES NO ARGUMENTS", linenum);
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
                assembly_error("EXPECTED rx AFTER `ifeq'",
                    linenum);

            x = token[1];
            if (!x)
                assembly_error("EXPECTED rx AFTER `ifeq'",
                    linenum);

            // Make sure user input is correct
            byte = base16_decode(token);
            if (byte > 0xF)
                assembly_error("`ifeq r' ARGUMENT CANNOT"
                    " BE GREATER THAN #F", linenum);

            token = strtok(NULL, " \t");

            if (!token)
                assembly_error("EXPECTED #NUM OR "
                    "ry AFTER `ifeq rx, '",
                    linenum);

            switch (*token) {
                case '#':
                    i = base16_decode(token);
                    if (i > 0xFFF)
                        assembly_error("`ifeq rx, #' ARGUMENT "
                                    "CANNOT BE GREATER THAN #FFF",
                                    linenum);
                    fputc(0x08, fpbin);
                    byte <<= 4;
                    fputc(byte | (i >> 8), fpbin);
                    fputc(i & 0xFF, fpbin);
                    break;

                case 'r':
                    i = base16_decode(token);
                    if (i > 0xF)
                        assembly_error("`ifeq rx, r' ARGUMENT "
                                    "CANNOT BE GREATER THAN #F",
                                    linenum);
                    fputc(0x09, fpbin);
                    byte <<= 4;
                    fputc(byte | i, fpbin);
                    fputc(0x01, fpbin);
                    break;

                default:
                    assembly_error("EXPECTED #NUM OR "
                        "ry AFTER `ifeq rx, '",
                        linenum);
                    break;
            }

        }

        /************************************************
                              ifneq
         ************************************************/


        else if (!strcmp(token, "ifneq")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                assembly_error("EXPECTED rx AFTER `ifneq'",
                    linenum);

            x = token[1];
            if (!x)
                assembly_error("EXPECTED rx AFTER `ifneq'",
                    linenum);

            // Make sure user input is correct
            byte = base16_decode(token);
            if (byte > 0xF)
                assembly_error("`ifneq r' ARGUMENT CANNOT"
                    " BE GREATER THAN #F", linenum);

            token = strtok(NULL, " \t");

            if (!token)
                assembly_error("EXPECTED #NUM OR "
                    "ry AFTER `ifneq rx, '",
                    linenum);

            switch (*token) {
                case '#':
                    i = base16_decode(token);
                    if (i > 0xFFF)
                        assembly_error("`ifneq rx, #' ARGUMENT "
                                    "CANNOT BE GREATER THAN #FFF",
                                    linenum);
                    fputc(0x07, fpbin);
                    byte <<= 4;
                    fputc(byte | (i >> 8), fpbin);
                    fputc(i & 0xFF, fpbin);
                    break;

                case 'r':
                    i = base16_decode(token);
                    if (i > 0xF)
                        assembly_error("`ifneq rx, r' ARGUMENT "
                                    "CANNOT BE GREATER THAN #F",
                                    linenum);
                    fputc(0x09, fpbin);
                    byte <<= 4;
                    fputc(byte | i, fpbin);
                    fputc(0x00, fpbin);
                    break;

                default:
                    assembly_error("EXPECTED #NUM OR "
                        "ry AFTER `ifneq rx, '",
                        linenum);
                    break;
            }

        }

        /************************************************
                              add
         ************************************************/


        else if (!strcmp(token, "add")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                assembly_error("EXPECTED rx OR [X] "
                                "AFTER `add'", linenum);

            if (*token == 'r') {
                i = base16_decode(token);
                if (i > 0xF)
                    assembly_error("`add r' ARGUMENT CANNOT "
                        "BE GREATER THAN #F", linenum);

                token = strtok(NULL, " \t");
                if (!token)
                    assembly_error("EXPECTED ry OR #NUM AFTER "
                        "`add rx, '",
                        linenum);

                byte = i;

                switch (*token) {
                    case '#':
                        i = base16_decode(token);
                        if (i > 0xFFF)
                            assembly_error("`add rx, #' ARGUMENT "
                                "CANNOT BE GREATER THAN #FFF",
                                linenum);

                        fputc(0x0C, fpbin);
                        byte <<= 4;
                        fputc(byte | (i >> 8), fpbin);
                        fputc(i & 0xFF, fpbin);
                        break;

                    case 'r':
                        i = base16_decode(token);
                        if (i > 0xF)
                            assembly_error("`add rx, r' ARGUMENT "
                                "CANNOT BE GREATER THAN #F",
                                linenum);

                        fputc(0x10, fpbin);
                        byte <<= 4;
                        fputc(byte | i, fpbin);
                        fputc(0x00, fpbin);
                        break;

                    default:
                        assembly_error("EXPECTED ry OR #NUM "
                            "AFTER `add rx, '", linenum);
                        break;
                }

            } else if (!strcmp(token, "[X]")) {
                token = strtok(NULL, " \t");
                if (!token)
                    assembly_error("EXPECTED rx OR #NUM "
                                    "AFTER `add [X], '", linenum);
                if (*token == '#') {
                    i = base16_decode(token);
                    if (i > 0xFFFF)
                        assembly_error("`add [X], #' ARGUMENT CANNOT "
                            "BE GREATER THAN #FFFF", linenum);
                    fputc(0x0A, fpbin);
                    fputc(i >> 8, fpbin);
                    fputc(i & 0xFF, fpbin);
                }
            }

        }
        
        /************************************************
                              sub
         ************************************************/


        else if (!strcmp(token, "sub")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                assembly_error("EXPECTED rx OR [X] "
                                "AFTER `sub'", linenum);

            if (*token == 'r') {
                i = base16_decode(token);
                if (i > 0xF)
                    assembly_error("`sub r' ARGUMENT CANNOT "
                        "BE GREATER THAN #F", linenum);

                token = strtok(NULL, " \t");
                if (!token)
                    assembly_error("EXPECTED ry OR #NUM AFTER "
                        "`sub rx, '",
                        linenum);

                byte = i;

                switch (*token) {
                    case '#':
                        i = base16_decode(token);
                        if (i > 0xFFF)
                            assembly_error("`sub rx, #' ARGUMENT "
                                "CANNOT BE GREATER THAN #FFF",
                                linenum);

                        fputc(0x0D, fpbin);
                        byte <<= 4;
                        fputc(byte | (i >> 8), fpbin);
                        fputc(i & 0xFF, fpbin);
                        break;

                    case 'r':
                        i = base16_decode(token);
                        if (i > 0xF)
                            assembly_error("`sub rx, r' ARGUMENT "
                                "CANNOT BE GREATER THAN #F",
                                linenum);

                        fputc(0x10, fpbin);
                        byte <<= 4;
                        fputc(byte | i, fpbin);
                        fputc(0x01, fpbin);
                        break;

                    default:
                        assembly_error("EXPECTED ry OR #NUM "
                            "AFTER `sub rx, '", linenum);
                        break;
                }

            } else if (!strcmp(token, "[X]")) {
                token = strtok(NULL, " \t");
                if (!token)
                    assembly_error("EXPECTED rx OR #NUM "
                                    "AFTER `sub [X], '", linenum);
                if (*token == '#') {
                    i = base16_decode(token);
                    if (i > 0xFFFF)
                        assembly_error("`sub [X], ' ARGUMENT CANNOT "
                            "BE GREATER THAN #FFFF", linenum);
                    fputc(0x0B, fpbin);
                    fputc(i >> 8, fpbin);
                    fputc(i & 0xFF, fpbin);
                }
            }

        }
        
        /************************************************
                              mul
         ************************************************/


        else if (!strcmp(token, "mul")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                assembly_error("EXPECTED rx "
                                "AFTER `mul'", linenum);

            if (*token == 'r') {
                i = base16_decode(token);
                if (i > 0xF)
                    assembly_error("`mul r' ARGUMENT CANNOT "
                        "BE GREATER THAN #F", linenum);

                token = strtok(NULL, " \t");
                if (!token)
                    assembly_error("EXPECTED ry OR #NUM AFTER "
                        "`mul rx, '",
                        linenum);

                byte = i;

                switch (*token) {
                    case '#':
                        i = base16_decode(token);
                        if (i > 0xFFF)
                            assembly_error("`mul rx, #' ARGUMENT "
                                "CANNOT BE GREATER THAN #FFF",
                                linenum);

                        fputc(0x0E, fpbin);
                        byte <<= 4;
                        fputc(byte | (i >> 8), fpbin);
                        fputc(i & 0xFF, fpbin);
                        break;

                    case 'r':
                        i = base16_decode(token);
                        if (i > 0xF)
                            assembly_error("`mul rx, r' ARGUMENT "
                                "CANNOT BE GREATER THAN #F",
                                linenum);

                        fputc(0x10, fpbin);
                        byte <<= 4;
                        fputc(byte | i, fpbin);
                        fputc(0x02, fpbin);
                        break;

                    default:
                        assembly_error("EXPECTED ry OR #NUM "
                            "AFTER `mul rx, '", linenum);
                        break;
                }

            }

        }
        
        /************************************************
                              div
         ************************************************/


        else if (!strcmp(token, "div")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                assembly_error("EXPECTED rx "
                                "AFTER `div'", linenum);

            if (*token == 'r') {
                i = base16_decode(token);
                if (i > 0xF)
                    assembly_error("`div r' ARGUMENT CANNOT "
                        "BE GREATER THAN #F", linenum);

                token = strtok(NULL, " \t");
                if (!token)
                    assembly_error("EXPECTED ry OR #NUM AFTER "
                        "`div rx, '",
                        linenum);

                byte = i;

                switch (*token) {
                    case '#':
                        i = base16_decode(token);
                        if (i > 0xFFF)
                            assembly_error("`div rx, #' ARGUMENT "
                                "CANNOT BE GREATER THAN #FFF",
                                linenum);

                        fputc(0x0F, fpbin);
                        byte <<= 4;
                        fputc(byte | (i >> 8), fpbin);
                        fputc(i & 0xFF, fpbin);
                        break;

                    case 'r':
                        i = base16_decode(token);
                        if (i > 0xF)
                            assembly_error("`div rx, r' ARGUMENT "
                                "CANNOT BE GREATER THAN #F",
                                linenum);

                        fputc(0x10, fpbin);
                        byte <<= 4;
                        fputc(byte | i, fpbin);
                        fputc(0x03, fpbin);
                        break;

                    default:
                        assembly_error("EXPECTED ry OR #NUM "
                            "AFTER `div rx, '", linenum);
                        break;
                }

            }

        }
        
        /************************************************
                              call
         ************************************************/


        else if (!strcmp(token, "call")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                assembly_error("EXPECTED @LABEL OR #ADDR "
                                "AFTER `call'", linenum);

            switch (*token) {
                case '@':
                    label_addr = get_label_addr(token);
                    if (label_addr == -1) {
                    fprintf(stderr, "*** LINE %i: "
                        "LABEL %s NOT FOUND\n",
                        linenum + 1, ++token);
                    exit(2);
                    }
                    break;
                case '#':
                    label_addr = base16_decode(token);
                    if (label_addr > 0xFFFF)
                        assembly_error("`call' ARGUMENT "
                            "CANNOT BE GREATER THAN #FFFF",
                            linenum);
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
                assembly_error("`ret' TAKES NO ARGUMENTS",
                    linenum);

            fputc(0x12, fpbin);
            fputc(0x00, fpbin);
            fputc(0x00, fpbin);

        }

        /************************************************
                             switchx
         ************************************************/


        else if (!strcmp(token, "switchx")) {
            token = strtok(NULL, " ,\t");
            if (!token)
                assembly_error("EXPECTED #NUM AFTER `switchx`",
                    linenum);

            i = base16_decode(token);
            if (i > 0xF)
                assembly_error("`switchx' ARGUMENT "
                    "CANNOT BE GREATER THAN #F",
                    linenum);

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
                assembly_error("EXPECTED #NUM AFTER `char'",
                    linenum);

            if (*token != '#')
                assembly_error("EXPECTED #NUM AFTER `char'",
                    linenum);
            i = base16_decode(token);
            if (i > 0xFF)
                assembly_error("`char #' ARGUMENT CANNOT "
                    "BE GREATER THAN #FF", linenum);
            fputc(i, fpbin);

        }

        /************************************************
                          something else
         ************************************************/


        else {
            errorstr = malloc(strlen(token) + 24);
            sprintf(errorstr,
                "UNKNOWN INSTRUCTION: `%s'",
                token);
            assembly_error(errorstr, linenum);
        }
    }
    free(label);
    free(errorstr);
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
            argv[0],
            fnasm);
        return 1;
    }

    fpbin = fopen(fnbin, "wb");
    if (!fpbin || ferror(fpbin)) {
        fprintf(stderr, "%s: failed to open "
            "file `%s' for writing.\n",
            argv[0],
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
