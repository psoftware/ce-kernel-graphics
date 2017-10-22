#include "libce_guard.h"
#include "libgr.h"
#include "consts.h"
#include "libtga.h"
#include "resources/resources.h"

PIXEL_UNIT loaded_font_bitmap[16*16*256];
unsigned short* loaded_font_width;

bool libfont_initialized = false;

void _libfont_init()
{
	if(libfont_initialized)
		return;

	#ifdef BPP_8
	// nel caso di modalità di funzionamento a 8BPP usiamo sempre un charset di default (senza semi-trasparenza)
	gr_memcpy(loaded_font_bitmap, font_bitmap_8BPP_mecha, 16*16*256);
	loaded_font_width = tga_font_width_mecha;

	#elif defined BPP_32
	// carico la bitmap del font dal file tga indicato
	TgaParser tgp(tga_font_bitmap_segoeui);
	tgp.to_bitmap(loaded_font_bitmap);

	// carico la lista delle larghezze dei caratteri
	loaded_font_width = tga_font_width_segoeui;
	#endif

	libfont_initialized = true;
}

int set_fontchar(PIXEL_UNIT *buffer, int x, int y, int MAX_X, int MAX_Y, int offset_x, int offset_y, int limit_x, int limit_y, int nchar, PIXEL_UNIT text_color, PIXEL_UNIT backColor)
{
	PIXEL_UNIT *font_bitmap_cast = reinterpret_cast<PIXEL_UNIT*>(loaded_font_bitmap);
	int row_off = nchar / 16;
	int col_off = nchar % 16;

	natb start = (16 - loaded_font_width[nchar])/2;
	natb end = start + loaded_font_width[nchar];

	for(int i=0; i<16; i++)
		for(int j=start; j<end; j++)
		{
			if(!pixel_in_bound(x+j-start, y+i, MAX_X, MAX_Y, offset_x, offset_y, limit_x, limit_y, backColor))
				continue;

			PIXEL_UNIT font_pixel = font_bitmap_cast[(row_off*16 + i)*256 + (j + col_off*16)];
			// purtroppo c'è poco da fare, le bitmap dei font a 8PP sono fatte male (basterebbe una bitmap di booleani)
			#ifdef BPP_8
			if(font_pixel == 0x00)
				set_pixel(buffer, x+j-start, y+i, MAX_X, MAX_Y, backColor);
			else
				set_pixel(buffer, x+j-start, y+i, MAX_X, MAX_Y, text_color);
			#endif
			#ifdef BPP_32
			// se il backColor non è completamente trasparente, allora lo usiamo per lo sfondo (senza considerare la sua semi-trasparenza)
			if((backColor & 0xff000000) != 0)
				set_pixel(buffer, x+j-start, y+i, MAX_X, MAX_Y, backColor);

			// usiamo l'alpha blending perchè la bitmap dei font ci fornisce caratteri semi-trasparenti
			set_pixel_alpha_blend(buffer, x+j-start, y+i, MAX_X, MAX_Y, (font_pixel & 0xff000000) | (text_color & 0x00ffffff));
			#endif
		}

	return loaded_font_width[nchar];
}

int get_fontstring_width(const char * str)
{
	int res=0;
	for(natb i=0; i<strlen(str); i++)
		res+=loaded_font_width[(unsigned int)str[i]];
	return res;
}

int get_nextnonspecialchar_index(const char * str, int now_index, int len)
{
	for(int i=now_index+1; i<len; i++)
		if(str[i]!='\n' && str[i]!='\0')
			return i;
	return -1;
}

int get_charfont_width(char c)
{
	if(c=='\n' || c=='\0')
		return 0;
	else
		return loaded_font_width[(int)c];
					
}

void set_fontstring(PIXEL_UNIT *buffer, int MAX_X, int MAX_Y, int x, int y, int bound_x, int bound_y, int offset_x, int offset_y, int limit_x, int limit_y, const char * str, PIXEL_UNIT text_color, PIXEL_UNIT backColor, bool print_caret)
{
	int slength = (int)strlen(str);
	//int x_eff = ((i*16) % bound_x);
	//int y_eff = (i*16 / bound_x)*16;
	int x_eff = 0;
	int y_eff = 0;
	for(int i=0; i<slength; i++)
	{	
		if(str[i] != '\n')
			x_eff += set_fontchar(buffer, x+x_eff, y+(y_eff*16), MAX_X, MAX_Y, offset_x, offset_y, limit_x, limit_y, str[i], text_color, backColor);

		if(i==slength-1)
			break;

		if((x_eff + get_charfont_width(str[i+1])) > bound_x || str[i] == '\n')
		{
			x_eff = 0;
			y_eff++;
		}
	
	}

	if(!print_caret)
		return;

	const natb ASCII_CARET = 124;
	if((x_eff + get_charfont_width(ASCII_CARET)) > bound_x)
	{
		x_eff = 0;
		y_eff++;
	}
	set_fontchar(buffer, x+x_eff, y+(y_eff*16), MAX_X, MAX_Y, offset_x, offset_y, limit_x, limit_y, ASCII_CARET, text_color, backColor);
}