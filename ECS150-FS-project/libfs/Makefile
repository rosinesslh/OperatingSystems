lib     := libfs.a
objs    := disk.o fs.o

ifneq ($(V),1)
Q = @
endif

CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -MMD -pipe
AR      := ar rcs

ifneq ($(D),1)
CFLAGS	+= -O2
else
CFLAGS	+= -g
endif

all: $(lib)

deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)

libfs.a: $(objs)
	@echo "AR $@"
	$(Q)$(AR) $@ $^

%.o: %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo "clean"
	$(Q)rm -f $(lib) $(objs) $(deps)
