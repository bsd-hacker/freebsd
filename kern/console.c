#include <kern/console.h>

#define COM1_RBR 0x3f8
#define COM1_THR 0x3f8
#define COM1_LSR 0x3fd

static __inline void
outb(unsigned port, unsigned char data)
{
	__asm __volatile("outb %0, %w1" : : "a" (data), "Nd" (port));
}

static __inline unsigned char
inb(unsigned port)
{
	unsigned char	data;

	__asm __volatile("inb %w1, %0" : "=a" (data) : "Nd" (port));
	return (data);
}

void putchar(int c)
{
	while ((inb(COM1_LSR) & 0x20) == 0);
	outb(COM1_THR, c);
}

int getchar(void)
{
	while ((inb(COM1_LSR) & 1) == 0);
	return inb(COM1_RBR);
}
