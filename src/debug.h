/**
 * debug.h
 * Debug print module.
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "koopa.h"

/* TODO: make these invisible from other transition units */
void _debug_raw_type(koopa_raw_type_t raw);
void _debug_raw_function(koopa_raw_function_t raw);
void _debug_raw_basic_block(koopa_raw_basic_block_t raw);
void _debug_raw_value(koopa_raw_value_t raw);
void _debug_raw_program(const koopa_raw_program_t *raw);

/**
 * Print raw Koopa IR in JSON5 (https://json5.org) format.
 * Similar to the `debug!` macro in Rust.
 */
#define debug(raw) _Generic((raw),					\
		koopa_raw_type_t: _debug_raw_type,			\
		koopa_raw_function_t: _debug_raw_function,		\
		koopa_raw_basic_block_t: _debug_raw_basic_block,	\
		koopa_raw_value_t: _debug_raw_value,			\
		koopa_raw_program_t *: _debug_raw_program		\
	)((raw))

#endif//_DEBUG_H_
