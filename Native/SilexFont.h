#ifndef SILEX_FONT_H
#define SILEX_FONT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SILEX_FONT_ABI_VERSION 2u

enum SilexFontStatus {
    SILEX_FONT_OK = 0,
    SILEX_FONT_EMPTY_DATA = 1,
    SILEX_FONT_INVALID_DATA = 2,
    SILEX_FONT_FACE_INDEX_OUT_OF_RANGE = 3,
    SILEX_FONT_OUT_OF_MEMORY = 4,
    SILEX_FONT_UNSUPPORTED = 5,
    SILEX_FONT_ABI_MISMATCH = 6,
    SILEX_FONT_INTERNAL = 7
};

enum SilexFontCapability {
    SILEX_FONT_CAPABILITY_OUTLINES = 1 << 0,
    SILEX_FONT_CAPABILITY_SHAPING = 1 << 1,
    SILEX_FONT_CAPABILITY_VARIATIONS = 1 << 2,
    SILEX_FONT_CAPABILITY_COLOR = 1 << 3
};

enum SilexFontOutlineKind {
    SILEX_FONT_OUTLINE_NONE = 0,
    SILEX_FONT_OUTLINE_QUADRATIC = 1,
    SILEX_FONT_OUTLINE_CUBIC = 2
};

uint32_t silex_font_abi_version(void);
uint32_t silex_font_abi_pointer_size(void);
uint32_t silex_font_abi_size_size(void);
uint32_t silex_font_abi_pointer_alignment(void);

void *silex_font_library_create(void);
void silex_font_library_destroy(void *library);
void *silex_font_face_create(void *library, const uint8_t *bytes, size_t byte_count, int32_t face_index);
void silex_font_face_destroy(void *face);

int32_t silex_font_face_index(const void *face);
int32_t silex_font_face_count(const void *face);
int32_t silex_font_face_glyph_count(const void *face);
int32_t silex_font_face_units_per_em(const void *face);
uint32_t silex_font_face_capabilities(const void *face);
int32_t silex_font_face_outline_kind(const void *face);
const char *silex_font_face_family_name(const void *face);
const char *silex_font_face_subfamily_name(const void *face);
const char *silex_font_face_postscript_name(const void *face);
int32_t silex_font_face_supports_scalar(const void *face, uint32_t scalar);

int32_t silex_font_face_ascender(const void *face);
int32_t silex_font_face_descender(const void *face);
int32_t silex_font_face_line_gap(const void *face);
int32_t silex_font_face_line_height(const void *face);
int32_t silex_font_face_underline_position(const void *face);
int32_t silex_font_face_underline_thickness(const void *face);
int32_t silex_font_face_strikeout_position(const void *face);
int32_t silex_font_face_strikeout_thickness(const void *face);

uint32_t silex_font_face_axis_count(const void *face);
uint32_t silex_font_face_axis_tag(const void *face, uint32_t axis_index);
const char *silex_font_face_axis_name(const void *face, uint32_t axis_index);
float silex_font_face_axis_minimum(const void *face, uint32_t axis_index);
float silex_font_face_axis_default(const void *face, uint32_t axis_index);
float silex_font_face_axis_maximum(const void *face, uint32_t axis_index);

uint32_t silex_font_face_named_instance_count(const void *face);
const char *silex_font_face_named_instance_name(const void *face, uint32_t instance_index);
const char *silex_font_face_named_instance_postscript_name(const void *face, uint32_t instance_index);
float silex_font_face_named_instance_coordinate(const void *face, uint32_t instance_index, uint32_t axis_index);

uint32_t silex_font_tag_from_bytes(const uint8_t *bytes);
const char *silex_font_tag_text(uint32_t tag);

void *silex_font_instance_create(const void *face);
int32_t silex_font_instance_set_variation(void *instance, uint32_t tag, float value);
void silex_font_instance_destroy(void *instance);
int32_t silex_font_instance_scalar_bounds(
    const void *instance,
    uint32_t scalar,
    int32_t *x_bearing,
    int32_t *y_bearing,
    int32_t *width,
    int32_t *height
);

int32_t silex_font_last_error_code(void);
const char *silex_font_last_error_detail(void);
uint32_t silex_font_live_library_count(void);
uint32_t silex_font_live_face_count(void);
uint32_t silex_font_live_instance_count(void);

#ifdef __cplusplus
}
#endif

#endif
