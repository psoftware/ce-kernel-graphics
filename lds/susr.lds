SECTIONS
{
	.text 0 : AT(0)
	{
		_u_text = . ;
		*(.text)
		*(.rodata)
	}
	.data : AT(_u_data - _u_text)
	{
		_u_data = . ;
		*(.data)
		*(.bss)
		*(.COMMON)
	}
}
