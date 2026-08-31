#ifndef SILEX_FONT_H
#define SILEX_FONT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SILEX_FONT_ABI_VERSION 5u

enum SilexFontStatus {
    SILEX_FONT_OK = 0,
    SILEX_FONT_EMPTY_DATA = 1,
    SILEX_FONT_INVALID_DATA = 2,
    SILEX_FONT_FACE_INDEX_OUT_OF_RANGE = 3,
    SILEX_FONT_OUT_OF_MEMORY = 4,
    SILEX_FONT_UNSUPPORTED = 5,
    SILEX_FONT_ABI_MISMATCH = 6,
    SILEX_FONT_INTERNAL = 7,
    SILEX_FONT_INVALID_INPUT = 8
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

enum SilexFontDirection {
    SILEX_FONT_DIRECTION_AUTO = 0,
    SILEX_FONT_DIRECTION_LEFT_TO_RIGHT = 1,
    SILEX_FONT_DIRECTION_RIGHT_TO_LEFT = 2,
    SILEX_FONT_DIRECTION_TOP_TO_BOTTOM = 3,
    SILEX_FONT_DIRECTION_BOTTOM_TO_TOP = 4
};

enum SilexFontOutlineCommand {
    SILEX_FONT_OUTLINE_MOVE = 1,
    SILEX_FONT_OUTLINE_LINE = 2,
    SILEX_FONT_OUTLINE_CONIC_TO = 3,
    SILEX_FONT_OUTLINE_CUBIC_TO = 4,
    SILEX_FONT_OUTLINE_CLOSE = 5
};

enum SilexFontHinting {
    SILEX_FONT_HINTING_NONE = 0,
    SILEX_FONT_HINTING_LIGHT = 1,
    SILEX_FONT_HINTING_NORMAL = 2
};

enum SilexFontAntialiasing {
    SILEX_FONT_ANTIALIASING_GRAYSCALE = 0,
    SILEX_FONT_ANTIALIASING_MONOCHROME = 1
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

void *silex_font_outline_create(const void *instance, uint32_t glyph_id);
void silex_font_outline_destroy(void *outline);
uint32_t silex_font_outline_step_count(const void *outline);
int32_t silex_font_outline_step_kind(const void *outline, uint32_t index);
int32_t silex_font_outline_step_coordinate(const void *outline, uint32_t index, uint32_t coordinate);
int32_t silex_font_outline_x_min(const void *outline);
int32_t silex_font_outline_y_min(const void *outline);
int32_t silex_font_outline_x_max(const void *outline);
int32_t silex_font_outline_y_max(const void *outline);
int32_t silex_font_outline_x_advance(const void *outline);
int32_t silex_font_outline_y_advance(const void *outline);
uint32_t silex_font_face_outline_decomposition_count(const void *face);
void silex_font_test_fail_outline_after(uint32_t step_count);

void *silex_font_bitmap_create(
    const void *instance,
    uint32_t glyph_id,
    int32_t size_26_6,
    int32_t hinting,
    int32_t antialiasing
);
void silex_font_bitmap_destroy(void *bitmap);
int32_t silex_font_bitmap_width(const void *bitmap);
int32_t silex_font_bitmap_height(const void *bitmap);
int32_t silex_font_bitmap_stride(const void *bitmap);
int32_t silex_font_bitmap_left(const void *bitmap);
int32_t silex_font_bitmap_top(const void *bitmap);
int32_t silex_font_bitmap_copy_pixels(const void *bitmap, uint8_t *output, size_t byte_count);
uint32_t silex_font_face_bitmap_rasterization_count(const void *face);
uint32_t silex_font_face_bitmap_cache_entry_count(const void *face);
size_t silex_font_face_bitmap_cache_byte_count(const void *face);

void *silex_font_coverage_create(
    const void *instance,
    int32_t size_26_6,
    int32_t hinting,
    int32_t antialiasing
);
int32_t silex_font_coverage_add_glyph(
    void *coverage,
    uint32_t glyph_id,
    int32_t origin_x,
    int32_t origin_y
);
int32_t silex_font_coverage_finish(void *coverage, int32_t padding, size_t maximum_pixels);
void silex_font_coverage_destroy(void *coverage);
int32_t silex_font_coverage_width(const void *coverage);
int32_t silex_font_coverage_height(const void *coverage);
int32_t silex_font_coverage_stride(const void *coverage);
int32_t silex_font_coverage_origin_x(const void *coverage);
int32_t silex_font_coverage_origin_y(const void *coverage);
int32_t silex_font_coverage_copy_pixels(const void *coverage, uint8_t *output, size_t byte_count);

void *silex_font_shape_create(
    const void *instance,
    const uint8_t *utf8,
    size_t byte_count,
    int32_t direction,
    uint32_t script,
    const uint8_t *language,
    size_t language_byte_count
);
int32_t silex_font_shape_add_feature(
    void *shape,
    uint32_t tag,
    uint32_t value,
    uint32_t start,
    uint32_t end
);
int32_t silex_font_shape_finish(void *shape);
void silex_font_shape_destroy(void *shape);
uint32_t silex_font_shape_glyph_count(const void *shape);
int32_t silex_font_shape_direction(const void *shape);
uint32_t silex_font_shape_script(const void *shape);
const char *silex_font_shape_language(const void *shape);
uint32_t silex_font_shape_glyph_id(const void *shape, uint32_t index);
uint32_t silex_font_shape_glyph_cluster_start(const void *shape, uint32_t index);
uint32_t silex_font_shape_glyph_cluster_end(const void *shape, uint32_t index);
int32_t silex_font_shape_glyph_x_advance(const void *shape, uint32_t index);
int32_t silex_font_shape_glyph_y_advance(const void *shape, uint32_t index);
int32_t silex_font_shape_glyph_x_offset(const void *shape, uint32_t index);
int32_t silex_font_shape_glyph_y_offset(const void *shape, uint32_t index);
int32_t silex_font_shape_glyph_x_origin(const void *shape, uint32_t index);
int32_t silex_font_shape_glyph_y_origin(const void *shape, uint32_t index);
int32_t silex_font_shape_glyph_has_ink(const void *shape, uint32_t index);
int32_t silex_font_shape_glyph_x_bearing(const void *shape, uint32_t index);
int32_t silex_font_shape_glyph_y_bearing(const void *shape, uint32_t index);
int32_t silex_font_shape_glyph_width(const void *shape, uint32_t index);
int32_t silex_font_shape_glyph_height(const void *shape, uint32_t index);
int32_t silex_font_shape_x_advance(const void *shape);
int32_t silex_font_shape_y_advance(const void *shape);
uint32_t silex_font_instance_shape_count(const void *instance);

int32_t silex_font_last_error_code(void);
const char *silex_font_last_error_detail(void);
uint32_t silex_font_live_library_count(void);
uint32_t silex_font_live_face_count(void);
uint32_t silex_font_live_instance_count(void);
uint32_t silex_font_live_outline_count(void);
uint32_t silex_font_live_bitmap_count(void);
uint32_t silex_font_live_coverage_count(void);

#ifdef __cplusplus
}
#endif

#endif
