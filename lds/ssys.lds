SECTIONS
{
	.text 0 : AT(0)
	{
		_s_text = . ;
		*(.text)
		*(.rodata)
	}
	.data : AT(_s_data - _s_text)
	{
		_s_data = . ;
		*(.data)
		*(.bss)
		*(.COMMON)
	}
}
