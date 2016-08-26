CFLAGS+=-Wall -Wextra
PVM=pvm
PASM=pasm

help:
	@echo -e "Available commands:"
	@echo -e "\tall - compile both pvm and pasm"
	@echo -e "\tpvm - compile P Virtual Machine"
	@echo -e "\tpasm - compile P Assembler"
	@echo -e "\tclean - clean up"
	@echo -e "\thelp - print this help message"

all: pvm pasm

pvm:
	$(CC) $(CFLAGS) -o bin/$(PVM) src/$(PVM).c

pasm:
	$(CC) $(CFLAGS) -o bin/$(PASM) src/$(PASM).c

clean:
	rm -f bin/*
