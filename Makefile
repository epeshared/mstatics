SRC=/home/xtang/mstatics/src
LIB=/home/xtang/mstatics/lib
TEST_DIR=test
HEADERS=mstatics.h

all: libs mtest
libs: libmstatics.so

mtest:$(TEST_DIR)/test.c
	gcc -lpthread  $(TEST_DIR)/test.c -O3 -o $(TEST_DIR)/mtest

libmstatics.so: $(SRC)/mstatics.c $(SRC)/$(HEADERS)
	gcc -fPIC -g -ggdb -O0  -shared -Wl,-z,defs,--as-needed -I./  $(SRC)/mstatics.c -ldl -lpthread -o $(LIB)/libmstatics.so 

clean:
	-rm -f $(LIB)/libmstatics.so
	-rm -f $(SRC)/mstatics.o
	-rm -f $(TEST_DIR)/test.o
	-rm -f $(TEST_DIR)/mtest
