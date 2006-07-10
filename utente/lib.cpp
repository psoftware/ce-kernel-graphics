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
	str[out] = 0;

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
////////////////////////////////////////////////////////////////////////////////
//                          TERMINALI VIRTUALI                               
////////////////////////////////////////////////////////////////////////////////

#include <keycodes.h>

struct des_vterm;
struct vterm_map {
	void (*action)(des_vterm*, int v, unsigned short code);
	int arg;
};

#define VTERM_MODIF_SHIFT	1U
#define VTERM_MODIF_ALTGR	2U
#define VTERM_MODIF_CTRL	4U
#define VTERM_MODIF_ALT		8U

char vterm_decode_map[4][64] = {
  // tasto senza modificatori
 { 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
      'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
      'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'',
      '`', '\\',
      'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
      ' ',
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       '\b', '\n', '\r', '\t'
 },
 // tasto + shift
 { 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 
      'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
      'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"',
      '~', '|',
      'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
      ' ',
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       '\b', '\r', '\n', '\t'
 },
 // tasto + altgr
 { 0
 },
 // tasto + shift + altgr
 { 0
 },
};

typedef void (*vterm_ctrl_func)(des_vterm *, char c);

static void vterm_do_ctrl(des_vterm*, char);
static void vterm_edit_eof(des_vterm*, char);

vterm_ctrl_func vterm_ctrl_map[4][32] = {
 { 0,
 },
  // tasto + ctrl
 {
	vterm_do_ctrl,	// '@'
	vterm_do_ctrl,	// 'a'
	vterm_do_ctrl,	// 'b'
	vterm_do_ctrl,	// 'c'
	vterm_edit_eof,	// 'd'
	vterm_do_ctrl,	// 'e'
	vterm_do_ctrl,	// 'f'
	vterm_do_ctrl,	// 'g'
	vterm_do_ctrl,	// 'h'
	vterm_do_ctrl,	// 'i'
	vterm_do_ctrl,	// 'j'
	vterm_do_ctrl,	// 'k'
	vterm_do_ctrl,	// 'l'
	vterm_do_ctrl,	// 'm'
	vterm_do_ctrl,	// 'n'
	vterm_do_ctrl,	// 'o'
	vterm_do_ctrl,	// 'p'
	vterm_do_ctrl,	// 'q'
	vterm_do_ctrl,	// 'r'
	vterm_do_ctrl,	// 's'
	vterm_do_ctrl,	// 't'
	vterm_do_ctrl,	// 'u'
	vterm_do_ctrl,	// 'v'
	vterm_do_ctrl,	// 'w'
	vterm_do_ctrl,	// 'x'
	vterm_do_ctrl,	// 'y'
	vterm_do_ctrl,	// 'z'
	vterm_do_ctrl,	// '['
	vterm_do_ctrl,	// '\'
	vterm_do_ctrl,	// ']'
	vterm_do_ctrl,	// '^'
	vterm_do_ctrl 	// '_'
 },
  // tasto + alt
 { 0
 },
  // tasto + ctrl + alt
 { 0
 }
};

static void vterm_esc(des_vterm*, int v, unsigned short code);
static void vterm_decode(des_vterm*, int v, unsigned short code);
static void vterm_modif(des_vterm*, int v, unsigned short code); 
static void vterm_motion(des_vterm*, int v, unsigned short code);
static void vterm_fnkey(des_vterm*, int v, unsigned short code); 
static void vterm_keypad(des_vterm*, int v, unsigned short code);
static void vterm_edit(des_vterm*, int v, unsigned short code);
static void vterm_special(des_vterm*, int v, unsigned short code);
static void vterm_tab(des_vterm*, int v, unsigned short code);
static void vterm_enter(des_vterm*, int v, unsigned short code);
static void vterm_numlock(des_vterm*, int v, unsigned short code);
static void vterm_capslock(des_vterm*, int v, unsigned short code);
static void vterm_scrlock(des_vterm*, int v, unsigned short code);
static void vterm_sysrq(des_vterm*, int v, unsigned short code);



#define VTERM_EDIT_HOME		'7'
#define VTERM_EDIT_END		'1'
#define VTERM_EDIT_LEFT		'4'
#define VTERM_EDIT_RIGHT	'6'
#define VTERM_EDIT_INSERT	'0'
#define VTERM_EDIT_DELETE	'.'
#define VTERM_EDIT_UP		'8'
#define VTERM_EDIT_DOWN		'2'
#define VTERM_EDIT_PAGEUP       '9'
#define VTERM_EDIT_PAGEDOWN    	'3'

#define VTERM_MOTION_HOME	VTERM_EDIT_HOME
#define VTERM_MOTION_END	VTERM_EDIT_END
#define VTERM_MOTION_UP		VTERM_EDIT_UP	   
#define VTERM_MOTION_DOWN	VTERM_EDIT_DOWN	   
#define VTERM_MOTION_PAGEUP	VTERM_EDIT_PAGEUP  
#define VTERM_MOTION_PAGEDOWN	VTERM_EDIT_PAGEDOWN

#include <colors.h>


static unsigned int vterm_shift(unsigned int flags)
{
	return (flags & (VTERM_MODIF_SHIFT | VTERM_MODIF_ALTGR));
}
static unsigned int vterm_ctrl(unsigned int flags)
{
	return (flags & (VTERM_MODIF_CTRL | VTERM_MODIF_ALT)) >> 2U;
}


