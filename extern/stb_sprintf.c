#ifdef sprintf
#undef sprintf
#endif

#ifdef snprintf
#undef snprintf
#endif

#ifdef _sprintf
#undef _sprintf
#endif

#ifdef _snprintf
#undef _snprintf
#endif

#define STB_SPRINTF_IMPLEMENTATION

#include "stb_sprintf.h"