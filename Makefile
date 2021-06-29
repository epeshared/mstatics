SRC=/home/xtang/mstatics/src
LIB=/home/xtang/mstatics/lib
TEST_DIR=test
HEADERS=mstatics.hpp

all: libs mtest
libs: libmstatics.so

mtest:$(TEST_DIR)/test.c
	#works for boost
	g++ -std=c++11 -lpthread $(TEST_DIR)/test.c -fno-pie -ggdb3 -O0 -no-pie -o $(TEST_DIR)/mtest -export-dynamic -ldl 
	#g++ -std=c++17 -lpthread $(TEST_DIR)/test.c -o $(TEST_DIR)/mtest

libmstatics.so: $(SRC)/mstatics.cpp $(SRC)/$(HEADERS)
	g++ -std=c++11 -fPIC -g -ggdb -O0 -shared -Wl,-z,defs,--as-needed -I/usr/include/ -I./include -I./  $(SRC)/mstatics.cpp -lpthread  -ldl -ldw -L/usr/lib64/libiberty.a  -o $(LIB)/libmstatics.so  -export-dynamic -ldl 

clean:
	-rm -f $(LIB)/libmstatics.so
	-rm -f $(SRC)/mstatics.o
	-rm -f $(TEST_DIR)/test.o
	-rm -f $(TEST_DIR)/mtest
