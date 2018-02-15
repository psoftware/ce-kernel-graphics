#include "gr_gaussblur.h"
#include "consts.h"

gr_gaussblur::gr_gaussblur(int pos_x, int pos_y, int size_x, int size_y, int z_index)
: gr_object(pos_x, pos_y, size_x, size_y, z_index, false)
{

}

void gr_gaussblur::build_render_areas(render_subset_unit *parent_restriction, gr_object *target, int ancestors_offset_x, int ancestors_offset_y, bool ancestor_modified)
{
	//render_subset_unit *newunit = new render_subset_unit(0, 0, size_x, size_y);
	//push_render_unit(newunit);
}

void gr_gaussblur::render()
{
	if(!buffered)
		return;

	// fai le modifiche sul buffer
	render_subset_unit objarea(0,0,size_x,size_y);
	draw(this->buffer, size_x, size_y, 0,0, &objarea);

	// indico l'area modificata
	render_subset_unit *newunit = new render_subset_unit(0, 0, size_x, size_y);
	push_render_unit(newunit);
}

void gr_gaussblur::draw(PIXEL_UNIT *ancestor_buffer, int ancestor_size_x, int ancestor_size_y, int total_offset_x, int total_offset_y, render_subset_unit *child_restriction)
{
	PIXEL_UNIT *area_result = new PIXEL_UNIT[child_restriction->size_x*child_restriction->size_y];
	//for(int y=0; y<child_restriction->size_y; y++)
	//	gr_memcpy(row_unmodified + y*child_restriction->size_x, ancestor_buffer + child_restriction->pos_x + (y+child_restriction->pos_y)*ancestor_size_x, child_restriction->size_x);

	const int BLUR_BOX_HALF_SIZE = 3;
	const int KERNEL_MULTIPLIER = 12;
	for(int y=0; y<child_restriction->size_y; y++)
	{
		for(int x=0; x<child_restriction->size_x; x++)
		{	// algoritmo per realizzare l'alpha blending
			natw sum=0;
			PIXEL_UNIT dst_pixel = *(ancestor_buffer + child_restriction->pos_x + x + (y+child_restriction->pos_y)*ancestor_size_x);
			//natb alpha = src_pixel >> 24;
			natl dst_red = (dst_pixel >> 16) & 0xff;
			natl dst_green = (dst_pixel >> 8) & 0xff;
			natl dst_blue = dst_pixel & 0xff;

			int center_pixel_multiplier = BLUR_BOX_HALF_SIZE*KERNEL_MULTIPLIER;
			dst_red*=center_pixel_multiplier;
			dst_green*=center_pixel_multiplier;
			dst_blue*=center_pixel_multiplier;
			sum=center_pixel_multiplier;

			for(int line=BLUR_BOX_HALF_SIZE*-1+1; line<BLUR_BOX_HALF_SIZE; line++)
				if(child_restriction->pos_y - total_offset_x - line < 0 || y!=child_restriction->size_y-line)
				{
					for(int i=0; i<BLUR_BOX_HALF_SIZE; i++)
						if(x!=child_restriction->size_x-i)
						{
							natb multiplier = (BLUR_BOX_HALF_SIZE-i)*KERNEL_MULTIPLIER;
							PIXEL_UNIT next_pixel = *(ancestor_buffer + child_restriction->pos_x + x + i + (y+line+child_restriction->pos_y)*ancestor_size_x);
							natb next_red = (next_pixel >> 16) & 0xff;
							natb next_green = (next_pixel >> 8) & 0xff;
							natb next_blue = next_pixel & 0xff;
							dst_red+=multiplier*next_red;
							dst_green+=multiplier*next_green;
							dst_blue+=multiplier*next_blue;
							sum+=multiplier;
						}
					for(int i=BLUR_BOX_HALF_SIZE; i>0; i--)
						if(child_restriction->pos_x - total_offset_x - i != 0)
						{
							natb multiplier = (BLUR_BOX_HALF_SIZE-i)*KERNEL_MULTIPLIER;
							//PIXEL_UNIT previous_pixel = *(row_unmodified + x + y*child_restriction->size_x - i);
							PIXEL_UNIT previous_pixel = *(ancestor_buffer + child_restriction->pos_x + x - i + (y+line+child_restriction->pos_y)*ancestor_size_x);
							natb previous_red = (previous_pixel >> 16) & 0xff;
							natb previous_green = (previous_pixel >> 8) & 0xff;
							natb previous_blue = previous_pixel & 0xff;
							dst_red+=multiplier*previous_red;
							dst_green+=multiplier*previous_green;
							dst_blue+=multiplier*previous_blue;
							sum+=multiplier;
						}
				}

			dst_red/=sum;
			dst_green/=sum;
			dst_blue/=sum;

			//*(ancestor_buffer + child_restriction->pos_x + x + (child_restriction->pos_y+y)*ancestor_size_x) = (dst_pixel & 0xff000000) | (dst_red << 16) | (dst_green << 8) | dst_blue;
			*(area_result + x + y*child_restriction->size_x) = (dst_pixel & 0xff000000) | (dst_red << 16) | (dst_green << 8) | dst_blue;

			/* //alpha blending debug code
			if(alpha!=0 && alpha!=0xff && false)
			{
				flog(LOG_INFO, "src %p dst %p newdest %p", src_pixel, dst_pixel, *(ancestor_buffer + child_restriction->pos_x + x + (y+child_restriction->pos_y)*ancestor_size_x));
				flog(LOG_INFO, "alpha %d src_red %d src_green %d src_blue %d", alpha, src_red, src_green, src_blue);
				flog(LOG_INFO, "PRE  dst_red %d dst_green %d dst_blue %d", (dst_pixel >> 16) & 0xff, (dst_pixel >> 8) & 0xff, dst_pixel & 0xff);
				flog(LOG_INFO, "POST dst_red %d dst_green %d dst_blue %d", dst_red, dst_green, dst_blue);
			}*/
		}
	}

	for(int y=0; y<child_restriction->size_y; y++)
		gr_memcpy(ancestor_buffer + child_restriction->pos_x + (y+child_restriction->pos_y)*ancestor_size_x, area_result + y*child_restriction->size_x, child_restriction->size_x);

	delete area_result;
}