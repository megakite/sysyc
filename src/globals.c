#include "globals.h"

jmp_buf exception_env;
/* god i really don't have any clue how we can free() things with this crappy
 * cyclic, self-referenced tree (graph?) structure. guess i'll just be lazy for
 * a moment. feel free to blame me if this is in fact a bad idea. */
bump_t g_bump;
symbols_t g_symbols;
