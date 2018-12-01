.PHONY: clean
CFLAGS+=-Wall -Wextra -static

objs = 	obj/chroot-escape.o obj/proc.o \
		obj/ptrace-check.o obj/try-escape.o \
		obj/network.o

chroot-escape: $(objs)
	$(CC) $(CFLAGS) $^ -o $@

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(objs) chroot-escape
