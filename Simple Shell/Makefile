CC=gcc
CFLAGS=-Wall -Wextra

# The target executable
TARGET=myshell

# Source files
SOURCES=myshell.c myshell_parser.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(TARGET)

#override CFLAGS := -std=gnu99 -O0 -Wall -Werror -g  -fsanitize=undefined $(CFLAGS) -I.
#override LDFLAGS := -fsanitize=undefined -fsanitize=leak $(LDLAGS)  
CC = gcc

#all: check

# # rule for making the parser.o  that is needed by all the test programs
#myshell_parser.o: myshell_parser.c myshell_parser.h

#test_simple_input.o: test_simple_input.c myshell_parser.h
#test_simple_pipe.o: test_simple_pipe.c myshell_parser.h
#test_simple_input2.o: test_simple_input2.c myshell_parser.h
#test_simple_pipe2.o: test_simple_pipe.c myshell_parser.h
#test_simple_background.o:test_simple_background.c myshell_parser.h
#test_simple_redirect.o:test_simple_redirect.c myshell_parser.h
#test_double_redirect.o: test_double_redirect.c myshell_parser.h
#test_all_features.o:test_all_features.c myshell_parser.h

#test_simple_input : test_simple_input.o myshell_parser.o
#test_simple_pipe : test_simple_pipe.o myshell_parser.o
#test_simple_input2: test_simple_input2.o myshell_parser.o
#test_simple_pipe2: test_simple_pipe2.o myshell_parser.o
#test_simple_background: test_simple_background.o myshell_parser.o
#test_simple_redirect: test_simple_redirect.o myshell_parser.o
#test_double_redirect: test_double_redirect.o myshell_parser.o
#test_all_features: test_all_features.o myshell_parser.o

# # Add any additional tests here
# test_files=./test_simple_input ./test_simple_pipe ./test_simple_input2 ./test_simple_pipe2 ./test_simple_background ./test_simple_redirect ./test_double_redirect ./test_all_features

#.PHONY: clean check checkprogs all

# Build all of the test program
checkprogs: $(test_files)

#check: checkprogs
	/bin/bash run_tests.sh $(test_files)

#clean:
	rm -f *~ *.o $(test_files) $(test_o_files)
