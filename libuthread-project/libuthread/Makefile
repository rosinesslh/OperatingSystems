targets := libuthread.a
objs    := queue.o context.o uthread.o preempt.o

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

all: $(targets)

deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)

libuthread.a: $(objs)
	@echo "AR $@"
	$(Q)$(AR) $@ $^

%.o: %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo "clean"
	$(Q)rm -f $(targets) $(objs) $(deps)
