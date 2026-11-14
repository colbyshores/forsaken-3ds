/* quadidx.h — shared quad index table for the mono-gated billboard
 * path (TF04-A revisit). Pure C99, host-testable.
 *
 * Billboard poly fills emit one texgroup per poly: 4 unique verts,
 * 2 triangles with local indices (0,1,2),(0,2,3). Converting the
 * 6 expanded verts per quad wastes a third of billboard conversion;
 * converting the 4 unique verts and drawing with this shared table
 * produces the same triangle stream. The table is pattern-fixed:
 * quad q contributes (4q, 4q+1, 4q+2), (4q, 4q+2, 4q+3).
 */
#ifndef QUADIDX_INCLUDED
#define QUADIDX_INCLUDED

/* Batch cap: table bytes = MAX_QUADS * 6 * 2 (48 KiB at 4096). */
#define QUADIDX_MAX_QUADS 4096

/* True iff idx[0..5] is exactly the canonical quad pattern. */
int quadidx_is_quad4(const unsigned short *idx);

/* Fill table[0 .. numQuads*6) with the shared pattern. */
void quadidx_build(unsigned short *table, int numQuads);

#endif /* QUADIDX_INCLUDED */
