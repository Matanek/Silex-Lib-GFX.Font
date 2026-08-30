#include "SilexFont.h"

#include <stdatomic.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_FONT_FORMATS_H
#include FT_MULTIPLE_MASTERS_H
#include FT_TRUETYPE_TABLES_H
#include <hb.h>
#include <hb-ot.h>

#if defined(_MSC_VER)
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL _Thread_local
#endif

#define LIBRARY_MAGIC UINT32_C(0x53464c42)
#define FACE_MAGIC UINT32_C(0x53464643)
#define INSTANCE_MAGIC UINT32_C(0x5346494e)
#define SHAPE_MAGIC UINT32_C(0x53465348)

typedef struct FontLibrary { uint32_t magic; } FontLibrary;

typedef struct FontFace {
    uint32_t magic;
    uint8_t *bytes;
    size_t byte_count;
    FT_Face ft_face;
    hb_blob_t *hb_blob;
    hb_face_t *hb_face;
    int32_t index;
    int32_t count;
} FontFace;

typedef struct FontInstance {
    uint32_t magic;
    hb_font_t *hb_font;
    hb_variation_t *variations;
    unsigned int variation_count;
    uint32_t shape_count;
} FontInstance;

typedef struct FontGlyphRecord {
    uint32_t id;
    uint32_t cluster_start;
    uint32_t cluster_end;
    int32_t x_advance;
    int32_t y_advance;
    int32_t x_offset;
    int32_t y_offset;
    int32_t x_origin;
    int32_t y_origin;
    int32_t has_ink;
    int32_t x_bearing;
    int32_t y_bearing;
    int32_t width;
    int32_t height;
} FontGlyphRecord;

typedef struct FontShape {
    uint32_t magic;
    FontInstance *instance;
    uint8_t *text;
    size_t byte_count;
    int32_t requested_direction;
    uint32_t requested_script;
    char requested_language[64];
    hb_feature_t *features;
    uint32_t feature_count;
    FontGlyphRecord *glyphs;
    uint32_t glyph_count;
    int32_t direction;
    uint32_t script;
    char language[64];
    int32_t x_advance;
    int32_t y_advance;
    int32_t finished;
} FontShape;

typedef struct FontState {
    FT_Library freetype;
    uint32_t libraries;
    uint32_t faces;
    uint32_t instances;
} FontState;

static FontState state;
static atomic_flag state_guard = ATOMIC_FLAG_INIT;
static THREAD_LOCAL int32_t error_code = SILEX_FONT_OK;
static THREAD_LOCAL char error_detail[256];
static THREAD_LOCAL char text_buffer[256];

static void lock_state(void) {
    while (atomic_flag_test_and_set_explicit(&state_guard, memory_order_acquire)) {}
}

static void unlock_state(void) {
    atomic_flag_clear_explicit(&state_guard, memory_order_release);
}

static void clear_error(void) {
    error_code = SILEX_FONT_OK;
    error_detail[0] = '\0';
}

static void fail(int32_t code, const char *detail) {
    error_code = code;
    (void)snprintf(
        error_detail,
        sizeof(error_detail),
        "%s",
        detail == NULL || detail[0] == '\0' ? "unknown font boundary error" : detail
    );
}

static int32_t map_ft_error(FT_Error error) {
    if (error == FT_Err_Out_Of_Memory) return SILEX_FONT_OUT_OF_MEMORY;
    if (error == FT_Err_Unimplemented_Feature) return SILEX_FONT_UNSUPPORTED;
    return SILEX_FONT_INVALID_DATA;
}

static void fail_ft(const char *action, FT_Error error) {
    const char *detail = FT_Error_String(error);
    char message[256];
    if (detail == NULL) {
        (void)snprintf(message, sizeof(message), "%s (FreeType error %d)", action, error);
    } else {
        (void)snprintf(message, sizeof(message), "%s: %s", action, detail);
    }
    fail(map_ft_error(error), message);
}

static void finalize_state_if_unused(void) {
    if (state.freetype != NULL && state.libraries == 0 && state.faces == 0 && state.instances == 0) {
        FT_Done_FreeType(state.freetype);
        state.freetype = NULL;
    }
}

static const FontFace *checked_face(const void *raw) {
    const FontFace *face = raw;
    if (face == NULL || face->magic != FACE_MAGIC) {
        fail(SILEX_FONT_INTERNAL, "a live font face is required");
        return NULL;
    }
    return face;
}

static FontInstance *checked_instance(void *raw) {
    FontInstance *instance = raw;
    if (instance == NULL || instance->magic != INSTANCE_MAGIC) {
        fail(SILEX_FONT_INTERNAL, "a live font instance is required");
        return NULL;
    }
    return instance;
}

static FontShape *checked_shape(void *raw) {
    FontShape *shape = raw;
    if (shape == NULL || shape->magic != SHAPE_MAGIC) {
        fail(SILEX_FONT_INTERNAL, "a live font shape is required");
        return NULL;
    }
    return shape;
}

static const FontShape *checked_finished_shape(const void *raw) {
    const FontShape *shape = raw;
    if (shape == NULL || shape->magic != SHAPE_MAGIC || !shape->finished) {
        fail(SILEX_FONT_INTERNAL, "a finished font shape is required");
        return NULL;
    }
    return shape;
}

