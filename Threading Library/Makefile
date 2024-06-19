override CFLAGS := -Wall -Werror -std=gnu99 -O0 -g $(CFLAGS) -I.

test_files=./test_busy_threads

all: check

TSTMYPTHREADS=1
ifeq ($(TSTMYPTHREADS),1)
  mythread=threads.o
else
  mythread=
  override LDFLAGS += -pthread
endif

CC = gcc

# Build the threads.o file
threads.o: threads.c ec440threads.h

# rules to build each of the tests
test_busy_threads.o : test_busy_threads.c 
test_busy_threads: test_busy_threads.o $(mythread)


.PHONY: clean check checkprogs

# Build all of the test programs
checkprogs: $(test_files)

# Run the test programs
check: checkprogs
	/bin/bash run_tests.sh $(test_files)

clean:
	rm -f *.o $(test_files) $(test_o_files)
