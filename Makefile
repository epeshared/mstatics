SRC=/home/xtang/mstatics/src
LIB=/home/xtang/mstatics/lib
TEST_DIR=test
HEADERS=mstatics.h

all: libs mtest
libs: libmstatics.so

mtest:$(TEST_DIR)/test.c
	gcc $(TEST_DIR)/test.c -O3 -o $(TEST_DIR)/mtest

libmstatics.so: $(SRC)/mstatics.c $(SRC)/$(HEADERS)
	gcc -fPIC -shared -Wl,-z,defs,--as-needed  $(SRC)/mstatics.c -ldl -O3 -o $(LIB)/libmstatics.so 

clean:
	-rm -f $(LIB)/libmstatics.so
	-rm -f $(SRC)/mstatics.o
	-rm -f $(TEST_DIR)/test.o
	-rm -f $(TEST_DIR)/mtest