static const FontGlyphRecord *checked_glyph(const void *raw, uint32_t index) {
    const FontShape *shape = checked_finished_shape(raw);
    if (shape == NULL || index >= shape->glyph_count) {
        fail(SILEX_FONT_INTERNAL, "font shape glyph index is out of range");
        return NULL;
    }
    return &shape->glyphs[index];
}

static int valid_utf8(const uint8_t *bytes, size_t count) {
    size_t index = 0;
    while (index < count) {
        uint8_t first = bytes[index];
        size_t length = 0;
        if (first <= UINT8_C(0x7f)) length = 1;
        else if (first >= UINT8_C(0xc2) && first <= UINT8_C(0xdf)) length = 2;
        else if (first >= UINT8_C(0xe0) && first <= UINT8_C(0xef)) length = 3;
        else if (first >= UINT8_C(0xf0) && first <= UINT8_C(0xf4)) length = 4;
        else return 0;
        if (index + length > count) return 0;
        for (size_t continuation = 1; continuation < length; ++continuation) {
            if (bytes[index + continuation] < UINT8_C(0x80) || bytes[index + continuation] > UINT8_C(0xbf)) return 0;
        }
        if (length >= 2) {
            uint8_t second = bytes[index + 1];
            if (first == UINT8_C(0xe0) && second < UINT8_C(0xa0)) return 0;
            if (first == UINT8_C(0xed) && second >= UINT8_C(0xa0)) return 0;
            if (first == UINT8_C(0xf0) && second < UINT8_C(0x90)) return 0;
            if (first == UINT8_C(0xf4) && second > UINT8_C(0x8f)) return 0;
        }
        index += length;
    }
    return 1;
}

static int scalar_boundary(const uint8_t *bytes, size_t count, uint32_t offset) {
    if ((size_t)offset > count) return 0;
    if ((size_t)offset == count) return 1;
    return bytes[offset] < UINT8_C(0x80) || bytes[offset] > UINT8_C(0xbf);
}

