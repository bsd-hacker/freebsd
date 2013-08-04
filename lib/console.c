
#include <lib/string.h>
#include <lib/console.h>

#define OUTD_BUF_SIZE 128
#define OUTU_BUF_SIZE 128
#define OUTX_BUF_SIZE 128

#define UTOC(u) ('0' + u)
#define XTOC(x) "0123456789ABCDEF"[x]

void 
putd(int num)
{
	char buf[OUTD_BUF_SIZE];
	unsigned short is_minus = 0;

	if (num < 0)
	{
		is_minus = 1;
		num = -num;
	}
	if (num < 10)
		putchar(UTOC(num));
	else
	{
		char *bp;

		bp = buf + OUTU_BUF_SIZE- 1;
		*bp-- = '\0';
		while (num && bp >= buf)
		{
			*bp-- = UTOC(num % 10);
			num /= 10;
		}
		if (is_minus)
			putchar('-');
		puts(++bp);
	}
}

void 
putu(unsigned num)
{
	char buf[OUTU_BUF_SIZE];

	if (num < 10)
		putchar(UTOC(num));
	else
	{
		char *bp;

		bp = buf + OUTU_BUF_SIZE- 1;
		*bp-- = '\0';
		while (num && bp >= buf)
		{
			*bp-- = UTOC(num % 10);
			num /= 10;
		}
		puts(++bp);
	}
}

void 
putlu(unsigned long long num)
{
	char buf[OUTU_BUF_SIZE];

	if (num < 10)
		putchar(UTOC(num));
	else
	{
		char *bp;

		bp = buf + OUTU_BUF_SIZE- 1;
		*bp-- = '\0';
		while (num && bp >= buf)
		{
			*bp-- = UTOC(num % 10);
			num /= 10;
		}
		puts(++bp);
	}
}

void
putx(unsigned num)
{
	char buf[OUTX_BUF_SIZE];

	puts("0x");
	if (num < 16)
		putchar(XTOC(num));
	else
	{
		char *bp;

		bp = buf + OUTX_BUF_SIZE- 1;
		*bp-- = '\0';
		while (num && bp >= buf)
		{
			*bp-- = XTOC(num % 16);
			num /= 16;
		}
		puts(++bp);
	}
}

void 
puts(const char *str)
{
	while (*str)
	        putchar (*str++);
}

void 
putns(const char *str, int n)
{
	while (n--)
		putchar (*str++);
}

int 
getns(char *s, int size)
{
	int i;
	for (i = 0; i < size; i++)
	{
		s[i] = getchar();
		if (s[i] == '\r')
			break;
	}
	return i;
}

int 
printf(const char *format, ...)
{
	int i;
	va_list ap;

	va_start(ap, format);
	for (i = 0; i < strlen(format); i++)
	{
		if (format[i] == '%')
		{
			i++;
			if (i >= strlen(format))
			{
				putchar('%');
				return 0;
			}
			else if (format[i] == 's')
				puts(va_arg(ap, char *));
			else if (format[i] == 'd')
				putd(va_arg(ap, int));
			else if (format[i] == 'u')
			        putu(va_arg(ap, unsigned));
			else if (format[i] == 'U')
				putlu(va_arg(ap, unsigned long long));
			else if (format[i] == 'x' || format[i] == 'p')
				putx(va_arg(ap, unsigned));
			else if (format[i] == 'c')
				putchar(va_arg(ap, int));
			else if (format[i] == 'b')
				puts(va_arg(ap, int) ? "true" : "false");
			else
			{
				putchar('%');
				putchar(format[i]);
			}
			continue;
		}
		else
			putchar(format[i]);
	}
	va_end(ap);
	return 0;
}
