#ifndef GRCONSTS_H
#define GRCONSTS_H

#include "tipo.h"

//profondità di colore (BPP_8 or BPP_32)
#define BPP_32

#if !defined(BPP_8) && !defined(BPP_32)
#error You must define video color depth (BPP_8 or BPP_32)
#endif

#if defined BPP_8
	typedef natb PIXEL_UNIT;
	#define VBE_DISPI_BPP 0x08;
#elif defined BPP_32
	typedef natl PIXEL_UNIT;
	#define VBE_DISPI_BPP 0x20;
#endif

#define LABEL_MAX_TEXT 40
#define BUTTON_MAX_TEXT 10
#define TEXTBOX_MAX_TEXT 100

// costanti per gli oggetti del desktop
#define MAINBAR_SIZE 40

#define CURSOR_ZINDEX 50
#define MAINBAR_ZINDEX 11
#define WINDOWS_PLANE_ZINDEX 10
#define HEAP_LABEL_ZINDEX 1
#define BACKGROUND_ZINDEX 0

// constanti per gr_window
#define TOPBAR_HEIGHT 20
#define CLOSEBUTTON_SIZE 18
#define CLOSEBUTTON_PADDING_X 1
#define CLOSEBUTTON_PADDING_Y 1
#define CLOSEBUTTON_ZINDEX 2
#define TITLELABEL_PADDING_X 2
#define TITLELABEL_PADDING_Y 2
#define TITLELABEL_ZINDEX 1
#define BORDER_TICK 1
#define BORDER_ZINDEX 100

#if defined BPP_8
	#define WIN_BACKGROUND_COLOR 0x36
	#define DEFAULT_WIN_BACKCOLOR 0x1e
	#define TOPBAR_WIN_BACKCOLOR 0x35
	#define BORDER_WIN_COLOR 0x2b
	#define TITLELABEL_TEXTCOLOR 0x0f
	#define CLOSEBUTTON_WIN_BACKCOLOR 0x04
	#define CLOSEBUTTON_WIN_BORDERCOLOR 0x00
	#define CLOSEBUTTON_WIN_CLICKEDCOLOR 0x00
	#define CLOSEBUTTON_WIN_TEXTCOLOR 0x0F
	#define MAINBAR_COLOR 0x20

	#define LABEL_DEFAULT_TEXTCOLOR 0x01
	#define LABEL_DEFAULT_BACKCOLOR 0x36

	#define BUTTON_DEFAULT_BACKCOLOR 0x0F
	#define BUTTON_DEFAULT_BORDERCOLOR 0x00
	#define BUTTON_DEFAULT_CLICKEDCOLOR 0x03
	#define BUTTON_DEFAULT_TEXTCOLOR 0x00

	#define TEXTBOX_DEFAULT_TEXTCOLOR 0x00
	#define TEXTBOX_DEFAULT_BACKCOLOR 0x0F
	#define TEXTBOX_DEFAULT_BORDERCOLOR 0x00

	#define PROGRESSBAR_DEFAULT_COLOR 0x00
	#define PROGRESSBAR_DEFAULT_BORDERCOLOR 0x00
	#define PROGRESSBAR_DEFAULT_BACKCOLOR 0x00

#elif defined BPP_32
	#define WIN_BACKGROUND_COLOR 0xff5cbbff
	#define DEFAULT_WIN_BACKCOLOR 0xff2ecc71
	#define TOPBAR_WIN_BACKCOLOR 0xff27ae60
	#define BORDER_WIN_COLOR 0xff16a085
	#define TITLELABEL_TEXTCOLOR 0xffffffff
	#define CLOSEBUTTON_WIN_BACKCOLOR 0xffe00000
	#define CLOSEBUTTON_WIN_BORDERCOLOR 0xff27ae60
	#define CLOSEBUTTON_WIN_CLICKEDCOLOR 0xffe67e22
	#define CLOSEBUTTON_WIN_TEXTCOLOR 0xffffffff
	#define MAINBAR_COLOR 0xffffffff

	#define LABEL_DEFAULT_TEXTCOLOR 0xff000000
	#define LABEL_DEFAULT_BACKCOLOR 0xffffffff

	#define BUTTON_DEFAULT_BACKCOLOR 0xffffffff
	#define BUTTON_DEFAULT_BORDERCOLOR 0xffffffff
	#define BUTTON_DEFAULT_CLICKEDCOLOR 0xff000000
	#define BUTTON_DEFAULT_TEXTCOLOR 0xff000000

	#define TEXTBOX_DEFAULT_TEXTCOLOR 0xff000000
	#define TEXTBOX_DEFAULT_BACKCOLOR 0xffffffff
	#define TEXTBOX_DEFAULT_BORDERCOLOR 0xff000000

	#define PROGRESSBAR_DEFAULT_COLOR 0xff3498db
	#define PROGRESSBAR_DEFAULT_BORDERCOLOR 0xff000000
	#define PROGRESSBAR_DEFAULT_BACKCOLOR 0xffffffff
#endif

#endif
