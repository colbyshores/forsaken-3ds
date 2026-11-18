#pragma once

/* file_reader.h -- reads fields out of a loaded data file, at any alignment
 *
 * Forsaken's level, model and game-data files (.mxv, .mx, .mxa, .bsp, .off,
 * .trg, .gol ...) are packed byte streams: a 16-bit count sits directly in
 * front of float data, and variable-length name strings precede structures,
 * so a field's address in the loaded buffer is often only 2-byte aligned, or
 * odd. Casting the buffer pointer to float * or to a structure pointer tells
 * the compiler the data is aligned, and on the 3DS's ARM11 it then emits VFP
 * (vldr) and load-multiple (ldm) instructions, which fault on such addresses.
 *
 * A FileReader keeps a byte pointer and returns every field by value through
 * memcpy. The compiler knows nothing about the source alignment, so it only
 * uses loads that are safe at any address (single ldr/ldrh, which ARMv6K
 * performs unaligned, or bytes). Each read advances the reader past the
 * field. Multi-byte values are little-endian in the files, as on both
 * targets, so no byte swapping is needed.
 *
 * bytes() copies a whole record or array into aligned memory. Use it only
 * when the destination type is the exact on-disk layout (the record types
 * carry a static_assert on their size); everything else is read field by
 * field.
 */

#include <stddef.h>
#include <string.h>
#include <sys/types.h>

#include "math/new3d.h"

class FileReader {
public:
	explicit FileReader( const void * start ) : pos_( static_cast<const unsigned char *>( start ) ) {}

	u_int8_t  u8()  { u_int8_t v;  bytes( &v, sizeof( v ) ); return v; }
	u_int16_t u16() { u_int16_t v; bytes( &v, sizeof( v ) ); return v; }
	int16_t   i16() { int16_t v;   bytes( &v, sizeof( v ) ); return v; }
	u_int32_t u32() { u_int32_t v; bytes( &v, sizeof( v ) ); return v; }
	int32_t   i32() { int32_t v;   bytes( &v, sizeof( v ) ); return v; }
	float     f32() { float v;     bytes( &v, sizeof( v ) ); return v; }

	/* Three floats: x, y, z. */
	VECTOR    vec() { VECTOR v;    bytes( &v, sizeof( v ) ); return v; }

	/* The next 16-bit value, without moving past it. */
	u_int16_t peek_u16() const { u_int16_t v; memcpy( &v, pos_, sizeof( v ) ); return v; }

	/* Copy the next n bytes to dst. */
	void bytes( void * dst, size_t n )
	{
		memcpy( dst, pos_, n );
		pos_ += n;
	}

	/* A NUL-terminated string stored in the file; the returned pointer is
	   into the buffer (strings need no alignment). */
	const char * cstring()
	{
		const char * s = reinterpret_cast<const char *>( pos_ );
		pos_ += strlen( s ) + 1;
		return s;
	}

	void skip( size_t n ) { pos_ += n; }

	/* Current position, for loaders that hand the rest of the file to a
	   later stage. */
	const unsigned char * pos() const { return pos_; }

private:
	const unsigned char * pos_;
};
