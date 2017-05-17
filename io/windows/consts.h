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

// constanti per gr_window
#define MAINBAR_SIZE 40
#define MAINBAR_ZINDEX 10
#define TOPBAR_HEIGHT 20
#define CLOSEBUTTON_PADDING_X 1
#define CLOSEBUTTON_PADDING_Y 1
#define TITLELABEL_PADDING_X 2
#define TITLELABEL_PADDING_Y 2
#define BORDER_TICK 1

#if defined BPP_8
	#define WIN_BACKGROUND_COLOR 0x36
	#define DEFAULT_WIN_BACKCOLOR 0x1e
	#define TOPBAR_WIN_BACKCOLOR 0x35
	#define CLOSEBUTTON_WIN_BACKCOLOR 0x04
	#define MAINBAR_COLOR 0x20
#elif defined BPP_32
	#define WIN_BACKGROUND_COLOR 0x005cbbff
	#define DEFAULT_WIN_BACKCOLOR 0x002ecc71
	#define TOPBAR_WIN_BACKCOLOR 0x0027ae60
	#define CLOSEBUTTON_WIN_BACKCOLOR 0x00e00000
	#define MAINBAR_COLOR 0x00ffffff
#endif

#endif
