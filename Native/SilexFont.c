#include "SilexFont.h"

#include <stdatomic.h>
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
} FontInstance;

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

int32_t silex_font_last_error_code(void) { return error_code; }
const char *silex_font_last_error_detail(void) { return error_detail; }

uint32_t silex_font_live_library_count(void) { lock_state(); uint32_t value = state.libraries; unlock_state(); return value; }
uint32_t silex_font_live_face_count(void) { lock_state(); uint32_t value = state.faces; unlock_state(); return value; }
uint32_t silex_font_live_instance_count(void) { lock_state(); uint32_t value = state.instances; unlock_state(); return value; }
