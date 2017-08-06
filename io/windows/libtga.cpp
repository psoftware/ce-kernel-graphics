#include "libtga.h"
#include "consts.h"
#include "libgr.h"

TgaParser::TgaParser(void* src_file) {
	this->src_file = reinterpret_cast<unsigned char*>(src_file);
	this->header = static_cast<tga_header*>(src_file);
	this->img_data = (natb*)(src_file) + header->image_id_length + 18;
}

// ATTENZIONE: la libreria supporta solo framebuffer costruiti con profondità a 32BPP. Inoltre sono supportate, per quanto
// riguarda i file TGA, solo profondità a 24 e 32 BPP e orientamento top-left e bottom-left
// questo metodo lo utilizziamo sia per controllare la correttezza del file che per individuare proprietà non supportate
bool TgaParser::is_valid() {
	natb kerneldepth;
	#ifdef BPP_8
	kerneldepth = 8;
	#elif defined BPP_32
	kerneldepth = 32;
	#endif
	natb alphadepth = (this->header->img_descriptor & tga_header::DESCR_ALPHADEPTH_MASK);
	return this->header->image_type==tga_header::UNCOMPRESSED_TRUE_COLOR && (alphadepth == 8 || alphadepth == 0)
		&& (this->header->depth == 24 || this->header->depth == 32) && kerneldepth == 32;
}

int TgaParser::get_width() {
	return this->header->width;
}

int TgaParser::get_height() {
	return this->header->height;
}

int TgaParser::get_depth() {
	return this->header->depth;
}

void TgaParser::to_bitmap(void *dest) {
	if(!is_valid())
	{
		flog(LOG_INFO, "libtga: file not valid or not supported");
		return;
	}

	natb direction = (this->header->img_descriptor & tga_header::DESCR_DIRECTION_MASK) >> 4;
	natb alphadepth = (this->header->img_descriptor & tga_header::DESCR_ALPHADEPTH_MASK);
	flog(LOG_INFO, "libtga: w %d h %d direction %d alpha %p", header->width, header->height, direction, alphadepth);

	// lo standard TGA descrive più metodi di rappresentazione della bitmap nell campo img_data,
	// in particolare si può scegliere l'angolo di partenza dalla quale si inizia a scrivere la
	// bitmap e se quanti bit è rappresentato l'alpha channel (qui supportiamo solo 0 o 8 bit)

	switch(direction) {
		case 0x00: //bottom-left (dobbiamo invertire l'ordine delle righe)
			//flog(LOG_INFO, "libtga: bottom-left");
			if(alphadepth==8 && header->depth==32)	//formato 0xAARRGGBB
				for(int r=0; r<this->header->height; r++)
					for(int c=0; c<this->header->width; c++)
							static_cast<unsigned int*>(dest)[(this->header->height-r-1)*this->header->width + c] = img_data[r*this->header->width + c];
			else if(alphadepth==0 && header->depth==24)	//formato 0xRRGGBB (24BPP)
				for(int r=0; r<this->header->height; r++)
					for(int c=0; c<this->header->width; c++)
					{
						for(int b=0; b<3; b++)	//0x??RRGGBB
							static_cast<unsigned char*>(dest)[((this->header->height-r-1)*this->header->width + c)*4 + b] = img_data[(r*this->header->width + c)*3 + b];
						static_cast<unsigned char*>(dest)[((this->header->height-r-1)*this->header->width + c)*4 + 3] = 0xFF;	// impostiamo il byte dell'aplha channel a 255
					}
		break;
		case 0x01: //bottom-right (dobbiamo invertire colonne e righe, ma bisogna capire se il pixel è 0xAARRGGBB oppure 0xBBGGRRAA)
			flog(LOG_INFO, "libtga: bottom-right not yet supported");
		break;
		case 0x10: //top-left (è la forma con la quale è costruito il framebuffer)
			//flog(LOG_INFO, "libtga: top-left");
			if(alphadepth==8 && header->depth==32)	//formato 0xAARRGGBB
				for(int r=0; r<this->header->height; r++)
					for(int c=0; c<this->header->width; c++)
							static_cast<unsigned int*>(dest)[r*this->header->width + c] = img_data[r*this->header->width + c];
			else if(alphadepth==0 && header->depth==24)	//formato 0xRRGGBB (24BPP)
				for(int r=0; r<this->header->height; r++)
					for(int c=0; c<this->header->width; c++)
					{
						for(int b=0; b<3; b++)	//0x??RRGGBB
							static_cast<unsigned char*>(dest)[(r*this->header->width + c)*4 + b] = img_data[(r*this->header->width + c)*3 + b];
						static_cast<unsigned char*>(dest)[(r*this->header->width + c)*4 + 3] = 0xFF;	// impostiamo il byte dell'aplha channel a 255
					}
		break;
		case 0x11: //top-right (dobbiamo invertire le colonne)
			flog(LOG_INFO, "libtga: top-right not yet supported");
		break;
	}
}