vterm_map vterm_maps[128] = {
	{ 0,             0 }, // 0x00
	{ vterm_esc,     0 }, // 0x01 esc
	{ vterm_decode,  1 }, // 0x02 1
	{ vterm_decode,  2 }, // 0x03 2
	{ vterm_decode,  3 }, // 0x04 3
	{ vterm_decode,  4 }, // 0x05 4
	{ vterm_decode,  5 }, // 0x06 5
	{ vterm_decode,  6 }, // 0x07 6
	{ vterm_decode,  7 }, // 0x08 7
	{ vterm_decode,  8 }, // 0x09 8
	{ vterm_decode,  9 }, // 0x0a 9
	{ vterm_decode, 10 }, // 0x0b 0
	{ vterm_decode, 11 }, // 0x0c -
	{ vterm_decode, 12 }, // 0x0d =
	{ vterm_decode, 60 }, // 0x0e backspace
	{ vterm_decode,	63 }, // 0x0f tab
	{ vterm_decode, 13 }, // 0x10 q
	{ vterm_decode, 14 }, // 0x11 w
	{ vterm_decode, 15 }, // 0x12 e
	{ vterm_decode, 16 }, // 0x13 r
	{ vterm_decode, 17 }, // 0x14 t
	{ vterm_decode, 18 }, // 0x15 y
	{ vterm_decode, 19 }, // 0x16 u
	{ vterm_decode, 20 }, // 0x17 i
	{ vterm_decode, 21 }, // 0x18 o
	{ vterm_decode, 22 }, // 0x19 p
	{ vterm_decode, 23 }, // 0x1a {
	{ vterm_decode, 24 }, // 0x1b }
	{ vterm_decode, 61 }, // 0x1c 
	{ vterm_modif,  VTERM_MODIF_CTRL }, // 0x1d Lctrl
	{ vterm_decode, 25 }, // 0x1e a
	{ vterm_decode, 26 }, // 0x1f s
	{ vterm_decode, 27 } ,// 0x20 d
	{ vterm_decode, 28 }, // 0x21 f
	{ vterm_decode, 29 }, // 0x22 g
	{ vterm_decode, 30 }, // 0x23 h
	{ vterm_decode, 31 }, // 0x24 j
	{ vterm_decode, 32 }, // 0x25 k
	{ vterm_decode, 33 }, // 0x26 l
	{ vterm_decode, 34 }, // 0x27 ;
	{ vterm_decode, 35 }, // 0x28 '
	{ vterm_decode, 36 }, // 0x29 `
	{ vterm_modif,  VTERM_MODIF_SHIFT }, // 0x2a Lshift
	{ vterm_decode, 37 }, // 0x2b backslash
	{ vterm_decode, 38 }, // 0x2c z
	{ vterm_decode, 39 }, // 0x2d x
	{ vterm_decode, 40 }, // 0x2e c
	{ vterm_decode, 41 }, // 0x2f v
	{ vterm_decode, 42 }, // 0x30 b
	{ vterm_decode, 43 }, // 0x31 n
	{ vterm_decode, 44 }, // 0x32 m
	{ vterm_decode, 45 }, // 0x33 ,
	{ vterm_decode, 46 }, // 0x34 .
	{ vterm_decode, 47 }, // 0x35 /
	{ vterm_modif,  VTERM_MODIF_SHIFT }, // 0x 36 Rshift
	{ vterm_keypad, '*' }, // 0x37 keypad *
	{ vterm_modif,  VTERM_MODIF_ALT }, // 0x38 Lalt
	{ vterm_decode, 48 }, // 0x39 space
	{ vterm_capslock, 0 }, // 0x3a caps lock
	{ vterm_fnkey,   0 }, // 0x3b F1
	{ vterm_fnkey,   1 }, // 0x3c F2
	{ vterm_fnkey,   2 }, // 0x3d F3
	{ vterm_fnkey,   3 }, // 0x3e F4
	{ vterm_fnkey,   4 }, // 0x3f F5
	{ vterm_fnkey,   5 }, // 0x40 F6
	{ vterm_fnkey,   6 }, // 0x41 F7
	{ vterm_fnkey,   7 }, // 0x42 F8
	{ vterm_fnkey,   8 }, // 0x43 F9
	{ vterm_fnkey,   9 }, // 0x44 F10
	{ vterm_numlock, 0 }, // 0x45
	{ vterm_scrlock, 0 }, // 0x46
	{ vterm_keypad,  '7' }, // 0x47 keypad 7
	{ vterm_keypad,  '8' }, // 0x48 keypad 8
	{ vterm_keypad,  '9' }, // 0x49 keypad 9
	{ vterm_keypad,  '-' }, // 0x4a keypad -
	{ vterm_keypad,  '4' }, // 0x4b keypad 4
	{ vterm_keypad,  '5' }, // 0x4c keypad 5
	{ vterm_keypad,  '6' }, // 0x4d keypad 6
	{ vterm_keypad,  '+' }, // 0x4e keypad +
	{ vterm_keypad,  '1' }, // 0x4f keypad 1
	{ vterm_keypad,  '2' }, // 0x50 keypad 2
	{ vterm_keypad,  '3' }, // 0x51 keypad 3
	{ vterm_keypad,  '0' }, // 0x52 keypad 0
	{ vterm_keypad,  '.' }, // 0x53 keypad .
	{ 0,	         0 }, // 0x54
	{ 0,	         0 }, // 0x55
	{ 0,	         0 }, // 0x56
	{ vterm_fnkey,  10 }, // 0x57 F11
	{ vterm_fnkey,  11 }  // 0x58 F12
};

