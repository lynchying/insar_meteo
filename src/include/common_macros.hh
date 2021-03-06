#ifndef CAPI_MACROS_HPP
#define CAPI_MACROS_HPP

#include "Python.h"

// some functions are inline, in case your compiler doesn't like "static inline"
// but wants "__inline__" or something instead, #define DG_DYNARR_INLINE accordingly.
#ifndef DG_DYNARR_INLINE
	// for pre-C99 compilers you might have to use something compiler-specific (or maybe only "static")
	#ifdef _MSC_VER
		#define DG_DYNARR_INLINE static __inline
	#else
		#define DG_DYNARR_INLINE static inline
	#endif
#endif


// if you want to prepend something to the non inline (DG_DYNARR_INLINE) functions,
// like "__declspec(dllexport)" or whatever, #define DG_DYNARR_DEF
#ifndef DG_DYNARR_DEF
	// by defaults it's empty.
	#define DG_DYNARR_DEF
#endif


#ifndef DG_DYNARR_MALLOC
	#define Mem_New(elem_type, n_elem) PyMem_New(elem_type, n_elem)

	#define Mem_Resize(ptr, elem_type, new_n_elem) PyMem_Resize(ptr, elem_type, new_n_elem)

	#define Mem_Del(ptr) PyMem_Del(ptr)
#endif

#endif
