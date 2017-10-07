#ifndef RESOURCES_H
#define RESOURCES_H

// cursore default
extern unsigned char tga_aero_arrow[];
extern unsigned int tga_aero_arrow_len;

// cursore resize
extern unsigned char tga_aero_h_resize[];
extern unsigned int tga_aero_h_resize_len;

// configurazione cursori (hot points)
extern const int main_cursor_click_x;
extern const int main_cursor_click_y;
extern const int h_resize_cursor_click_x;
extern const int h_resize_cursor_click_y;

// wallpaper
extern unsigned char tga_wallpaper[];
extern unsigned int tga_wallpaper_len;

// default 8BPP font (Mecha 8BPP)
extern unsigned char font_bitmap_8BPP_mecha[];

// font mecha
extern unsigned char tga_font_bitmap_mecha[];
extern unsigned short tga_font_width_mecha[];

// font segoeui
extern unsigned char tga_font_bitmap_segoeui[];
extern unsigned short tga_font_width_segoeui[];

// font arial
extern unsigned char tga_font_bitmap_arial[];
extern unsigned short tga_font_width_arial[];

// font verdana
extern unsigned char tga_font_bitmap_verdana[];
extern unsigned short tga_font_width_verdana[];

// font msserif
extern unsigned char tga_font_bitmap_msserif[];
extern unsigned short tga_font_width_msserif[];

#endif