vterm_map vterm_emaps[128] = {
	{ 0,             0 }, // 0x00
	{ 0,             0 }, // 0x01
	{ 0,             0 }, // 0x02
	{ 0,             0 }, // 0x03
	{ 0,             0 }, // 0x04
	{ 0,             0 }, // 0x05
	{ 0,             0 }, // 0x06
	{ 0,             0 }, // 0x07
	{ 0,             0 }, // 0x08
	{ 0,             0 }, // 0x09
	{ 0,             0 }, // 0x0a
	{ 0,             0 }, // 0x0b
	{ 0,             0 }, // 0x0c
	{ 0,             0 }, // 0x0d
	{ 0,             0 }, // 0x0e
	{ 0,             0 }, // 0x0f
	{ 0,             0 }, // 0x10
	{ 0,             0 }, // 0x11
	{ 0,             0 }, // 0x12
	{ 0,             0 }, // 0x13
	{ 0,             0 }, // 0x14
	{ 0,             0 }, // 0x15
	{ 0,             0 }, // 0x16
	{ 0,             0 }, // 0x17
	{ 0,             0 }, // 0x18
	{ 0,             0 }, // 0x19
	{ 0,             0 }, // 0x1a
	{ 0,             0 }, // 0x1b
	{ vterm_keypad, '\n' }, // 0x1c keypad enter
	{ vterm_modif,  VTERM_MODIF_CTRL }, // 0x1d Rctrl
	{ 0,             0 }, // 0x1e
	{ 0,             0 }, // 0x1f
	{ 0,             0 }, // 0x20
	{ 0,             0 }, // 0x21
	{ 0,             0 }, // 0x22
	{ 0,             0 }, // 0x23
	{ 0,             0 }, // 0x24
	{ 0,             0 }, // 0x25
	{ 0,             0 }, // 0x26
	{ 0,             0 }, // 0x27
	{ 0,             0 }, // 0x28
	{ 0,             0 }, // 0x29
	{ 0,             0 }, // 0x2a
	{ 0,             0 }, // 0x2b
	{ 0,             0 }, // 0x2c
	{ 0,             0 }, // 0x2d
	{ 0,             0 }, // 0x2e
	{ 0,             0 }, // 0x2f
	{ 0,             0 }, // 0x30
	{ 0,             0 }, // 0x31
	{ 0,             0 }, // 0x32
	{ 0,             0 }, // 0x33
	{ 0,             0 }, // 0x34
	{ vterm_keypad, '/' }, // 0x35 keypad /
	{ 0,             0 }, // 0x36
	{ vterm_sysrq,   0 }, // 0x37
	{ vterm_modif,   VTERM_MODIF_ALTGR }, // 0x38 Ralt
	{ 0,             0 }, // 0x39
	{ 0,             0 }, // 0x3a
	{ 0,             0 }, // 0x3b
	{ 0,             0 }, // 0x3c
	{ 0,             0 }, // 0x3d
	{ 0,             0 }, // 0x3e
	{ 0,             0 }, // 0x3f
	{ 0,             0 }, // 0x40
	{ 0,             0 }, // 0x41
	{ 0,             0 }, // 0x42
	{ 0,             0 }, // 0x43
	{ 0,             0 }, // 0x44
	{ 0,             0 }, // 0x45
	{ 0,             0 }, // 0x46
	{ vterm_edit,    VTERM_EDIT_HOME }, // 0x47 home
	{ vterm_edit,    VTERM_EDIT_UP }, // 0x48 up
	{ vterm_edit,    VTERM_EDIT_PAGEUP }, // 0x49 page up
	{ 0,             0 }, // 0x4a
	{ vterm_edit,    VTERM_EDIT_LEFT }, // 0x4b left
	{ 0,             0 }, // 0x4c
	{ vterm_edit,    VTERM_EDIT_RIGHT }, // 0x4d right
	{ 0,             0 }, // 0x4e
	{ vterm_edit,    VTERM_EDIT_END }, // 0x4f end
	{ vterm_edit,    VTERM_EDIT_DOWN }, // 0x50 down
	{ vterm_edit,    VTERM_EDIT_PAGEDOWN }, // 0x51 page down
	{ vterm_edit,    VTERM_EDIT_INSERT }, // 0x52 insert
	{ vterm_edit,    VTERM_EDIT_DELETE }, // 0x53 delete
	{ 0,             0 }, // 0x54
	{ 0,             0 }, // 0x55
	{ 0,             0 }, // 0x56
	{ 0,             0 }, // 0x57
	{ 0,             0 }, // 0x58
	{ 0,             0 }, // 0x59
	{ 0,             0 }, // 0x5a
	{ vterm_special, 1 }, // 0x5b Lwin
	{ vterm_special, 1 }, // 0x5c Rwin
	{ vterm_special, 2 }, // 0x5d menu
};


enum funz { none, input_ln, input_n };

struct des_vterm_global {
	bool numlock;		// stato del tasto numlock
	bool capslock;		// stato del tasto capslock
	unsigned int flags;	// stato dei tasti modificatori
} vterm_global;

struct des_vterm {
	int num;		// numero del terminale virtuale
	int id;			// identificatore del processo di input

	int mutex_r;		// mutua esclusione in lettura
	int sincr;		// sincronizzazione in lettura
	int vkbd;		// id della tastiera virtuale (dal modulo IO)
	int vmon;		// id del monitor virtuale (dal modulo IO)
	int cont;		// caratteri contenuti nel buffer di lettura
	int orig_cont;		// dimensione del buffer di lettura
	char* punt;		// puntatore di inserimento nel buffer
	char* orig_punt;	// primo carattere nel buffer di lettura
	int orig_off;		// offset (rispetto a "base") sul video del
				// primo carattere nel buffer
	int letti;		// caratteri letti
	funz funzione;		// tipo di lettura (linea o fisso)
	bool echo;		// echo su video dei caratteri inseriti
	bool insert;		// modalita' inserimento o sostituzione

