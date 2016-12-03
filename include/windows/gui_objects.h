extern "C" void __cxa_pure_virtual()
{
	flog(LOG_INFO, "Pure Virtual function called, aborting.");
}

const natb W_ID_LABEL=0;
const natb W_ID_BUTTON=1;

//-----	UTENTE (Il codice di questa gerarchia di classi è replicato anche
//		nel file hello1.in, andrebbe fatto un file header comune)
enum user_event_type {NOEVENT, USER_EVENT_MOUSEZ, USER_EVENT_MOUSEUP, USER_EVENT_MOUSEDOWN};
enum mouse_button {LEFT,MIDDLE,RIGHT};
class u_windowObject
{
	public:
	natb TYPE;
	unsigned short size_x;
	unsigned short size_y;
	short pos_x;
	short pos_y;
	short z_index;

	natb backColor;

	virtual void process_event(user_event_type type)=0;
};

class u_button : public u_windowObject
{
	public:
	char text[20];
	natb borderColor;
	u_button()
	{
		TYPE=W_ID_BUTTON;
	}

	void process_event(user_event_type type)
	{

	}
};

class u_label : public u_windowObject
{
	public:
	char text[100];
	u_label()
	{
		TYPE=W_ID_LABEL;
	}

	void process_event(user_event_type type)
	{
		
	}
};

//-------- SISTEMA

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

	button(unsigned short size_x, unsigned short size_y, short pos_x, short pos_y, short z_index, natb backColor, natb borderColor, const char * text)
	 : windowObject(size_x, size_y, pos_x, pos_y, z_index, backColor)
	{
		copy(text, this->text);
		this->borderColor=borderColor;
	}

	button(u_button * u_b) : windowObject(u_b->size_x, u_b->size_y, u_b->pos_x, u_b->pos_y, u_b->z_index, u_b->backColor)
	{
		copy(u_b->text, this->text);
		this->borderColor=u_b->borderColor;
	}

	void render()
	{
		flog(LOG_INFO, "Rendering button...");
		gr_memset_safe_onobject(render_buff, backColor, size_x*size_y);
		int text_width = get_fontstring_width(this->text);
		this->set_fontstring((this->size_x - text_width)/2, (this->size_y - 16)/2, text_width, this->size_y,this->text, this->backColor);

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
