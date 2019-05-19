# Target library
lib := libfs.a
objs := fs.o disk.o

CC := gcc
CFLAGS := -Wall -Werror -g 

all: $(lib)

## TODO: Phase 1
deps := $(patsubst %.o, %.d,$(objs))


-include $(deps)

DEPFLAGS = -MMD -MF $(@: .o = .d)


libfs.a: $(objs) 
	ar rcs $@ $^

%.o: %.c
	@echo "CC $@"
	$(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)

clean:
	rm -f $(lib) $(objs) $(deps)
