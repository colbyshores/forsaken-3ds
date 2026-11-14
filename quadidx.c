/* quadidx.c — see quadidx.h. No platform dependencies. */
#include "quadidx.h"

int quadidx_is_quad4(const unsigned short *idx)
{
	return idx[0] == 0 && idx[1] == 1 && idx[2] == 2 &&
	       idx[3] == 0 && idx[4] == 2 && idx[5] == 3;
}

void quadidx_build(unsigned short *table, int numQuads)
{
	int q;
	for (q = 0; q < numQuads; q++) {
		unsigned short b = (unsigned short)(4 * q);
		table[6 * q + 0] = b;
		table[6 * q + 1] = (unsigned short)(b + 1);
		table[6 * q + 2] = (unsigned short)(b + 2);
		table[6 * q + 3] = b;
		table[6 * q + 4] = (unsigned short)(b + 2);
		table[6 * q + 5] = (unsigned short)(b + 3);
	}
}
