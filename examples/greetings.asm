init:
    load    [X], @question

main:
    print0
        
    load    [X], #100
    input
        
    load    [X], @say1
    print0
        
    load    [X], #100
    print0
        
    load    [X], @say2
    print0

    jump    @end

end:
    halt

question:
    string  "What is your name? "

say1:
    string  "Greetings, "

say2:
    stringl "!"
