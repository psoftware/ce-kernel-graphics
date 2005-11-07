addr = 0x198000;

SECTIONS
{
	.text addr : AT(addr)
	{
		_text = . ;
		_io_text = . ;
		*(.text)
		*(.rodata)
		. = ALIGN(4096);
	}
	.data : AT(addr + (_data - _text))
	{
		_data = . ;
		*(.data)
		. = ALIGN(4096);
		_bss = . ;
		*(.bss)
		*(COMMON)
		. = ALIGN(4096);
		_io_end = . ;
		_end = . ;
	}
}
