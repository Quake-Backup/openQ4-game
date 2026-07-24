
#ifndef __MD5_H__
#define __MD5_H__

#include <stdint.h>

/*
===============================================================================

	Calculates a checksum for a block of data
	using the MD5 message-digest algorithm.

===============================================================================
*/

uint32_t MD5_BlockChecksum( const void *data, int length );

#endif /* !__MD5_H__ */
