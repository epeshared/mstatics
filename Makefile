SRC=src
BIN=bin
HEADERS=mstatics.h

default: mstatics

mstatics.o: $(SRC)/mstatics.c $(SRC)/$(HEADERS)
	gcc -c $(SRC)/mstatics.c -o $(SRC)/mstatics.o

mstatics: mstatics.o
	gcc $(SRC)/mstatics.o -o $(BIN)/mstatics

clean:
	-rm -f $(SRC)/mstatics.o
	-rm -f $(BIN)/mstatics
