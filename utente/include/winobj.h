extern "C" void __cxa_pure_virtual()
{
	flog(LOG_INFO, "Pure Virtual function called, aborting.");
}

void * operator new(long unsigned int n)
{
	return mem_alloc(n);
}

const natb W_ID_LABEL=0;
const natb W_ID_BUTTON=1;

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
};

class u_label : public u_windowObject
{
	public:
	char text[100];
	u_label()
	{
		TYPE=W_ID_LABEL;
	}
};