	int mutex_w;		// mutua esclusione in scrittura
	unsigned short *video;	// puntatore al buffer video
	int video_max_x,	// numero di colonne del buffer video
	    video_max_y;	// numero di righe del buffer video
	unsigned int base;	// offset (rispetto a "video") 
				// del primo caratter valido nel
				// buffer video (allineato alla riga)
	int video_off;		// offset (rispetto a "base") del cursore
	int append_off;		// offset (rispetto a "base") della posizione
				// successiva all'ultimo carattere valido
				// contenuto nel buffer video
	int pref_off;		// offset (rispetto a "base") a cui il cursore si dovrebbe spostare
				// tramite le operazioni di movimento cursore
	int vmon_off;		// offset (rispetto a "base") del primo
				// carattere visualizzato sul monitor virtuale
	vterm_edit_status stat;
	unsigned int vmon_size;	// dimensione del monitor virtuale
	unsigned int video_size;// dimensione del buffer video
	unsigned char attr;	// byte di attributi colore
	int tab;		// dimensione delle tabulazioni

	bool scroll_lock;	// blocco dello scorrimento
	int scroll_sincr;	// sincronizzazione sul blocco dello scorrimento
	int waiting_scroll;	// numero di processi in attesa dello sblocco
				// dello scorrimento
};

void vterm_switch(int vterm);
static void vterm_update_vmoncursor(des_vterm *t);
static void vterm_update_vmon(des_vterm *t, int first, int end);
static int vterm_row(const des_vterm *t, int off);
static void vterm_move_cursor(des_vterm *t, int off);
static void vterm_write_chars(des_vterm* t, const char vetti[], int quanti);
static bool vterm_make_visible(des_vterm* t, int off);
static int vterm_calc_off(const des_vterm *t, int off, const char* start, const char* end);
static void vterm_rewrite_chars(des_vterm *t, const char vetti[], int quanti, bool visible = false);
static void vterm_edit_insert(des_vterm *t, const char* v, int quanti);
static void vterm_edit_replace(des_vterm *t, const char *v, int quanti);
static void vterm_edit_remove(des_vterm *t, int quanti);
static int vterm_delta_off(const des_vterm *t, int off, char c);
static void vterm_output_status(des_vterm *t);

void vterm_esc(des_vterm*, int v, unsigned short code)
{
}

void vterm_add_char(des_vterm *t, char c)
{
	bool fine = false;

	if (t->funzione != input_n && t->funzione != input_ln)
		return;

	if (t->echo)
		sem_wait(t->mutex_w);

	if (c == '\b') {
		if (t->punt > t->orig_punt) {
			t->punt--;
			t->pref_off = vterm_calc_off(t, t->orig_off, t->orig_punt, t->punt);
			vterm_move_cursor(t, t->pref_off);
			vterm_edit_remove(t, 1);
		}
	} else if (c == '\n' && t->funzione == input_ln) {
		*(t->orig_punt + t->cont) = '\0';
		vterm_output_status(t);
		if (t->echo) {
			vterm_move_cursor(t, t->append_off);
			vterm_write_chars(t, &c, 1);
		}
		fine = true;
	} else {
		if (t->punt < t->orig_punt + t->orig_cont) {
			if (t->insert) {
				if (t->cont < t->orig_cont)
					vterm_edit_insert(t, &c, 1);
			} else {
				vterm_edit_replace(t, &c, 1);
			}
		}
		if (t->funzione == input_n && t->cont >= t->orig_cont) {
			vterm_output_status(t);
			vterm_move_cursor(t, t->append_off);
			fine = true;
		}
	}
	if (fine) {
		t->letti += t->cont;
		t->funzione = none;
	}
	if (t->echo)
		sem_signal(t->mutex_w);
	if (fine)
		sem_signal(t->sincr);
}

void vterm_edit_eof(des_vterm *t, char c)
{
	if (t->funzione != input_n)
		return;

	vterm_move_cursor(t, t->append_off);
	t->letti += t->cont;
	t->funzione = none;
	if (t->echo)
		sem_signal(t->mutex_w);
	sem_signal(t->sincr);
}

void vterm_decode(des_vterm* t, int v, unsigned short code)
{
	if (code & 0x0080) 
		return;

	char c = vterm_decode_map[vterm_shift(vterm_global.flags)][v];
	if (!c)
		return;

	if ((vterm_global.capslock || (vterm_ctrl(vterm_global.flags))) && (c >= 'a' && c <= 'z'))
		c = c - 'a' + 'A';

	if (c >= '@' && c <= '_') {
		vterm_ctrl_func f = vterm_ctrl_map[vterm_ctrl(vterm_global.flags)][c - '@'];
		if (f) {
			(*f)(t, c);
			return;
		}
	}

	vterm_add_char(t, c);

}

void vterm_do_ctrl(des_vterm *t, char c)
{
	vterm_add_char(t, c ^ 0x40);
}

void vterm_modif(des_vterm* t, int v, unsigned short code)
{
	if (code & 0x0080) 
		vterm_global.flags &= ~v;	
	else
		vterm_global.flags |= v;
}
		
