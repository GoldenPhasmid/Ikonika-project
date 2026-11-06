/*
 * The one translation unit that emits miniaudio's function bodies. Everywhere else includes
 * miniaudio.h as a declaration-only header.
 *
 * Compiled as C rather than C++ so the header takes its C path, which is the one upstream tests.
 */

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
