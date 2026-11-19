/* texture_png.cpp -- libpng decoder behind load_image / destroy_image; see texture.h.
 *
 * Accepts 8-bit RGB or RGBA PNGs only (grey, palette and 16-bit images
 * return -1). RGB images get an opaque alpha channel added and colorkey set.
 */
#include "game/main.h"
#include <stdio.h>
#include <string.h>
#include <png.h>

#include "rendering/texture.h"
#include "data/file.h"

void destroy_image( texture_image_t * image )
{
	free(image->data);
	memset(image,0,sizeof(*image));
}

int load_image( texture_image_t * image, int /*mipmap*/ )
{
  png_byte magic[8];
  png_structp png_ptr = NULL;
  png_infop info_ptr = NULL;
  int bit_depth, color_type;
  /* Read on the longjmp path below: volatile keeps their values there
     (row_pointers and data are assigned after setjmp). */
  FILE * volatile fp = NULL;
  png_bytep * volatile row_pointers = NULL;
  char * volatile data = NULL;
  int i;

  /* open image file */
  fp = file_open (image->path, "rb");
  if (!fp)
	{
	  fprintf (stderr, "error: couldn't open \"%s\"!\n", image->path);
	  return -1;
	}

  /* read magic number */
  if (fread (magic, 1, sizeof (magic), fp) != sizeof (magic)
	  || !png_check_sig (magic, sizeof (magic)))
	{
	  fprintf (stderr, "error: \"%s\" is not a valid PNG image!\n",
		   image->path);
	  goto fail;
	}

  /* create a png read struct */
  png_ptr = png_create_read_struct
	(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_ptr)
	goto fail;

  /* create a png info struct */
  info_ptr = png_create_info_struct (png_ptr);
  if (!info_ptr)
	goto fail;

  /* a libpng error longjmps back here */
  if (setjmp (png_jmpbuf (png_ptr)))
	goto fail;

  /* setup libpng for using standard C fread() function
	 with our FILE pointer */
  png_init_io (png_ptr, fp);

  /* tell libpng that we have already read the magic number */
  png_set_sig_bytes (png_ptr, sizeof (magic));

  /* read png info */
  png_read_info (png_ptr, info_ptr);

  /* get some usefull information from header */
  bit_depth = png_get_bit_depth (png_ptr, info_ptr);
  color_type = png_get_color_type (png_ptr, info_ptr);

  // 8 bits per pixel
  if (bit_depth != 8)
	  goto fail;

  /* detect if not rgb */
  if (!(color_type & PNG_COLOR_MASK_COLOR))
	  goto fail;

  /* "colorkey" here is the flag the downstream create_texture()
   * consults to decide whether to replace pure-black RGB pixels with
   * alpha=0. Forsaken's classic UI atlases (VDU font, HUD sheets)
   * ship as RGB PNGs with black backgrounds representing
   * transparency — the original 90s-era convention. We want those to
   * be color-keyed. PNGs that already carry a real alpha channel
   * carry their own transparency and do not need the black-key pass.
   *
   * Previously this was written as
   *   image->colorkey = (color_type & PNG_COLOR_MASK_ALPHA);
   * which set the flag only when the PNG already had alpha — the
   * exact opposite of what the downstream loop needs. Fonts loaded
   * as fully-opaque black-on-green and rendered invisible over the
   * in-scene VDU panel (green on green with black background
   * additively blended = no visible characters). */
  {
	int has_alpha = (color_type & PNG_COLOR_MASK_ALPHA) != 0;
	image->colorkey = !has_alpha;
	if (!has_alpha)
	  png_set_add_alpha(png_ptr, (png_uint_32)255, PNG_FILLER_AFTER);
  }

  /* update info structure to apply transformations */
  png_read_update_info (png_ptr, info_ptr);

  /* retrieve updated information */
  png_get_IHDR (png_ptr, info_ptr,
		(png_uint_32*)(&image->w),
		(png_uint_32*)(&image->h),
		&bit_depth, &color_type,
		NULL, NULL, NULL);

  if(color_type != PNG_COLOR_TYPE_RGB_ALPHA)
	  goto fail;

  /* we can now allocate memory for storing pixel data - 4 = rgba */
  data = (char*)malloc (sizeof (char) * image->w * image->h * 4);

  /* setup a pointer array.  Each one points at the begening of a row. */
  row_pointers = (png_bytep *)malloc (sizeof (png_bytep) * image->h);

  if (!data || !row_pointers)
	{
	  fprintf (stderr, "error: out of memory decoding \"%s\"!\n", image->path);
	  goto fail;
	}

  /* PNG rows are stored top to bottom. */
  for (i = 0; i < image->h; ++i)
	  row_pointers[i] = (png_bytep)(data +
	  (i * image->w * 4));

  /* read pixel data using row pointers */
  png_read_image (png_ptr, row_pointers);

  /* finish decompression and release memory */
  png_read_end (png_ptr, NULL);
  png_destroy_read_struct (&png_ptr, &info_ptr, NULL);

  /* we don't need row pointers anymore */
  free (row_pointers);

  fclose (fp);
  image->data = data;
  return 0;

fail:
  png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
  free (row_pointers);
  free (data);
  fclose (fp);
  return -1;
}

