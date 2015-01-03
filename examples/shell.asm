; A small `shell'-like program
init:
    load    [X], @welcome
    print0

main:
    load    [X], @prompt
    print0
    load    [X], @user_input
    input
    jump    @check_cmd

check_cmd:
    load    r1, [X]
    ifeq    r1, #0
    jump    @main

    ; `help'
    load    r0, #4
    load    [X], @_help
    call    @check_string
    ifeq    r0, #0
    jump    @action_help

    ; `hello'
    load    r0, #5
    load    [X], @_hello
    call    @check_string
    ifeq    r0, #0
    jump    @action_hello

    ; `asm'
    load    r0, #3
    load    [X], @_asm
    call    @check_string
    ifeq    r0, #0
    jump    @action_asm

    ; `exit'
    load    r0, #4
    load    [X], @_exit
    call    @check_string
    ifeq    r0, #0
    jump    @end

    jump    @unknown_command

check_string:
    switchx #f
    load    [X], @user_input
    switchx #0

csloop: ifeq    r0, #0
    jump    @last_check

    load    r2, [X]

    switchx #f
    load    r3, [X]
    add     [X], #1

    switchx #0
    add     [X], #1

    ifneq   r2, r3
    ret

    sub     r0, #1
    jump    @csloop

last_check:
    switchx #f
    load    r2, [X]
    ifneq   r2, #0
    load    r0, #1

    ret

action_help:
    load    [X], @commands
    print0
    jump    @main

action_hello:
    load    [X], @helloworld
    print0
    jump    @main

action_asm:
    load    [X], @helloasm
    print0
    jump    @main

unknown_command:
    load    [X], @cmd_error
    print0
    load    [X], @user_input
    print0
    load    [X], @cmd_error_end
    print0
    jump    @main

end:
    halt

welcome:
    stringl "PShell v0.1, running on pvm"

prompt:
    string  "> "

commands:
    stringl "Commands: help, hello, asm, exit."

helloworld:
    stringl "Hello, World!"

helloasm:
    stringl "Hello, Assembly!"

cmd_error:
    string  "Unknown command: `"

cmd_error_end:
    stringl "'."

_help:
    string "help"

_hello:
    string "hello"

_asm:
    string "asm"

_exit:
    string "exit"

user_input:
    ; User input will be stored here
