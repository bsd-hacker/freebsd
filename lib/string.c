#include <lib/string.h>
#include <lib/console.h>

//#define DPRINTF (printf("[%s:%s:%d] ", __FILE__, __FUNCTION__, __LINE__), printf)
#define DPRINTF(...) do{}while(0)

int
strlen(const char *s)
{
	int i = 0;
	while (*s++)
		i++;
	return i;
}

void *
memset(void *s, int c, int n)
{
	DPRINTF("s:%x c:%d n:%d\n", s, c, n);
	int i;
	for (i = 0; i < n; i++)
		((char *)s)[i] = c;
	return s;
}

void *
memcpy(void *dest, const void *src, int n)
{
	DPRINTF("dest:%x src:%x n:%d\n", dest, src, n);
	int i;
	for (i = 0; i < n; i++)
		((char *)dest)[i] = ((char *)src)[i];
	return dest;
}

char *
strcpy(char *dest, const char *src)
{
	int i;
	for (i = 0; src[i]; i++)
		dest[i] = ((char *)src)[i];
	return dest;
}

char *
strncpy(char *dest, const char *src, size_t n)
{
	DPRINTF("dest:%x src:%x n:%d\n", dest, src, n);
	int i;
	for (i = 0; i < n && src[i] != '\0'; i++)
		dest[i] = src[i];
	for (; i < n; i++)
		dest[i] = '\0';
	return dest;
}

int 
strcmp(const char *s1, const char *s2)
{
	int i;
	for(i = 0; s1[i] == s2[i]; i++)
		if(!s1[i])
			return 0;
	return s1[i] - s2[i];
}

int
strncmp(const char *s1, const char *s2, size_t n)
{
	int i;
	for(i = 0; s1[i] == s2[i] && i < n; i++)
		if(!s1[i])
			return 0;
	return s1[i] - s2[i];
}
