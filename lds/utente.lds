addr = 0x02000000;

SECTIONS
{
	.text addr : AT(addr)
	{
		_u_text = . ;
		shared/usr.o(.text)
		shared/usr.o(.rodata)
		. = ALIGN(4096);
		_u_usr_shtext_end = . ;
		shared/sys.o(.text)
		shared/sys.o(.rodata)
		. = ALIGN(4096);
		_u_sys_shtext_end = . ;
		prog/prog.o(.text)
		prog/prog.o(.rodata)
		. = ALIGN(4096);
	}
	.data : AT(addr + (_u_data - _u_text))
	{
		_u_data = . ;
		shared/usr.o(.data)
		shared/usr.o(.bss)
		shared/usr.o(COMMON)
		. = ALIGN(4096);
		_u_usr_shdata_end = . ;
		shared/sys.o(.data)
		shared/sys.o(.bss)
		shared/sys.o(COMMON)
		. = ALIGN(4096);
		_u_sys_shdata_end = . ;
		prog/prog.o(.data)
		prog/prog.o(.bss)
		prog/prog.o(COMMON)
		. = ALIGN(4096);
		_u_end = . ;
	}
}