void vterm_motion(des_vterm* t, int v, unsigned short code)
{
	if (code & 0x0080)
		return;

	int move = 0;
	sem_wait(t->mutex_w);
	switch (v) {
	case VTERM_MOTION_HOME:
		t->vmon_off = 0;
		break;
	case VTERM_MOTION_END:
		t->vmon_off = vterm_row(t, t->append_off) + t->video_max_x;
		break;
	case VTERM_MOTION_UP:
		move = -t->video_max_x;
		break;
	case VTERM_MOTION_DOWN:
		move = t->video_max_x;
		break;
	case VTERM_MOTION_PAGEUP:
		move = -t->vmon_size;
		break;
	case VTERM_MOTION_PAGEDOWN:
		move = t->vmon_size;
		break;
	}
	if (t->vmon_off + move < 0)
		t->vmon_off = 0;
	else if (t->vmon_off + move + t->vmon_size > vterm_row(t, t->append_off) + t->video_max_x)
		t->vmon_off = vterm_row(t, t->append_off) + t->video_max_x - t->vmon_size;
	else
		t->vmon_off += move;
	vterm_update_vmon(t, t->vmon_off, t->vmon_off + t->vmon_size);
	sem_signal(t->mutex_w);
}

void vterm_fnkey(des_vterm* t, int v, unsigned short code)
{
	if (code & 0x00800)
		return;

	if (vterm_global.flags & VTERM_MODIF_ALT)
		vterm_switch(v + ((vterm_global.flags & VTERM_MODIF_SHIFT) ? 12 : 0));
}

void vterm_keypad(des_vterm* t, int v, unsigned short code)
{
	if (code & 0x0080)
		return;

	if (vterm_global.numlock)
		vterm_add_char(t, (char)v);
	else
		vterm_edit(t, v, code);
}

static
void vterm_edit_insert(des_vterm *t, const char* v, int quanti)
{
	int last = t->cont - 1;
	if (quanti + t->cont > t->orig_cont)
		return;
	for (char *p = t->orig_punt + last; p >= t->punt; p--)
		*(p + quanti) = *p;
	for (int i = 0; i < quanti; i++)
		t->punt[i] = v[i];
	t->cont += quanti;
	if (t->echo) {
		vterm_write_chars(t, t->punt, quanti);
		t->pref_off = t->video_off;
		vterm_rewrite_chars(t, t->punt + quanti, (t->orig_punt + t->cont) - (t->punt + quanti));
	}
	t->punt += quanti;
}

static
void vterm_edit_replace(des_vterm *t, const char *v, int quanti)
{
	if (t->punt + quanti > t->orig_punt + t->orig_cont)
		return;
	for (int i = 0; i < quanti; i++)
		t->punt[i] = v[i];
	if (t->echo) {
		vterm_write_chars(t, t->punt, quanti);
		t->pref_off = t->video_off;
		vterm_rewrite_chars(t, t->punt + quanti, (t->orig_punt + t->cont) - (t->punt + quanti));
	}
	t->punt += quanti;
}

static
void vterm_edit_remove(des_vterm *t, int quanti)
{
	if (t->punt + quanti > t->orig_punt + t->cont)
		return;
	for (char *p = t->punt; p + quanti < t->orig_punt + t->cont; p++)
		*p = *(p + quanti);
	t->cont -= quanti;
	if (t->echo) 
		vterm_rewrite_chars(t, t->punt, (t->orig_punt + t->cont) - t->punt);
}

static
bool vterm_edit_moveto(des_vterm *t, int off)
{
	int scan = t->orig_off, prec = scan;
	char *p = t->orig_punt;
	while (scan <= off && (p - t->orig_punt) < t->cont) {
		prec = scan;
		scan += vterm_delta_off(t, scan, *p);
		p++;
	}
	if (vterm_row(t, prec) != vterm_row(t, t->video_off)) {
		t->punt = p - 1;
		vterm_move_cursor(t, prec);
		return true;
	}
	return false;
}

