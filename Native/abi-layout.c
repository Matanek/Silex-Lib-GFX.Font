#include "SilexFont.h"

#include <stddef.h>
#include <stdint.h>

_Static_assert(SILEX_FONT_ABI_VERSION == 5u, "unexpected font ABI version");
_Static_assert(sizeof(void *) == 8u, "GFX.Font supports the 64-bit Silex matrix");
_Static_assert(sizeof(size_t) == 8u, "GFX.Font requires a 64-bit size_t");
_Static_assert(_Alignof(void *) == 8u, "GFX.Font requires 8-byte pointer alignment");

int silex_font_abi_layout_probe(void) {
    return 0;
}
