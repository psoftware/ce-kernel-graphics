#include <lib.h>
#include <sys.h>

int strlen(const char *s)
{
	int l = 0;

	while(*s++)
		++l;

	return l;
}

char *strncpy(char *dest, const char *src, size_t l)
{
	size_t i;

	for(i = 0; i < l && src[i]; ++i)
		dest[i] = src[i];

	return dest;
}

static const char hex_map[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

static void htostr(char *buf, unsigned long l)
{
	int i;

	buf[0] = '0';
	buf[1] = 'x';

	for(i = 9; i > 1; --i) {
		buf[i] = hex_map[l % 16];
		l /= 16;
	}
}

static void itostr(char *buf, unsigned int len, long l)
{
	int i, div = 1000000000, v, w = 0;

	if(l == (-2147483647 - 1)) {
		strncpy(buf, "-2147483648", 12);
		return;
	} else if(l < 0) {
		buf[0] = '-';
		l = -l;
		i = 1;
	} else if(l == 0) {
		buf[0] = '0';
		buf[1] = 0;
		return;
	} else
		i = 0;

	while(i < len - 1 && div != 0) {
		if((v = l / div) || w) {
			buf[i++] = '0' + (char)v;
			w = 1;
		}

		l %= div;
		div /= 10;
	}

	buf[i] = 0;
}

#define DEC_BUFSIZE 12

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
	int in = 0, out = 0, tmp;
	char *aux, buf[DEC_BUFSIZE];

	while(out < size - 1 && fmt[in]) {
		switch(fmt[in]) {
			case '%':
				switch(fmt[++in]) {
					case 'd':
						tmp = va_arg(ap, int);
						itostr(buf, DEC_BUFSIZE, tmp);
						if(strlen(buf) >
								size - out - 1)
							goto end;
						for(aux = buf; *aux; ++aux)
							str[out++] = *aux;
						break;
					case 'x':
						tmp = va_arg(ap, int);
						if(out > size - 11)
							goto end;
						htostr(&str[out], tmp);
						out += 10;
						break;
					case 's':
						aux = va_arg(ap, char *);
						while(out < size - 1 && *aux)
							str[out++] = *aux++;
						break;	
				}
				++in;
				break;
			default:
				str[out++] = fmt[in++];
		}
	}
end:
	str[out++] = 0;

	return out;
}

int snprintf(char *buf, size_t n, const char *fmt, ...)
{
	va_list ap;
	int l;

	va_start(ap, fmt);
	l = vsnprintf(buf, n, fmt, ap);
	va_end(ap);

	return l;
}

int printf(int term, const char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	int l;

	va_start(ap, fmt);
	l = vsnprintf(buf, 1024, fmt, ap);
	va_end(ap);

	writevterm_n(term, buf, l);

	return l;
}


// copia n byte da src a dest
void *memcpy(void *dest, const void *src, unsigned int n)
{
	char       *dest_ptr = static_cast<char*>(dest);
	const char *src_ptr  = static_cast<const char*>(src);

	for (int i = 0; i < n; i++)
		dest_ptr[i] = src_ptr[i];

	return dest;
}

// scrive n byte pari a c, a partire da dest
void *memset(void *dest, int c, unsigned int n)
{
	char *dest_ptr = static_cast<char*>(dest);

        for (int i = 0; i < n; i++)
              dest_ptr[i] = static_cast<char>(c);

        return dest;
}