void vterm_edit(des_vterm* t, int v, unsigned short code)
{
	int target;

	if (vterm_global.flags & VTERM_MODIF_SHIFT) {
		vterm_motion(t, v, code);
		return;
	}

	if ((code & 0x0080) || t->funzione != input_n && t->funzione != input_ln ) 
		return;
	
	if (t->echo)
		sem_wait(t->mutex_w);

	switch (v) {
	case VTERM_EDIT_HOME:
		t->pref_off = t->orig_off;
		vterm_move_cursor(t, t->pref_off);
		t->punt = t->orig_punt;
		break;
	case VTERM_EDIT_END:
		t->punt = t->orig_punt + t->cont;
		t->pref_off = vterm_calc_off(t, t->orig_off, t->orig_punt, t->punt);
		vterm_move_cursor(t, t->pref_off);
		break;
	case VTERM_EDIT_LEFT:
		if (t->punt > t->orig_punt) {
			t->punt--;
			t->pref_off = vterm_calc_off(t, t->orig_off, t->orig_punt, t->punt);
			vterm_move_cursor(t, t->pref_off);
		}
		break;
	case VTERM_EDIT_RIGHT:
		if (t->punt < t->orig_punt + t->cont) {
			t->punt++;
			t->pref_off = vterm_calc_off(t, t->orig_off, t->orig_punt, t->punt);
			vterm_move_cursor(t, t->pref_off);
		}
		break;
	case VTERM_EDIT_UP:
		if (vterm_edit_moveto(t, t->pref_off - t->video_max_x)) {
			t->pref_off -= t->video_max_x;
		} 
		break;
	case VTERM_EDIT_PAGEUP:
		if ( (target = t->pref_off - t->vmon_size) > 0) {
		    	t->pref_off = target;
			vterm_edit_moveto(t, t->pref_off);
		}
		break;
	case VTERM_EDIT_DOWN:
		if (vterm_edit_moveto(t, t->pref_off + t->video_max_x)) {
			t->pref_off += t->video_max_x;
		}
		break;
	case VTERM_EDIT_PAGEDOWN:
		if ( (target = t->pref_off + t->vmon_size) <= vterm_row(t, t->append_off) + t->video_max_x) {
			t->pref_off = target;
			vterm_edit_moveto(t, t->pref_off);
		}
		break;
	case VTERM_EDIT_INSERT:
		t->insert = !t->insert;
		vmon_cursor_shape(t->vmon, (t->insert ? 0 : 1));
		break;
	case VTERM_EDIT_DELETE:
		if (t->punt < t->orig_punt + t->cont) 
			vterm_edit_remove(t, 1);
		break;
	}
	if (t->echo)
		sem_signal(t->mutex_w);

}
void vterm_special(des_vterm*, int v, unsigned short code)
{ }
void vterm_tab(des_vterm* t, int v, unsigned short code)
{
	vterm_decode(t, v, code);
}
void vterm_numlock(des_vterm* t, int v, unsigned short code)
{
	if (code & 0x0080)
		return;

	vterm_global.numlock = !vterm_global.numlock;
	vkbd_leds(t->vkbd, VKBD_LED_NUMLOCK, vterm_global.numlock);
}
void vterm_capslock(des_vterm* t, int v, unsigned short code)
{
	if (code & 0x0080)
		return;

	vterm_global.capslock = !vterm_global.capslock;
	vkbd_leds(t->vkbd, VKBD_LED_CAPSLOCK, vterm_global.capslock);
}
void vterm_scrlock(des_vterm* t, int v, unsigned short code)
{
	if (code & 0x0080)
		return;

	sem_wait(t->mutex_w);
	t->scroll_lock = !t->scroll_lock;
	vkbd_leds(t->vkbd, VKBD_LED_SCROLLOCK, t->scroll_lock);
	while (t->waiting_scroll > 0) {
		sem_signal(t->scroll_sincr);
		t->waiting_scroll--;
	}
	sem_signal(t->mutex_w);
}
void vterm_sysrq(des_vterm*, int v, unsigned short code)
{ }

const int N_VTERM = 100;

des_vterm vterm[N_VTERM];
des_vterm* vterm_active;


void input_term(int h)
{
	unsigned short code;
	des_vterm *p_des;

	p_des = &vterm[h];
	vkbd_intr_enable(p_des->vkbd, true);
	vkbd_wfi(p_des->vkbd);

	for(;;) {
		code = vkbd_read(p_des->vkbd);
		
		bool ext = (code & 0xff00) == 0xe000;
		char c = (code & 0x007f);
		vterm_map* map = 0;
		if (code == KBD_PAUSE)
			; // pause
		else if (ext)
			map = vterm_emaps;
		else 
			map = vterm_maps;

		if (map && map[c].action)
			(*map[c].action)(p_des, map[c].arg, code);

		vkbd_wfi(p_des->vkbd);
	}
}

static
unsigned short vterm_mks(char c, unsigned char attr)
{
	return (((unsigned short)attr) << 8) | c;
}

static
void vterm_update_vmoncursor(des_vterm *t)
{
	vmon_setcursor(t->vmon, t->video_off - t->vmon_off);
}


static
void vterm_update_vmon(des_vterm *t, int first, int end)
{
	if (first >= t->vmon_off + t->vmon_size || end <= t->vmon_off)
	    return;
	
	if (first < t->vmon_off)
		first = t->vmon_off;

	if (end > t->vmon_off + t->vmon_size)
		end = t->vmon_off + t->vmon_size;

	unsigned int abs_first = (t->base + first) % t->video_size;
	unsigned int abs_end   = (t->base + end)   % t->video_size;
	int off = first - t->vmon_off;

	if (abs_end > abs_first) {
		vmon_write_n(t->vmon, off, &t->video[abs_first],         abs_end - abs_first);
	} else {
		int slice = t->video_size - abs_first;
		vmon_write_n(t->vmon, off,         &t->video[abs_first], slice);
		vmon_write_n(t->vmon, off + slice, &t->video[0],         abs_end);
	}
	vterm_update_vmoncursor(t);
}

static 
void vterm_redraw_vmon(des_vterm *t)
{
	vterm_update_vmon(t, t->vmon_off, t->vmon_off + t->vmon_size);
	vterm_update_vmoncursor(t);
}

static int vterm_row(const des_vterm *t, int off)
{
	return (off / t->video_max_x) * t->video_max_x;
}

static
bool vterm_make_visible(des_vterm *t, int off)
{
	if (off >= t->vmon_off && off < t->vmon_off + t->vmon_size)
		return false;

	if (off < t->vmon_off)
		t->vmon_off = vterm_row(t, off);
	else
		t->vmon_off = vterm_row(t, off) + t->video_max_x - t->vmon_size;
	if (t->vmon_off < 0)
		t->vmon_off = 0;
	return true;
}

static
void vterm_addline(des_vterm *t)
{
	for (int i = 0; i < t->video_max_x; i++) {
		t->video[t->base] = vterm_mks(' ', t->attr);
		t->base = (t->base + 1) % t->video_size;
	}
	t->video_off  -= t->video_max_x;
	t->append_off -= t->video_max_x;
	t->vmon_off   -= t->video_max_x;
	t->orig_off   -= t->video_max_x;
	t->pref_off   -= t->video_max_x;
}

