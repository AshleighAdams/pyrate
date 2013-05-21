## General
SRC_PREFIX = .
OUT_PREFIX = .

RM = rm -rf


## C++
CPP_COMPILE = g++ -c
CPP_LINK = g++

CPP_COMPILE_OPTIONS = -O3 -Wall -fmessage-length=0 -std=c++11 -Iinclude -D_GLIBCXX_USE_NANOSLEEP -pthread -fPIC
CPP_LINK_OPTIONS = -llua -lpthread -shared

CPP_OBJS = 	$(OUT_PREFIX)/pyrate.o
CPP_OUT = $(OUT_PREFIX)/pyrate.so

$(OUT_PREFIX)/%.o: $(SRC_PREFIX)/%.cpp
	$(CPP_COMPILE) $(CPP_COMPILE_OPTIONS) -o$@ $<

$(CPP_OUT): $(CPP_OBJS)
	$(CPP_LINK) $(CPP_LINK_OPTIONS) -o$@ $(CPP_OBJS)

cpp_build: $(CPP_OUT)

cpp_clean:
	$(RM) $(CPP_OUT) $(CPP_OBJS)

cpp_test: $(CPP_OUT)
	./$(CPP_OUT)


## CTags
ctags:
	ctags -f .tags -R /usr/include .


## Shortcuts
all: cpp_build
clean: cpp_clean
test: cpp_test
