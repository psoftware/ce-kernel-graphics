#include "libce.h"
#include "libgr.h"
#include "consts.h"
#include "font.h"


int set_fontchar(PIXEL_UNIT *buffer, int x, int y, int MAX_X, int MAX_Y, int nchar, PIXEL_UNIT backColor)
{
	PIXEL_UNIT *font_bitmap_cast = reinterpret_cast<PIXEL_UNIT*>(font_bitmap);
	int row_off = nchar / 16;
	int col_off = nchar % 16;

	natb start = (16 - font_width[nchar])/2;
	natb end = start + font_width[nchar];

	for(int i=0; i<16; i++)
		for(int j=start; j<end; j++)
		{
			//purtroppo c'è poco da fare, le bitmap dei font sono fatte male
			#ifdef BPP_8
			if(font_bitmap_cast[(row_off*16 + i)*256 + (j + col_off*16)] == 0x00)
				set_pixel(buffer, x+j-start, y+i, MAX_X, MAX_Y, backColor);
			else
				set_pixel(buffer, x+j-start, y+i, MAX_X, MAX_Y, font_bitmap_cast[(row_off*16 + i)*256 + (j + col_off*16)]);
			#endif
			#ifdef BPP_32
			if(font_bitmap_cast[(row_off*16 + i)*256 + (j + col_off*16)] >> 24 < 0xff)
				set_pixel(buffer, x+j-start, y+i, MAX_X, MAX_Y, backColor);
			else
				set_pixel(buffer, x+j-start, y+i, MAX_X, MAX_Y, font_bitmap_cast[(row_off*16 + i)*256 + (j + col_off*16)]);
			#endif
		}

	return font_width[nchar];
}

int get_fontstring_width(const char * str)
{
	int res=0;
	for(natb i=0; i<strlen(str); i++)
		res+=font_width[(unsigned int)str[i]];
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
		return font_width[(int)c];
					
}

void set_fontstring(PIXEL_UNIT *buffer, int MAX_X, int MAX_Y, int x, int y, int bound_x, int bound_y, const char * str, PIXEL_UNIT backColor)
{
	int slength = (int)strlen(str);
	//int x_eff = ((i*16) % bound_x);
	//int y_eff = (i*16 / bound_x)*16;
	int x_eff = 0;
	int y_eff = 0;
	for(int i=0; i<slength; i++)
	{	
		if(str[i] != '\n')
			x_eff += set_fontchar(buffer, x+x_eff, y+(y_eff*16), MAX_X, MAX_Y, str[i], backColor);

		if(i==slength-1)
			break;

		if((x_eff + get_charfont_width(str[i+1])) > bound_x || str[i] == '\n')
		{
			x_eff = 0;
			y_eff++;
		}
	
	}
}