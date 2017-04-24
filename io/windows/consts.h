#ifndef GRCONSTS_H
#define GRCONSTS_H

#include "tipo.h"

//profondità di colore (BPP_8 or BPP_32)
#define BPP_8

#if !defined(BPP_8) && !defined(BPP_32)
#error You must define video color depth (BPP_8 or BPP_32)
#endif

#if defined BPP_8
	typedef natb PIXEL_UNIT;
	#define VBE_DISPI_BPP 0x08;
#elif defined BPP_32
	typedef natq PIXEL_UNIT;
	#define VBE_DISPI_BPP 0x20;
#endif

#define LABEL_MAX_TEXT 40
#define BUTTON_MAX_TEXT 10

// constanti per gr_window
#define TOPBAR_HEIGHT 20
#define CLOSEBUTTON_PADDING_X 1
#define CLOSEBUTTON_PADDING_Y 1
#define TITLELABEL_PADDING_X 2
#define TITLELABEL_PADDING_Y 2

#if defined BPP_8
	#define DEFAULT_WIN_BACKCOLOR 0x1e
	#define TOPBAR_WIN_BACKCOLOR 0x35
	#define CLOSEBUTTON_WIN_BACKCOLOR 0x04
#elif defined BPP_32
	#define DEFAULT_WIN_BACKCOLOR 0x1e
	#define TOPBAR_WIN_BACKCOLOR 0x35
	#define CLOSEBUTTON_WIN_BACKCOLOR 0x04
#endif

#endif