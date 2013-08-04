CFLAGS = -I. -Werror -Wall -Wmissing-prototypes -Wunused -Wshadow -Wpointer-arith -Wundef -fno-builtin -O
CFLAGS += $(shell if $(CC) -fno-stack-protector -S -o /dev/null -xc /dev/null > /dev/null 2>&1; then echo "-fno-stack-protector"; fi)
LDFLAGS = -T ldscript
OBJS = kern/boot.o kern/test1.o kern/console.o lib/console.o lib/string.o

all: test1.bin

test1.bin: test1
	objcopy -I elf64-x86-64 -O binary test1 test1.bin	

test1: $(OBJS)
	$(LD) -Map test1.map $(LDFLAGS) -o test1 $(OBJS) `$(CC) -print-libgcc-file-name`

run:
	-sudo bhyvectl --vm=test1 --destroy
	sudo bhyveload -m 256 -S`pwd`/test1.bin:0x100000 test1
	sudo bhyve -m 256 -e -H -S 31,uart,stdio test1

clean:
	rm -f $(OBJS) test1 test1.*

.S.o:
	$(CC) -c $(CFLAGS) $< -o $@

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@