static
void vterm_putcode(des_vterm *t, char c)
{
	t->video[(t->base + t->video_off) % t->video_size] = vterm_mks(c, t->attr);
	if (++t->video_off >= t->video_size)
		vterm_addline(t);
}

static
int vterm_delta_off(const des_vterm *t, int off, char c)
{
	int noff = off;
	switch (c) {
	case '\n':
		noff = off + t->video_max_x;
	case '\r':
		noff = vterm_row(t, noff);
		break;
	case '\t':
		noff = (off / t->tab + 1) * t->tab;
		break;
	case '\b':
		if (off > 0)
			noff = off - 1;
		break;
	default:
		noff = off + 1;
		break;
	}
	return noff - off;
}

static
int vterm_calc_off(const des_vterm *t, int off, const char* start, const char* end)
{
	for (; start < end; start++)
		off += vterm_delta_off(t, off, *start);
	return off;
}
	

static
void vterm_write_chars(des_vterm * t, const char vetti[], int quanti)
{
	int first, last, delta;

	first = last = t->video_off;
	for (int i = 0; i < quanti; i++) {
		char c = vetti[i];
		delta = vterm_delta_off(t, t->video_off, c);
		if (c == '\n' || c == '\t')
			c = ' ';
		if (delta > 0) {
			for (int j = 0; j < delta; j++) {
				vterm_putcode(t, c);
			}
			if (t->video_off > last)
				last = t->video_off;
		} else {
			t->video_off += delta;
			if (t->video_off < first)
				first = t->video_off;
		}
	}
	if (last > first) 
		vterm_update_vmon(t, first, last);
	if (t->video_off > t->append_off)
		t->append_off = t->video_off;
}

static
void vterm_rewrite_chars(des_vterm *t, const char vetti[], int quanti, bool visible)
{
	char c = ' ';
	int save1 = t->video_off;
	vterm_write_chars(t, vetti, quanti);
	int save2 = t->video_off;
	while (t->video_off < t->append_off)
		vterm_write_chars(t, &c, 1);
	vterm_move_cursor(t, save1);
	t->append_off = save2;
	if (visible && vterm_make_visible(t, t->append_off))
		vterm_redraw_vmon(t);
}


void vterm_setcolor(int term, int fgcol, int bgcol, bool blink)
{
	if(term < 0 || term >= N_VTERM) {
		flog(LOG_WARN, "vterm: terminale inesitente: %d", term);
		return;
	}

	des_vterm *t = &vterm[term];

	t->attr = (fgcol & 0xff) | (bgcol & 0x7f) << 8 | (blink ? 0x80 : 0x00);
}

static
void vterm_move_cursor(des_vterm *t, int off)
{
	t->video_off = off;
	if (vterm_make_visible(t, t->video_off)) 
		vterm_redraw_vmon(t);
	else
		vterm_update_vmoncursor(t);
	if (t->video_off > t->append_off)
		t->append_off = t->video_off;
}



void writevterm_n(int term, const char vetti[], int quanti)
{
	des_vterm *p_des;

	if(term < 0 || term >= N_VTERM) {
		flog(LOG_WARN, "vterm: terminale inesitente: %d", term);
		return;
	}

	des_vterm* t = &vterm[term];

	sem_wait(t->mutex_w);

	while (t->scroll_lock) {
		t->waiting_scroll++;
		sem_signal(t->mutex_w);
		sem_wait(t->scroll_sincr);
		sem_wait(t->mutex_w);
	}

	vterm_move_cursor(t, t->append_off);

	vterm_write_chars(t, vetti, quanti);
	if (vterm_make_visible(t, t->video_off)) 
		vterm_redraw_vmon(t);

	if (t->funzione != none && t->echo) {
		t->orig_off = t->video_off;
		t->orig_punt += t->cont;
		t->pref_off = t->orig_off;
		t->punt = t->orig_punt;
		t->orig_cont -= t->cont;
		t->letti += t->cont;
		t->cont = 0;
	}
	sem_signal(t->mutex_w);
}

static void vterm_input_status(des_vterm *t, vterm_edit_status* stat)
{
	int quanti = t->orig_cont;
	int validi = (stat->validi > quanti ? quanti : stat->validi);
	int cursore = (stat->cursore > validi ? validi : stat->cursore);
	if (cursore < 0)
		cursore = 0;
	sem_wait(t->mutex_w);
	vterm_write_chars(t, t->orig_punt, cursore);
	vterm_rewrite_chars(t, t->orig_punt + cursore, validi - cursore);
	t->vmon_off = vterm_row(t, t->video_off - stat->posizione);
	if (t->vmon_off < 0)
		t->vmon_off = 0;
	vterm_redraw_vmon(t);
	sem_signal(t->mutex_w);
	t->cont = validi;
	t->punt += cursore;
}

static void vterm_output_status(des_vterm *t)
{
	t->stat.validi = t->letti + t->cont;
	t->stat.cursore = t->punt - t->orig_punt;
	t->stat.posizione = t->video_off - t->vmon_off;
}


static
void startvterm_in(des_vterm *p_des, char vetti[], int quanti, funz op, struct vterm_edit_status* stat)
{
	p_des->orig_cont = quanti;
	if (op == input_ln)
		p_des->orig_cont--;
	p_des->letti = p_des->cont = 0;
	p_des->orig_punt = p_des->punt = vetti;
	p_des->funzione = op;
	p_des->insert = true;
	p_des->echo = true;
	p_des->pref_off = p_des->orig_off = p_des->video_off;
	if (stat == VTERM_NOECHO)
		p_des->echo = false;
	else if (stat && stat->validi > 0)
		vterm_input_status(p_des, stat);
}


