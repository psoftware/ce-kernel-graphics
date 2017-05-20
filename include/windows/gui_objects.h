#include "virtual.h"
#include "windows/u_obj.h"
#include "../../io/windows/font.h"

//-------- SISTEMA
/*
class windowObject
{
	public:
	unsigned short size_x;
	unsigned short size_y;
	short pos_x;
	short pos_y;
	short z_index;

	natb backColor;

	public:
	bool is_rendered;
	natb * render_buff;

	windowObject(unsigned short size_x, unsigned short size_y, short pos_x, short pos_y, short z_index, natb backColor)
	{
		this->size_x=size_x;
		this->size_y=size_y;
		this->pos_x=pos_x;
		this->pos_y=pos_y;
		this->z_index=z_index;
		this->backColor=backColor;
		this->is_rendered=false;
		this->render_buff = new natb[size_x*size_y];
	}

	virtual ~windowObject()
	{
		delete[] render_buff;
	}

	void set_pixel(int x, int y, natb col)
	{
		if(x<this->size_x && y<this->size_y && x>=0 && y>=0)
			this->render_buff[size_x*y+x] = col;
		else
			flog(LOG_INFO, "windowObject set_pixel: buffer overflow detected!");
	}

	void *gr_memset_safe_onobject(void *__restrict dest, const natb b, unsigned long int n)
	{
		if((static_cast<natb*>(dest)+n) > this->render_buff+(this->size_x*this->size_y) || static_cast<natb*>(dest) < this->render_buff)
		{
			flog(LOG_INFO, "gr_memcpy_safe_onobject: anticipato buffer overflow, salto scrittura");
			return 0;
		}

		char *s1 = static_cast<char*>(dest);
		for(; 0<n; --n)*s1++ = b;
		return dest;
	}

	int set_fontchar(int x, int y, int nchar, natb backColor)
	{
		int row_off = nchar / 16;
		int col_off = nchar % 16;

		natb start = (16 - font_width[nchar])/2;
		natb end = start + font_width[nchar];

		for(int i=0; i<16; i++)
			for(int j=start; j<end; j++)
				if(font_bitmap[(row_off*16 + i)*256 + (j + col_off*16)] == 0x00)
					this->set_pixel(x+j-start, y+i, backColor);
				else
					//this->set_pixel(x+j-start, y+i, font_bitmap[(row_off*16 + i)*256 + (j + col_off*16)] & 15);
					this->set_pixel(x+j-start, y+i, 0x00);

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
	}

	int get_charfont_width(char c)
	{
		if(c=='\n' || c=='\0')
			return 0;
		else
			return font_width[(int)c];
						
	}

	void set_fontstring(int x, int y, int bound_x, int bound_y, const char * str, natb backColor)
	{
		int slength = (int)strlen(str);
		//int x_eff = ((i*16) % bound_x);
		//int y_eff = (i*16 / bound_x)*16;
		int x_eff = 0;
		int y_eff = 0;
		for(int i=0; i<slength; i++)
		{	
			if(str[i] != '\n')
				x_eff += set_fontchar(x+x_eff, y+(y_eff*16), str[i], backColor);

			if(i==slength-1)
				break;

			if((x_eff + get_charfont_width(str[i+1])) > bound_x || str[i] == '\n')
			{
				x_eff = 0;
				y_eff++;
			}
		
		}
	}

	virtual void render()=0;
};

class button : public windowObject
{
	public:
	char text[20];
	natb borderColor;
	bool clicked;

	button(unsigned short size_x, unsigned short size_y, short pos_x, short pos_y, short z_index, natb backColor, natb borderColor, const char * text)
	 : windowObject(size_x, size_y, pos_x, pos_y, z_index, backColor)
	{
		copy(text, this->text);
		this->borderColor=borderColor;
		this->clicked=false;
	}

	button(u_button * u_b) : windowObject(u_b->size_x, u_b->size_y, u_b->pos_x, u_b->pos_y, u_b->z_index, u_b->backColor)
	{
		copy(u_b->text, this->text);
		this->borderColor=u_b->borderColor;
		this->clicked=u_b->clicked;
	}

	void render()
	{
		flog(LOG_INFO, "Rendering button...");
		int color_background = (clicked) ? 0x02 : backColor;
		gr_memset_safe_onobject(render_buff, color_background, size_x*size_y);
		int text_width = get_fontstring_width(this->text);
		this->set_fontstring((this->size_x - text_width)/2, (this->size_y - 16)/2, text_width, this->size_y,this->text, color_background);

		gr_memset_safe_onobject(render_buff, borderColor, size_x);
		for(natw y=1; y < this->size_y-1; y++)
		{
			this->set_pixel(0, y, borderColor);
			this->set_pixel(size_x-1, y, borderColor);
		}
		gr_memset_safe_onobject(render_buff+size_x*(size_y-1), borderColor, size_x);

		this->is_rendered=true;
	}
};

class label : public windowObject
{
	public:
	char text[100];

	label(unsigned short size_x, unsigned short size_y, short pos_x, short pos_y, short z_index, natb backColor, const char * text)
	 : windowObject(size_x, size_y, pos_x, pos_y, z_index, backColor)
	{
		copy(text, this->text);
	}

	label(u_label * u_l) : windowObject(u_l->size_x, u_l->size_y, u_l->pos_x, u_l->pos_y, u_l->z_index, u_l->backColor)
	{
		copy(u_l->text, this->text);
	}
	
	void render()
	{
		flog(LOG_INFO, "Rendering label...");
		gr_memset_safe_onobject(render_buff, backColor, size_x*size_y);
		this->set_fontstring(0, 0, this->size_x, this->size_y, this->text, this->backColor);
		this->is_rendered=true;
	}
};

class textbox : public windowObject
{
	public:
	char text[100];

	textbox(unsigned short size_x, unsigned short size_y, short pos_x, short pos_y, short z_index, natb backColor, const char * text)
	 : windowObject(size_x, size_y, pos_x, pos_y, z_index, backColor)
	{
		copy(text, this->text);
	}

	textbox(u_textbox * u_t) : windowObject(u_t->size_x, u_t->size_y, u_t->pos_x, u_t->pos_y, u_t->z_index, u_t->backColor)
	{
		copy(u_t->text, this->text);
	}

	void render()
	{
		flog(LOG_INFO, "Rendering textbox...");
		gr_memset_safe_onobject(render_buff, backColor, size_x*size_y);
		this->set_fontstring(2, 2, this->size_x-2,this->size_y-2, this->text, backColor);

		gr_memset_safe_onobject(render_buff, 0x00, size_x);
		for(natw y=1; y < this->size_y-1; y++)
		{
			this->set_pixel(0, y, 0x00);
			this->set_pixel(size_x-1, y, 0x00);
		}
		gr_memset_safe_onobject(render_buff+size_x*(size_y-1), 0x00, size_x);

		this->is_rendered=true;
	}
};*/