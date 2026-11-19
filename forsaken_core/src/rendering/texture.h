#pragma once

/* texture.h -- seam between the renderers and the image decoder
 *
 * load_image() decodes image->path into 8-bit RGBA (data is w*h*4 bytes, freed
 * by destroy_image). texture_png.cpp implements it for both builds.
 * colorkey is set for images without an alpha channel, telling the renderer to
 * treat pure black as transparent. The PNG loader ignores the mipmap argument.
 */

struct texture_image_t {
	int id;			// internal id
	int w;			// width
	int h;			// height
	int size;		// bytes
	int colorkey;	// bool - requires colorkeying
	char path[256];	// image path
	char * data;	// image data (bytes)
};

int load_image( texture_image_t * image, int mipmap ); //, float gamma );
void destroy_image( texture_image_t * image );
