#ifndef SYMBEX_H_
#define SYMBEX_H_

#ifdef SYMBEX_OPTIMIZATIONS

#include <pydebug.h>

#include <s2e.h>

/* Concretize memory block sizes at allocation */
#define _SYMBEX_ALLOC             1
#define _SYMBEX_INSTRUMENT        1
#endif

#define MAX_ALLOC_SIZE		1024

#if 0 // XXX: Investigate this optimization
#define PREPARE_ALLOC(size) \
	do { \
		if (s2e_is_symbolic(&(size), sizeof(size))) { \
			size = s2e_get_upper_bound(size); \
		} \
	} while(0)
#else
#define PREPARE_ALLOC(size) \
	do { \
		s2e_get_example(&(size), sizeof(size)); \
	} while(0)
#endif

#endif

#endif /* !SYMBEX_H_ */
