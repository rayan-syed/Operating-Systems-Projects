override CFLAGS := -Wall -Werror -std=gnu99 -pedantic -O0 -g $(CFLAGS)
override LDLIBS := $(LDLIBS)

test_files=test_make_fs test_mount_umount test_fs_create \
 test_listfiles test_open_close test_fs_write \
 test_get_filesize test_fs_read test_fs_delete \
 test_truncate

test_files := $(addprefix ./, $(test_files))
test_objects := $(addsuffix .o, $(test_files))

#fs.o: fs.c fs.h disk.h

all: check

.PHONY: clean check checkprogs

# Run the test programs
check: checkprogs
	/bin/bash run_tests.sh $(test_files)

# build all of the test programs
checkprogs: $(test_files)

$(test_files): %: %.o fs.o disk.o

$(objects): %.o: %.c

clean:
	rm -f *.o $(test_files)
