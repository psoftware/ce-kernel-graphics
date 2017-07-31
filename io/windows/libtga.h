#include "libce_guard.h"
#include "consts.h"

struct tga_header {
	natb image_id_length;

	// color map types
	static const natb HASNT_COLOR_MAP = 0;
	static const natb HAS_COLOR_MAP = 1;
	natb color_map_type;

	// image type
	static const natb NO_IMAGE_DATA = 0;
	static const natb UNCOMPRESSED_WITH_COLOR_MAP = 1;
	static const natb UNCOMPRESSED_TRUE_COLOR = 2;
	static const natb UNCOMPRESSED_BW = 3;
	static const natb ENCODED_WITH_COLOR_MAP = 9;
	static const natb ENCODED_TRUE_COLOR = 10;
	static const natb ENCODED_BW = 11;
	natb image_type;

	// color map fields
	natw color_map_first_entry_index;
	natw color_map_length;
	natb color_map_entry_size;

	// image descriptor fields
	natw x_origin;
	natw y_origin;
	natw width;
	natw height;
	natb depth;

	static const natb DESCR_DIRECTION_MASK = 3u << 4;	// bit 5-4
	static const natb DESCR_ALPHADEPTH_MASK = 15u;	// bit 3-0
	natb img_descriptor;	//Image descriptor (1 byte): bits 3-0 give the alpha channel depth, bits 5-4 give direction
} __attribute__((packed));


// opzionale
struct tga_footer {
	natb ext_offset[4];
	natb dev_area_offset[4];
	char signature[16];
	char dot;
	char null_val;
} __attribute__((packed));


class TgaParser {
	unsigned char *src_file;
	tga_header* header;
	unsigned char *img_data;

public:
	TgaParser(void *src_file);
	bool is_valid();
	int get_width();
	int get_height();
	int get_depth();
	void to_bitmap(void *dest);
};