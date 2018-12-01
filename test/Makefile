.PHONY: all clean

all: suid-sleep
	$(MAKE) -C ..
	cp ../chroot-escape jail/chroot-escape

suid-sleep: suid-sleep.c

clean:
	rm -f jail/chroot-escape suid-sleep