int readvterm_n(int term, char vetti[], int quanti, struct vterm_edit_status* stat)
{
	des_vterm *p_des;
	int letti;

	if(term < 0 || term >= N_VTERM)
		return 0;

	if (quanti <= 0)
		return 0;

	p_des = &vterm[term];
	sem_wait(p_des->mutex_r);
	startvterm_in(p_des, vetti, quanti, input_n, stat);
	sem_wait(p_des->sincr);
	letti = p_des->letti;
	if (stat) 
		*stat = p_des->stat;
	sem_signal(p_des->mutex_r);
	return letti;
}

int readvterm_ln(int term, char vetti[], int quanti, struct vterm_edit_status* stat)
{
	des_vterm *p_des;
	int letti;

	if(term < 0 || term >= N_VTERM)

	if (quanti <= 0)
		return 0;

	p_des = &vterm[term];
	sem_wait(p_des->mutex_r);
	startvterm_in(p_des, vetti, quanti, input_ln, stat);
	sem_wait(p_des->sincr);
	letti = p_des->letti;
	if (stat) 
		*stat = p_des->stat;
	sem_signal(p_des->mutex_r);
	return letti;
}


void vterm_switch(int v)
{
	if (v < 0 || v >= N_VTERM)
		return;
	des_vterm *t = &vterm[v];
	if (t->num < 0)
		return;
	vterm_active = t;
	vkbd_leds(vterm_active->vkbd, VKBD_LED_NUMLOCK,  vterm_global.numlock);
	vkbd_leds(vterm_active->vkbd, VKBD_LED_CAPSLOCK, vterm_global.capslock);
	vkbd_switch(vterm_active->vkbd);
	vmon_switch(vterm_active->vmon);
}

bool vterm_setresident(int term)
{
	if(term < 0 || term >= N_VTERM)
		return false;

	des_vterm *t = &vterm[term];

	unsigned int size = t->video_size * sizeof(*t->video);
	unsigned int rsize = resident(t->video, size);

	if (rsize < size) {
		flog(LOG_WARN, "vterm: residenti %d/%d byte", rsize, size);
		return false;
	}

	return true;
}

void vterm_clear(int term)
{
	if(term < 0 || term >= N_VTERM)
		return;
	
	des_vterm *t = &vterm[term];


	sem_wait(t->mutex_w);
	if (t->funzione == none) {
		vterm_setcolor(term, COL_LIGHTGRAY, COL_BLACK);
		t->base = 0;
		t->video_off = t->append_off = t->vmon_off = 0;
		for (int j = 0; j < t->video_size; j++)
			t->video[j] = vterm_mks(' ', t->attr);
		vterm_redraw_vmon(t);
	}
	sem_signal(t->mutex_w);
}

// inizializzazione
bool vterm_init()
{
	short id;
	unsigned int maxx = 80, maxy = 25;
	int i;

	for (i = 0; i < N_VTERM; i++) {
		des_vterm* p_des = &vterm[i];
		p_des->num = -1;
	}

	for (i = 0; i < N_VTERM; i++) {
		des_vterm* p_des = &vterm[i];

		p_des->num = i;
		if (!vmon_getsize(i, maxx, maxy))
			break;

		if ( (p_des->mutex_r = sem_ini(1)) == 0) {
			flog(LOG_WARN, "vterm: impossibile creare mutex_r %d", i);
			return false;
		}
		if ( (p_des->mutex_w = sem_ini(1)) == 0) {
			flog(LOG_WARN, "vterm: impossibile creare mutex_w %d", i);
			return false;
		}
		if ( (p_des->sincr = sem_ini(0)) == 0) {
			flog(LOG_WARN, "vterm: impossibile creare sincr %d", i);
			return false;
		}
		p_des->video_max_x = maxx;
		p_des->video_max_y = maxy * 10;
		p_des->video_size = p_des->video_max_x * p_des->video_max_y;
		p_des->vmon_size  = p_des->video_max_x * maxy;
		if ( (p_des->video = (unsigned short*)mem_alloc(p_des->video_size * sizeof(short))) == 0) {
			flog(LOG_WARN, "vterm: memoria terminata");
			return false;
		}
		if ( (p_des->id = activate_p(input_term, i, 100, LIV_UTENTE)) == 0) {
			flog(LOG_WARN, "vterm: impossibile creare processo %d", i);
			return false;
		}

		p_des->scroll_lock = false;
		p_des->waiting_scroll = 0;
		if ( (p_des->scroll_sincr = sem_ini(0)) == 0) {
			flog(LOG_WARN, "vterm: impossibile creare scroll_sincr %d", i);
			return false;
		}
		p_des->vkbd = i;
		p_des->vmon = i;
		p_des->funzione = none;
		p_des->tab = 8;
		vterm_clear(i);

	}
	flog(LOG_INFO, "vterm: creati %d terminali virtuali", i);

	vterm_switch(0);
	return true;
}

bool lib_init()
{
	return vterm_init();
}
// log formattato
void flog(log_sev sev, const char *fmt, ...)
{
	va_list ap;
	char buf[LOG_MSG_SIZE];

	va_start(ap, fmt);
	int l = vsnprintf(buf, LOG_MSG_SIZE, fmt, ap);
	va_end(ap);

	log(sev, buf, l);
}