static int valid_language(const uint8_t *bytes, size_t count) {
    if (count == 0 || count >= 64) return 0;
    if (bytes[0] == '-' || bytes[0] == '_' || bytes[count - 1] == '-' || bytes[count - 1] == '_') return 0;
    for (size_t index = 0; index < count; ++index) {
        uint8_t value = bytes[index];
        int alphanumeric = (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9');
        if (!alphanumeric && value != '-' && value != '_') return 0;
        if (index > 0 && (value == '-' || value == '_') && (bytes[index - 1] == '-' || bytes[index - 1] == '_')) return 0;
    }
    return 1;
}

static hb_direction_t to_hb_direction(int32_t direction) {
    if (direction == SILEX_FONT_DIRECTION_LEFT_TO_RIGHT) return HB_DIRECTION_LTR;
    if (direction == SILEX_FONT_DIRECTION_RIGHT_TO_LEFT) return HB_DIRECTION_RTL;
    if (direction == SILEX_FONT_DIRECTION_TOP_TO_BOTTOM) return HB_DIRECTION_TTB;
    if (direction == SILEX_FONT_DIRECTION_BOTTOM_TO_TOP) return HB_DIRECTION_BTT;
    return HB_DIRECTION_INVALID;
}

static int32_t from_hb_direction(hb_direction_t direction) {
    if (direction == HB_DIRECTION_RTL) return SILEX_FONT_DIRECTION_RIGHT_TO_LEFT;
    if (direction == HB_DIRECTION_TTB) return SILEX_FONT_DIRECTION_TOP_TO_BOTTOM;
    if (direction == HB_DIRECTION_BTT) return SILEX_FONT_DIRECTION_BOTTOM_TO_TOP;
    return SILEX_FONT_DIRECTION_LEFT_TO_RIGHT;
}

static int compare_u32(const void *left, const void *right) {
    uint32_t a = *(const uint32_t *)left;
    uint32_t b = *(const uint32_t *)right;
    return a < b ? -1 : a > b ? 1 : 0;
}

static int get_axis(const FontFace *face, uint32_t index, hb_ot_var_axis_info_t *info) {
    if (face == NULL || index >= hb_ot_var_get_axis_count(face->hb_face)) {
        fail(SILEX_FONT_INTERNAL, "font variation axis index is out of range");
        return 0;
    }
    unsigned int count = 1;
    (void)hb_ot_var_get_axis_infos(face->hb_face, index, &count, info);
    return count == 1;
}

static const char *get_name(hb_face_t *face, hb_ot_name_id_t name_id) {
    text_buffer[0] = '\0';
    if (name_id == HB_OT_NAME_ID_INVALID) return text_buffer;
    unsigned int size = (unsigned int)sizeof(text_buffer) - 1;
    unsigned int written = hb_ot_name_get_utf8(
        face, name_id, hb_language_from_string("en", -1), &size, text_buffer
    );
    if (written == 0) {
        size = (unsigned int)sizeof(text_buffer) - 1;
        written = hb_ot_name_get_utf8(face, name_id, HB_LANGUAGE_INVALID, &size, text_buffer);
    }
    if (written >= sizeof(text_buffer)) written = sizeof(text_buffer) - 1;
    text_buffer[written] = '\0';
    return text_buffer;
}

uint32_t silex_font_abi_version(void) { return SILEX_FONT_ABI_VERSION; }
uint32_t silex_font_abi_pointer_size(void) { return (uint32_t)sizeof(void *); }
uint32_t silex_font_abi_size_size(void) { return (uint32_t)sizeof(size_t); }
uint32_t silex_font_abi_pointer_alignment(void) { return (uint32_t)_Alignof(void *); }

void *silex_font_library_create(void) {
    clear_error();
    FontLibrary *library = calloc(1, sizeof(*library));
    if (library == NULL) {
        fail(SILEX_FONT_OUT_OF_MEMORY, "could not allocate the font library handle");
        return NULL;
    }
    lock_state();
    if (state.freetype == NULL) {
        FT_Error error = FT_Init_FreeType(&state.freetype);
        if (error != FT_Err_Ok) {
            unlock_state();
            free(library);
            fail_ft("could not initialize FreeType", error);
            return NULL;
        }
    }
    ++state.libraries;
    unlock_state();
    library->magic = LIBRARY_MAGIC;
    return library;
}

void silex_font_library_destroy(void *raw) {
    if (raw == NULL) return;
    FontLibrary *library = raw;
    if (library->magic != LIBRARY_MAGIC) {
        fail(SILEX_FONT_INTERNAL, "invalid font library handle");
        return;
    }
    library->magic = 0;
    free(library);
    lock_state();
    if (state.libraries > 0) --state.libraries;
    finalize_state_if_unused();
    unlock_state();
}

void *silex_font_face_create(void *raw_library, const uint8_t *bytes, size_t count, int32_t index) {
    clear_error();
    FontLibrary *library = raw_library;
    if (library == NULL || library->magic != LIBRARY_MAGIC) {
        fail(SILEX_FONT_INTERNAL, "a live font library is required");
        return NULL;
    }
    if (bytes == NULL || count == 0) {
        fail(SILEX_FONT_EMPTY_DATA, "font data cannot be empty");
        return NULL;
    }
    if (index < 0) {
        fail(SILEX_FONT_FACE_INDEX_OUT_OF_RANGE, "font face index cannot be negative");
        return NULL;
    }
    FontFace *face = calloc(1, sizeof(*face));
    if (face == NULL) {
        fail(SILEX_FONT_OUT_OF_MEMORY, "could not allocate the font face handle");
        return NULL;
    }
    face->bytes = malloc(count);
    if (face->bytes == NULL) {
        free(face);
        fail(SILEX_FONT_OUT_OF_MEMORY, "could not retain the font bytes");
        return NULL;
    }
    memcpy(face->bytes, bytes, count);
    face->byte_count = count;

    FT_Face probe = NULL;
    lock_state();
    FT_Error error = FT_New_Memory_Face(state.freetype, face->bytes, (FT_Long)count, -1, &probe);
    unlock_state();
    if (error != FT_Err_Ok) {
        free(face->bytes);
        free(face);
        fail_ft("could not read the font data", error);
        return NULL;
    }
    face->count = (int32_t)probe->num_faces;
    lock_state();
    FT_Done_Face(probe);
    unlock_state();
    if (index >= face->count) {
        char message[160];
        (void)snprintf(message, sizeof(message), "font face index %d is outside the collection of %d face(s)", index, face->count);
        free(face->bytes);
        free(face);
        fail(SILEX_FONT_FACE_INDEX_OUT_OF_RANGE, message);
        return NULL;
    }

    lock_state();
    error = FT_New_Memory_Face(state.freetype, face->bytes, (FT_Long)count, index, &face->ft_face);
    unlock_state();
    if (error != FT_Err_Ok) {
        free(face->bytes);
        free(face);
        fail_ft("could not open the requested font face", error);
        return NULL;
    }
    face->hb_blob = hb_blob_create((const char *)face->bytes, (unsigned int)count, HB_MEMORY_MODE_READONLY, NULL, NULL);
    face->hb_face = hb_face_create(face->hb_blob, (unsigned int)index);
    if (face->hb_blob == hb_blob_get_empty() || face->hb_face == hb_face_get_empty() || hb_face_get_upem(face->hb_face) == 0) {
        hb_face_destroy(face->hb_face);
        hb_blob_destroy(face->hb_blob);
        lock_state();
        FT_Done_Face(face->ft_face);
        unlock_state();
        free(face->bytes);
        free(face);
        fail(SILEX_FONT_INVALID_DATA, "HarfBuzz could not read the requested font face");
        return NULL;
    }
    face->magic = FACE_MAGIC;
    face->index = index;
    lock_state();
    ++state.faces;
    unlock_state();
    return face;
}

void silex_font_face_destroy(void *raw) {
    if (raw == NULL) return;
    FontFace *face = raw;
    if (face->magic != FACE_MAGIC) {
        fail(SILEX_FONT_INTERNAL, "invalid font face handle");
        return;
    }
    face->magic = 0;
    hb_face_destroy(face->hb_face);
    hb_blob_destroy(face->hb_blob);
    lock_state();
    FT_Done_Face(face->ft_face);
    unlock_state();
    free(face->bytes);
    free(face);
    lock_state();
    if (state.faces > 0) --state.faces;
    finalize_state_if_unused();
    unlock_state();
}

int32_t silex_font_face_index(const void *raw) { const FontFace *f = checked_face(raw); return f == NULL ? -1 : f->index; }
int32_t silex_font_face_count(const void *raw) { const FontFace *f = checked_face(raw); return f == NULL ? 0 : f->count; }
int32_t silex_font_face_glyph_count(const void *raw) { const FontFace *f = checked_face(raw); return f == NULL ? 0 : (int32_t)f->ft_face->num_glyphs; }
int32_t silex_font_face_units_per_em(const void *raw) { const FontFace *f = checked_face(raw); return f == NULL ? 0 : (int32_t)f->ft_face->units_per_EM; }

uint32_t silex_font_face_capabilities(const void *raw) {
    const FontFace *face = checked_face(raw);
    if (face == NULL) return 0;
    uint32_t result = SILEX_FONT_CAPABILITY_SHAPING;
    if (FT_IS_SCALABLE(face->ft_face)) result |= SILEX_FONT_CAPABILITY_OUTLINES;
    if (FT_HAS_MULTIPLE_MASTERS(face->ft_face)) result |= SILEX_FONT_CAPABILITY_VARIATIONS;
    if (FT_HAS_COLOR(face->ft_face)) result |= SILEX_FONT_CAPABILITY_COLOR;
    return result;
}

int32_t silex_font_face_outline_kind(const void *raw) {
    const FontFace *face = checked_face(raw);
    if (face == NULL || !FT_IS_SCALABLE(face->ft_face)) return SILEX_FONT_OUTLINE_NONE;
    const char *format = FT_Get_Font_Format(face->ft_face);
    if (format != NULL && (strcmp(format, "CFF") == 0 || strcmp(format, "CID Type 1") == 0)) return SILEX_FONT_OUTLINE_CUBIC;
    return SILEX_FONT_OUTLINE_QUADRATIC;
}

const char *silex_font_face_family_name(const void *raw) {
    const FontFace *face = checked_face(raw);
    return face == NULL || face->ft_face->family_name == NULL ? "" : face->ft_face->family_name;
}

const char *silex_font_face_subfamily_name(const void *raw) {
    const FontFace *face = checked_face(raw);
    return face == NULL || face->ft_face->style_name == NULL ? "" : face->ft_face->style_name;
}

const char *silex_font_face_postscript_name(const void *raw) {
    const FontFace *face = checked_face(raw);
    if (face == NULL) return "";
    const char *name = FT_Get_Postscript_Name(face->ft_face);
    return name == NULL ? "" : name;
}

int32_t silex_font_face_supports_scalar(const void *raw, uint32_t scalar) {
    const FontFace *face = checked_face(raw);
    if (face == NULL || scalar > UINT32_C(0x10ffff) || (scalar >= UINT32_C(0xd800) && scalar <= UINT32_C(0xdfff))) return 0;
    lock_state();
    FT_UInt glyph = FT_Get_Char_Index(face->ft_face, (FT_ULong)scalar);
    unlock_state();
    return glyph == 0 ? 0 : 1;
}

int32_t silex_font_face_ascender(const void *raw) { const FontFace *f = checked_face(raw); return f == NULL ? 0 : (int32_t)f->ft_face->ascender; }
int32_t silex_font_face_descender(const void *raw) { const FontFace *f = checked_face(raw); return f == NULL ? 0 : (int32_t)f->ft_face->descender; }
int32_t silex_font_face_line_height(const void *raw) { const FontFace *f = checked_face(raw); return f == NULL ? 0 : (int32_t)f->ft_face->height; }
int32_t silex_font_face_underline_position(const void *raw) { const FontFace *f = checked_face(raw); return f == NULL ? 0 : (int32_t)f->ft_face->underline_position; }
int32_t silex_font_face_underline_thickness(const void *raw) { const FontFace *f = checked_face(raw); return f == NULL ? 0 : (int32_t)f->ft_face->underline_thickness; }

int32_t silex_font_face_line_gap(const void *raw) {
    const FontFace *face = checked_face(raw);
    return face == NULL ? 0 : (int32_t)(face->ft_face->height - face->ft_face->ascender + face->ft_face->descender);
}

int32_t silex_font_face_strikeout_position(const void *raw) {
    const FontFace *face = checked_face(raw);
    const TT_OS2 *os2 = face == NULL ? NULL : FT_Get_Sfnt_Table(face->ft_face, ft_sfnt_os2);
    return os2 == NULL ? 0 : (int32_t)os2->yStrikeoutPosition;
}

int32_t silex_font_face_strikeout_thickness(const void *raw) {
    const FontFace *face = checked_face(raw);
    const TT_OS2 *os2 = face == NULL ? NULL : FT_Get_Sfnt_Table(face->ft_face, ft_sfnt_os2);
    return os2 == NULL ? 0 : (int32_t)os2->yStrikeoutSize;
}

uint32_t silex_font_face_axis_count(const void *raw) { const FontFace *f = checked_face(raw); return f == NULL ? 0 : hb_ot_var_get_axis_count(f->hb_face); }

uint32_t silex_font_face_axis_tag(const void *raw, uint32_t index) {
    const FontFace *face = checked_face(raw); hb_ot_var_axis_info_t info;
    return get_axis(face, index, &info) ? info.tag : 0;
}

const char *silex_font_face_axis_name(const void *raw, uint32_t index) {
    const FontFace *face = checked_face(raw); hb_ot_var_axis_info_t info;
    return get_axis(face, index, &info) ? get_name(face->hb_face, info.name_id) : "";
}

float silex_font_face_axis_minimum(const void *raw, uint32_t index) {
    const FontFace *face = checked_face(raw); hb_ot_var_axis_info_t info;
    return get_axis(face, index, &info) ? info.min_value : 0.0f;
}

float silex_font_face_axis_default(const void *raw, uint32_t index) {
    const FontFace *face = checked_face(raw); hb_ot_var_axis_info_t info;
    return get_axis(face, index, &info) ? info.default_value : 0.0f;
}

float silex_font_face_axis_maximum(const void *raw, uint32_t index) {
    const FontFace *face = checked_face(raw); hb_ot_var_axis_info_t info;
    return get_axis(face, index, &info) ? info.max_value : 0.0f;
}

uint32_t silex_font_face_named_instance_count(const void *raw) {
    const FontFace *face = checked_face(raw);
    return face == NULL ? 0 : hb_ot_var_get_named_instance_count(face->hb_face);
}

const char *silex_font_face_named_instance_name(const void *raw, uint32_t index) {
    const FontFace *face = checked_face(raw);
    if (face == NULL || index >= hb_ot_var_get_named_instance_count(face->hb_face)) return "";
    return get_name(face->hb_face, hb_ot_var_named_instance_get_subfamily_name_id(face->hb_face, index));
}

const char *silex_font_face_named_instance_postscript_name(const void *raw, uint32_t index) {
    const FontFace *face = checked_face(raw);
    if (face == NULL || index >= hb_ot_var_get_named_instance_count(face->hb_face)) return "";
    return get_name(face->hb_face, hb_ot_var_named_instance_get_postscript_name_id(face->hb_face, index));
}

float silex_font_face_named_instance_coordinate(const void *raw, uint32_t instance_index, uint32_t axis_index) {
    const FontFace *face = checked_face(raw);
    uint32_t axis_count = face == NULL ? 0 : hb_ot_var_get_axis_count(face->hb_face);
    if (face == NULL || instance_index >= hb_ot_var_get_named_instance_count(face->hb_face) || axis_index >= axis_count || axis_count > 64) return 0.0f;
    float coordinates[64];
    unsigned int count = axis_count;
    if (hb_ot_var_named_instance_get_design_coords(face->hb_face, instance_index, &count, coordinates) != axis_count) return 0.0f;
    return coordinates[axis_index];
}

uint32_t silex_font_tag_from_bytes(const uint8_t *bytes) {
    return bytes == NULL ? 0 : HB_TAG(bytes[0], bytes[1], bytes[2], bytes[3]);
}

const char *silex_font_tag_text(uint32_t tag) {
    text_buffer[0] = (char)((tag >> 24) & 0xff);
    text_buffer[1] = (char)((tag >> 16) & 0xff);
    text_buffer[2] = (char)((tag >> 8) & 0xff);
    text_buffer[3] = (char)(tag & 0xff);
    text_buffer[4] = '\0';
    return text_buffer;
}

void *silex_font_instance_create(const void *raw_face) {
    clear_error();
    const FontFace *face = checked_face(raw_face);
    if (face == NULL) return NULL;
    FontInstance *instance = calloc(1, sizeof(*instance));
    if (instance == NULL) {
        fail(SILEX_FONT_OUT_OF_MEMORY, "could not allocate the font instance handle");
        return NULL;
    }
    instance->hb_font = hb_font_create(face->hb_face);
    if (instance->hb_font == hb_font_get_empty()) {
        free(instance);
        fail(SILEX_FONT_OUT_OF_MEMORY, "could not create the HarfBuzz font instance");
        return NULL;
    }
    hb_ot_font_set_funcs(instance->hb_font);
    int upem = (int)hb_face_get_upem(face->hb_face);
    hb_font_set_scale(instance->hb_font, upem, upem);
    instance->magic = INSTANCE_MAGIC;
    lock_state(); ++state.instances; unlock_state();
    return instance;
}

int32_t silex_font_instance_set_variation(void *raw, uint32_t tag, float value) {
    FontInstance *instance = checked_instance(raw);
    if (instance == NULL) return 0;
    hb_variation_t *next = realloc(
        instance->variations,
        (instance->variation_count + 1) * sizeof(*instance->variations)
    );
    if (next == NULL) {
        fail(SILEX_FONT_OUT_OF_MEMORY, "could not retain the variation coordinates");
        return 0;
    }
    instance->variations = next;
    instance->variations[instance->variation_count].tag = tag;
    instance->variations[instance->variation_count].value = value;
    ++instance->variation_count;
    hb_font_set_variations(instance->hb_font, instance->variations, instance->variation_count);
    return 1;
}

void silex_font_instance_destroy(void *raw) {
    if (raw == NULL) return;
    FontInstance *instance = raw;
    if (instance->magic != INSTANCE_MAGIC) {
        fail(SILEX_FONT_INTERNAL, "invalid font instance handle");
        return;
    }
    instance->magic = 0;
    hb_font_destroy(instance->hb_font);
    free(instance->variations);
    free(instance);
    lock_state();
    if (state.instances > 0) --state.instances;
    finalize_state_if_unused();
    unlock_state();
}

int32_t silex_font_instance_scalar_bounds(
    const void *raw,
    uint32_t scalar,
    int32_t *x_bearing,
    int32_t *y_bearing,
    int32_t *width,
    int32_t *height
) {
    const FontInstance *instance = raw;
    if (instance == NULL || instance->magic != INSTANCE_MAGIC || x_bearing == NULL || y_bearing == NULL || width == NULL || height == NULL) {
        fail(SILEX_FONT_INTERNAL, "a live font instance and four bounds outputs are required");
        return 0;
    }
    hb_codepoint_t glyph = 0;
    hb_glyph_extents_t extents;
    if (!hb_font_get_nominal_glyph(instance->hb_font, scalar, &glyph) || !hb_font_get_glyph_extents(instance->hb_font, glyph, &extents)) return 0;
    *x_bearing = extents.x_bearing;
    *y_bearing = extents.y_bearing;
    *width = extents.width;
    *height = extents.height;
    return 1;
}

void *silex_font_shape_create(
    const void *raw_instance,
    const uint8_t *utf8,
    size_t byte_count,
    int32_t direction,
    uint32_t script,
    const uint8_t *language,
    size_t language_byte_count
) {
    clear_error();
    FontInstance *instance = checked_instance((void *)raw_instance);
    if (instance == NULL) return NULL;
    if (byte_count > (size_t)INT_MAX || byte_count > UINT32_MAX || (byte_count > 0 && utf8 == NULL)) {
        fail(SILEX_FONT_INVALID_INPUT, "font shaping requires a bounded UTF-8 buffer");
        return NULL;
    }
    if (!valid_utf8(utf8, byte_count)) {
        fail(SILEX_FONT_INVALID_INPUT, "font shaping text must be valid UTF-8");
        return NULL;
    }
    for (size_t index = 0; index < byte_count; ++index) {
        if (utf8[index] == '\n' || utf8[index] == '\r') {
            fail(SILEX_FONT_INVALID_INPUT, "font shaping accepts one homogeneous run without line breaks");
            return NULL;
        }
    }
    if (direction < SILEX_FONT_DIRECTION_AUTO || direction > SILEX_FONT_DIRECTION_BOTTOM_TO_TOP) {
        fail(SILEX_FONT_INVALID_INPUT, "font shaping direction is invalid");
        return NULL;
    }
    if (script != 0) {
        for (unsigned int shift = 0; shift < 32; shift += 8) {
            uint8_t value = (uint8_t)((script >> shift) & UINT32_C(0xff));
            if (!((value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z'))) {
                fail(SILEX_FONT_INVALID_INPUT, "font shaping script must be a four-letter ISO 15924 tag");
                return NULL;
            }
        }
    }
    if (language_byte_count == 0) {
        language = (const uint8_t *)"und";
        language_byte_count = 3;
    }
    if (language == NULL || !valid_language(language, language_byte_count)) {
        fail(SILEX_FONT_INVALID_INPUT, "font shaping language must be a valid HarfBuzz-compatible language tag");
        return NULL;
    }

    FontShape *shape = calloc(1, sizeof(*shape));
    if (shape == NULL) {
        fail(SILEX_FONT_OUT_OF_MEMORY, "could not allocate the font shape handle");
        return NULL;
    }
    shape->text = malloc(byte_count == 0 ? 1 : byte_count);
    if (shape->text == NULL) {
        free(shape);
        fail(SILEX_FONT_OUT_OF_MEMORY, "could not retain the font shaping text");
        return NULL;
    }
    if (byte_count > 0) memcpy(shape->text, utf8, byte_count);
    memcpy(shape->requested_language, language, language_byte_count);
    shape->requested_language[language_byte_count] = '\0';
    shape->magic = SHAPE_MAGIC;
    shape->instance = instance;
    shape->byte_count = byte_count;
    shape->requested_direction = direction;
    shape->requested_script = script;
    return shape;
}

int32_t silex_font_shape_add_feature(
    void *raw,
    uint32_t tag,
    uint32_t value,
    uint32_t start,
    uint32_t end
) {
    FontShape *shape = checked_shape(raw);
    if (shape == NULL) return 0;
    if (shape->finished) {
        fail(SILEX_FONT_INTERNAL, "font shaping features cannot change after shaping");
        return 0;
    }
    for (unsigned int shift = 0; shift < 32; shift += 8) {
        uint8_t byte = (uint8_t)((tag >> shift) & UINT32_C(0xff));
        if (byte < UINT8_C(0x20) || byte > UINT8_C(0x7e)) {
            fail(SILEX_FONT_INVALID_INPUT, "OpenType feature tags must contain four printable ASCII bytes");
            return 0;
        }
    }
    if (start > end || (size_t)end > shape->byte_count ||
        !scalar_boundary(shape->text, shape->byte_count, start) ||
        !scalar_boundary(shape->text, shape->byte_count, end)) {
        fail(SILEX_FONT_INVALID_INPUT, "OpenType feature ranges must use ordered UTF-8 scalar boundaries");
        return 0;
    }
    if (shape->feature_count == UINT32_MAX ||
        (size_t)(shape->feature_count + 1) > SIZE_MAX / sizeof(*shape->features)) {
        fail(SILEX_FONT_OUT_OF_MEMORY, "too many OpenType shaping features");
        return 0;
    }
    hb_feature_t *next = realloc(shape->features, (shape->feature_count + 1) * sizeof(*shape->features));
    if (next == NULL) {
        fail(SILEX_FONT_OUT_OF_MEMORY, "could not retain the OpenType shaping features");
        return 0;
    }
    shape->features = next;
    shape->features[shape->feature_count].tag = tag;
    shape->features[shape->feature_count].value = value;
    shape->features[shape->feature_count].start = start;
    shape->features[shape->feature_count].end = end;
    ++shape->feature_count;
    return 1;
}

int32_t silex_font_shape_finish(void *raw) {
    clear_error();
    FontShape *shape = checked_shape(raw);
    if (shape == NULL) return 0;
    if (shape->finished) return 1;
    if (shape->instance == NULL || shape->instance->magic != INSTANCE_MAGIC) {
        fail(SILEX_FONT_INTERNAL, "font shaping requires a live instance");
        return 0;
    }

    hb_buffer_t *buffer = hb_buffer_create();
    if (buffer == hb_buffer_get_empty()) {
        fail(SILEX_FONT_OUT_OF_MEMORY, "could not allocate the HarfBuzz shaping buffer");
        return 0;
    }
    hb_buffer_set_cluster_level(buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);
    hb_buffer_add_utf8(buffer, (const char *)shape->text, (int)shape->byte_count, 0, (int)shape->byte_count);
    hb_direction_t requested_direction = to_hb_direction(shape->requested_direction);
    if (requested_direction != HB_DIRECTION_INVALID) hb_buffer_set_direction(buffer, requested_direction);
    if (shape->requested_script != 0) hb_buffer_set_script(buffer, hb_script_from_iso15924_tag(shape->requested_script));
    hb_buffer_set_language(buffer, hb_language_from_string(shape->requested_language, -1));
    hb_buffer_guess_segment_properties(buffer);
    if (hb_buffer_get_direction(buffer) == HB_DIRECTION_INVALID) hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
    if (hb_buffer_get_script(buffer) == HB_SCRIPT_INVALID) hb_buffer_set_script(buffer, HB_SCRIPT_COMMON);
    if (hb_buffer_get_language(buffer) == HB_LANGUAGE_INVALID) hb_buffer_set_language(buffer, hb_language_from_string("und", -1));

    hb_shape(shape->instance->hb_font, buffer, shape->features, shape->feature_count);
    if (!hb_buffer_allocation_successful(buffer)) {
        hb_buffer_destroy(buffer);
        fail(SILEX_FONT_OUT_OF_MEMORY, "HarfBuzz could not allocate the shaped glyph buffer");
        return 0;
    }
    unsigned int glyph_count = 0;
    const hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buffer, &glyph_count);
    const hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer, NULL);
    if (glyph_count > UINT32_MAX || (glyph_count > 0 && (infos == NULL || positions == NULL))) {
        hb_buffer_destroy(buffer);
        fail(SILEX_FONT_INTERNAL, "HarfBuzz returned an invalid shaped glyph buffer");
        return 0;
    }
    if (glyph_count > 0) {
        shape->glyphs = calloc(glyph_count, sizeof(*shape->glyphs));
        if (shape->glyphs == NULL) {
            hb_buffer_destroy(buffer);
            fail(SILEX_FONT_OUT_OF_MEMORY, "could not copy the shaped glyphs");
            return 0;
        }
    }

    uint32_t *cluster_starts = NULL;
    if (glyph_count > 0) {
        cluster_starts = malloc(glyph_count * sizeof(*cluster_starts));
        if (cluster_starts == NULL) {
            free(shape->glyphs);
            shape->glyphs = NULL;
            hb_buffer_destroy(buffer);
            fail(SILEX_FONT_OUT_OF_MEMORY, "could not copy the shaped clusters");
            return 0;
        }
        for (unsigned int index = 0; index < glyph_count; ++index) cluster_starts[index] = infos[index].cluster;
        qsort(cluster_starts, glyph_count, sizeof(*cluster_starts), compare_u32);
    }

    unsigned int unique_count = 0;
    for (unsigned int index = 0; index < glyph_count; ++index) {
        if (unique_count == 0 || cluster_starts[index] != cluster_starts[unique_count - 1]) {
            cluster_starts[unique_count++] = cluster_starts[index];
        }
    }

    int32_t pen_x = 0;
    int32_t pen_y = 0;
    for (unsigned int index = 0; index < glyph_count; ++index) {
        FontGlyphRecord *glyph = &shape->glyphs[index];
        glyph->id = infos[index].codepoint;
        glyph->cluster_start = infos[index].cluster;
        glyph->cluster_end = (uint32_t)shape->byte_count;
        for (unsigned int cluster = 0; cluster < unique_count; ++cluster) {
            if (cluster_starts[cluster] == glyph->cluster_start && cluster + 1 < unique_count) {
                glyph->cluster_end = cluster_starts[cluster + 1];
                break;
            }
        }
        glyph->x_advance = positions[index].x_advance;
        glyph->y_advance = positions[index].y_advance;
        glyph->x_offset = positions[index].x_offset;
        glyph->y_offset = positions[index].y_offset;
        glyph->x_origin = pen_x + glyph->x_offset;
        glyph->y_origin = pen_y + glyph->y_offset;
        hb_glyph_extents_t extents;
        if (hb_font_get_glyph_extents(shape->instance->hb_font, glyph->id, &extents) &&
            (extents.width != 0 || extents.height != 0)) {
            glyph->has_ink = 1;
            glyph->x_bearing = extents.x_bearing;
            glyph->y_bearing = extents.y_bearing;
            glyph->width = extents.width;
            glyph->height = extents.height;
        }
        pen_x += glyph->x_advance;
        pen_y += glyph->y_advance;
    }
    free(cluster_starts);

    shape->glyph_count = glyph_count;
    shape->x_advance = pen_x;
    shape->y_advance = pen_y;
    shape->direction = from_hb_direction(hb_buffer_get_direction(buffer));
    shape->script = hb_script_to_iso15924_tag(hb_buffer_get_script(buffer));
    const char *language = hb_language_to_string(hb_buffer_get_language(buffer));
    (void)snprintf(shape->language, sizeof(shape->language), "%s", language == NULL ? "und" : language);
    shape->finished = 1;
    ++shape->instance->shape_count;
    hb_buffer_destroy(buffer);
    return 1;
}

void silex_font_shape_destroy(void *raw) {
    if (raw == NULL) return;
    FontShape *shape = raw;
    if (shape->magic != SHAPE_MAGIC) {
        fail(SILEX_FONT_INTERNAL, "invalid font shape handle");
        return;
    }
    shape->magic = 0;
    free(shape->glyphs);
    free(shape->features);
    free(shape->text);
    free(shape);
}

uint32_t silex_font_shape_glyph_count(const void *raw) { const FontShape *s = checked_finished_shape(raw); return s == NULL ? 0 : s->glyph_count; }
int32_t silex_font_shape_direction(const void *raw) { const FontShape *s = checked_finished_shape(raw); return s == NULL ? 0 : s->direction; }
uint32_t silex_font_shape_script(const void *raw) { const FontShape *s = checked_finished_shape(raw); return s == NULL ? 0 : s->script; }
const char *silex_font_shape_language(const void *raw) { const FontShape *s = checked_finished_shape(raw); return s == NULL ? "" : s->language; }
uint32_t silex_font_shape_glyph_id(const void *raw, uint32_t index) { const FontGlyphRecord *g = checked_glyph(raw, index); return g == NULL ? 0 : g->id; }
uint32_t silex_font_shape_glyph_cluster_start(const void *raw, uint32_t index) { const FontGlyphRecord *g = checked_glyph(raw, index); return g == NULL ? 0 : g->cluster_start; }
uint32_t silex_font_shape_glyph_cluster_end(const void *raw, uint32_t index) { const FontGlyphRecord *g = checked_glyph(raw, index); return g == NULL ? 0 : g->cluster_end; }
int32_t silex_font_shape_glyph_x_advance(const void *raw, uint32_t index) { const FontGlyphRecord *g = checked_glyph(raw, index); return g == NULL ? 0 : g->x_advance; }
int32_t silex_font_shape_glyph_y_advance(const void *raw, uint32_t index) { const FontGlyphRecord *g = checked_glyph(raw, index); return g == NULL ? 0 : g->y_advance; }
int32_t silex_font_shape_glyph_x_offset(const void *raw, uint32_t index) { const FontGlyphRecord *g = checked_glyph(raw, index); return g == NULL ? 0 : g->x_offset; }
int32_t silex_font_shape_glyph_y_offset(const void *raw, uint32_t index) { const FontGlyphRecord *g = checked_glyph(raw, index); return g == NULL ? 0 : g->y_offset; }
int32_t silex_font_shape_glyph_x_origin(const void *raw, uint32_t index) { const FontGlyphRecord *g = checked_glyph(raw, index); return g == NULL ? 0 : g->x_origin; }
int32_t silex_font_shape_glyph_y_origin(const void *raw, uint32_t index) { const FontGlyphRecord *g = checked_glyph(raw, index); return g == NULL ? 0 : g->y_origin; }
int32_t silex_font_shape_glyph_has_ink(const void *raw, uint32_t index) { const FontGlyphRecord *g = checked_glyph(raw, index); return g == NULL ? 0 : g->has_ink; }
int32_t silex_font_shape_glyph_x_bearing(const void *raw, uint32_t index) { const FontGlyphRecord *g = checked_glyph(raw, index); return g == NULL ? 0 : g->x_bearing; }
int32_t silex_font_shape_glyph_y_bearing(const void *raw, uint32_t index) { const FontGlyphRecord *g = checked_glyph(raw, index); return g == NULL ? 0 : g->y_bearing; }
int32_t silex_font_shape_glyph_width(const void *raw, uint32_t index) { const FontGlyphRecord *g = checked_glyph(raw, index); return g == NULL ? 0 : g->width; }
int32_t silex_font_shape_glyph_height(const void *raw, uint32_t index) { const FontGlyphRecord *g = checked_glyph(raw, index); return g == NULL ? 0 : g->height; }
int32_t silex_font_shape_x_advance(const void *raw) { const FontShape *s = checked_finished_shape(raw); return s == NULL ? 0 : s->x_advance; }
int32_t silex_font_shape_y_advance(const void *raw) { const FontShape *s = checked_finished_shape(raw); return s == NULL ? 0 : s->y_advance; }

uint32_t silex_font_instance_shape_count(const void *raw) {
    const FontInstance *instance = raw;
    if (instance == NULL || instance->magic != INSTANCE_MAGIC) {
        fail(SILEX_FONT_INTERNAL, "a live font instance is required");
        return 0;
    }
    return instance->shape_count;
}

int32_t silex_font_last_error_code(void) { return error_code; }
const char *silex_font_last_error_detail(void) { return error_detail; }

uint32_t silex_font_live_library_count(void) { lock_state(); uint32_t value = state.libraries; unlock_state(); return value; }
uint32_t silex_font_live_face_count(void) { lock_state(); uint32_t value = state.faces; unlock_state(); return value; }
uint32_t silex_font_live_instance_count(void) { lock_state(); uint32_t value = state.instances; unlock_state(); return value; }
