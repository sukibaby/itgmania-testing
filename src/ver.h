#ifndef STEPMANIA_VER_H
#define STEPMANIA_VER_H

// Backward-compat header for legacy global version symbols.
// Prefer the generated version.hpp when available.
#if defined(__has_include)
#if __has_include("version.hpp")
#include "version.hpp"
#else
extern char const * const product_version;
extern unsigned long const version_num;
extern char const * const version_time;
extern char const * const version_date;
extern char const * const sm_version_git_hash;
#endif
#else
extern char const * const product_version;
extern unsigned long const version_num;
extern char const * const version_time;
extern char const * const version_date;
extern char const * const sm_version_git_hash;
#endif

#endif
