/**
 * globals.h
 * _Real_ global variables.
 */

#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#include <setjmp.h>

#include "bump.h"
#include "symbols.h"

// exception handler
extern jmp_buf g_exception_env;

// bump allocator for Koopa raw program
extern bump_t g_bump;

// symbol table
extern symbols_t g_symbols;

#endif//_GLOBALS_H_
