#include "SilexFont.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include <hb.h>
#include <hb-ot.h>

#if defined(_MSC_VER)
#define SILEX_FONT_THREAD_LOCAL __declspec(thread)
#else
#define SILEX_FONT_THREAD_LOCAL _Thread_local
#endif

#define SILEX_FONT_LIBRARY_MAGIC UINT32_C(0x53464c42)
#define SILEX_FONT_FACE_MAGIC UINT32_C(0x53464643)

typedef struct SilexFontLibraryHandle {
    uint32_t magic;
} SilexFontLibraryHandle;

typedef struct SilexFontFaceHandle {
    uint32_t magic;
    uint8_t *bytes;
    size_t byte_count;
    FT_Face freetype_face;
    hb_blob_t *harfbuzz_blob;
    hb_face_t *harfbuzz_face;
    int32_t face_index;
    int32_t face_count;
} SilexFontFaceHandle;

typedef struct SilexFontState {
    FT_Library freetype;
    uint32_t libraries;
    uint32_t faces;
} SilexFontState;

static SilexFontState silex_font_state;
static atomic_flag silex_font_state_lock = ATOMIC_FLAG_INIT;
static SILEX_FONT_THREAD_LOCAL int32_t silex_font_error_code = SILEX_FONT_OK;
static SILEX_FONT_THREAD_LOCAL char silex_font_error_detail[256];

static void state_lock(void) {
    while (atomic_flag_test_and_set_explicit(
        &silex_font_state_lock,
        memory_order_acquire
    )) {}
}

static void state_unlock(void) {
    atomic_flag_clear_explicit(&silex_font_state_lock, memory_order_release);
}

static void clear_error(void) {
    silex_font_error_code = SILEX_FONT_OK;
    silex_font_error_detail[0] = '\0';
}

static void set_error(int32_t code, const char *detail) {
    silex_font_error_code = code;
    if (detail == NULL || detail[0] == '\0') {
        detail = "unknown font boundary error";
    }
    (void)snprintf(
        silex_font_error_detail,
        sizeof(silex_font_error_detail),
        "%s",
        detail
    );
}

static int32_t map_freetype_error(FT_Error error) {
    if (error == FT_Err_Out_Of_Memory) {
        return SILEX_FONT_OUT_OF_MEMORY;
    }
    if (error == FT_Err_Unimplemented_Feature) {
        return SILEX_FONT_UNSUPPORTED;
    }
    return SILEX_FONT_INVALID_DATA;
}

static void set_freetype_error(const char *action, FT_Error error) {
    const char *detail = FT_Error_String(error);
    char message[256];
    if (detail == NULL) {
        (void)snprintf(message, sizeof(message), "%s (FreeType error %d)", action, error);
    } else {
        (void)snprintf(message, sizeof(message), "%s: %s", action, detail);
    }
    set_error(map_freetype_error(error), message);
}

static void finalize_state_if_unused(void) {
    if (
        silex_font_state.freetype != NULL &&
        silex_font_state.libraries == 0 &&
        silex_font_state.faces == 0
    ) {
        FT_Done_FreeType(silex_font_state.freetype);
        silex_font_state.freetype = NULL;
    }
}

uint32_t silex_font_abi_version(void) {
    return SILEX_FONT_ABI_VERSION;
}

uint32_t silex_font_abi_pointer_size(void) {
    return (uint32_t)sizeof(void *);
}

uint32_t silex_font_abi_size_size(void) {
    return (uint32_t)sizeof(size_t);
}

uint32_t silex_font_abi_pointer_alignment(void) {
    return (uint32_t)_Alignof(void *);
}

void *silex_font_library_create(void) {
    clear_error();
    SilexFontLibraryHandle *handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        set_error(SILEX_FONT_OUT_OF_MEMORY, "could not allocate the font library handle");
        return NULL;
    }

    state_lock();
    if (silex_font_state.freetype == NULL) {
        FT_Error error = FT_Init_FreeType(&silex_font_state.freetype);
        if (error != FT_Err_Ok) {
            state_unlock();
            free(handle);
            set_freetype_error("could not initialize FreeType", error);
            return NULL;
        }
    }
    ++silex_font_state.libraries;
    state_unlock();

    handle->magic = SILEX_FONT_LIBRARY_MAGIC;
    return handle;
}

void silex_font_library_destroy(void *raw_library) {
    if (raw_library == NULL) {
        return;
    }
    SilexFontLibraryHandle *library = raw_library;
    if (library->magic != SILEX_FONT_LIBRARY_MAGIC) {
        set_error(SILEX_FONT_INTERNAL, "invalid font library handle");
        return;
    }
    library->magic = 0;
    free(library);

    state_lock();
    if (silex_font_state.libraries > 0) {
        --silex_font_state.libraries;
    }
    finalize_state_if_unused();
    state_unlock();
}

void *silex_font_face_create(
    void *raw_library,
    const uint8_t *bytes,
    size_t byte_count,
    int32_t face_index
) {
    clear_error();
    SilexFontLibraryHandle *library = raw_library;
    if (library == NULL || library->magic != SILEX_FONT_LIBRARY_MAGIC) {
        set_error(SILEX_FONT_INTERNAL, "a live font library is required");
        return NULL;
    }
    if (bytes == NULL || byte_count == 0) {
        set_error(SILEX_FONT_EMPTY_DATA, "font data cannot be empty");
        return NULL;
    }
    if (face_index < 0) {
        set_error(SILEX_FONT_FACE_INDEX_OUT_OF_RANGE, "font face index cannot be negative");
        return NULL;
    }

    SilexFontFaceHandle *face = calloc(1, sizeof(*face));
    if (face == NULL) {
        set_error(SILEX_FONT_OUT_OF_MEMORY, "could not allocate the font face handle");
        return NULL;
    }
    face->bytes = malloc(byte_count);
    if (face->bytes == NULL) {
        free(face);
        set_error(SILEX_FONT_OUT_OF_MEMORY, "could not retain the font bytes");
        return NULL;
    }
    memcpy(face->bytes, bytes, byte_count);
    face->byte_count = byte_count;

    FT_Face probe = NULL;
    FT_Error error = FT_New_Memory_Face(
        silex_font_state.freetype,
        face->bytes,
        (FT_Long)face->byte_count,
        -1,
        &probe
    );
    if (error != FT_Err_Ok) {
        free(face->bytes);
        free(face);
        set_freetype_error("could not read the font data", error);
        return NULL;
    }
    face->face_count = (int32_t)probe->num_faces;
    FT_Done_Face(probe);
    if (face_index >= face->face_count) {
        char message[160];
        (void)snprintf(
            message,
            sizeof(message),
            "font face index %d is outside the collection of %d face(s)",
            face_index,
            face->face_count
        );
        free(face->bytes);
        free(face);
        set_error(SILEX_FONT_FACE_INDEX_OUT_OF_RANGE, message);
        return NULL;
    }

    error = FT_New_Memory_Face(
        silex_font_state.freetype,
        face->bytes,
        (FT_Long)face->byte_count,
        face_index,
        &face->freetype_face
    );
    if (error != FT_Err_Ok) {
        free(face->bytes);
        free(face);
        set_freetype_error("could not open the requested font face", error);
        return NULL;
    }

    face->harfbuzz_blob = hb_blob_create(
        (const char *)face->bytes,
        (unsigned int)face->byte_count,
        HB_MEMORY_MODE_READONLY,
        NULL,
        NULL
    );
    face->harfbuzz_face = hb_face_create(face->harfbuzz_blob, (unsigned int)face_index);
    if (
        face->harfbuzz_blob == hb_blob_get_empty() ||
        face->harfbuzz_face == hb_face_get_empty() ||
        hb_face_get_upem(face->harfbuzz_face) == 0
    ) {
        hb_face_destroy(face->harfbuzz_face);
        hb_blob_destroy(face->harfbuzz_blob);
        FT_Done_Face(face->freetype_face);
        free(face->bytes);
        free(face);
        set_error(SILEX_FONT_INVALID_DATA, "HarfBuzz could not read the requested font face");
        return NULL;
    }

    face->magic = SILEX_FONT_FACE_MAGIC;
    face->face_index = face_index;
    state_lock();
    ++silex_font_state.faces;
    state_unlock();
    return face;
}

void silex_font_face_destroy(void *raw_face) {
    if (raw_face == NULL) {
        return;
    }
    SilexFontFaceHandle *face = raw_face;
    if (face->magic != SILEX_FONT_FACE_MAGIC) {
        set_error(SILEX_FONT_INTERNAL, "invalid font face handle");
        return;
    }
    face->magic = 0;
    hb_face_destroy(face->harfbuzz_face);
    hb_blob_destroy(face->harfbuzz_blob);
    FT_Done_Face(face->freetype_face);
    free(face->bytes);
    free(face);

    state_lock();
    if (silex_font_state.faces > 0) {
        --silex_font_state.faces;
    }
    finalize_state_if_unused();
    state_unlock();
}

static const SilexFontFaceHandle *checked_face(const void *raw_face) {
    const SilexFontFaceHandle *face = raw_face;
    if (face == NULL || face->magic != SILEX_FONT_FACE_MAGIC) {
        set_error(SILEX_FONT_INTERNAL, "a live font face is required");
        return NULL;
    }
    return face;
}

int32_t silex_font_face_index(const void *raw_face) {
    const SilexFontFaceHandle *face = checked_face(raw_face);
    return face == NULL ? -1 : face->face_index;
}

int32_t silex_font_face_count(const void *raw_face) {
    const SilexFontFaceHandle *face = checked_face(raw_face);
    return face == NULL ? 0 : face->face_count;
}

int32_t silex_font_face_glyph_count(const void *raw_face) {
    const SilexFontFaceHandle *face = checked_face(raw_face);
    return face == NULL ? 0 : (int32_t)face->freetype_face->num_glyphs;
}

int32_t silex_font_face_units_per_em(const void *raw_face) {
    const SilexFontFaceHandle *face = checked_face(raw_face);
    return face == NULL ? 0 : (int32_t)face->freetype_face->units_per_EM;
}

uint32_t silex_font_face_capabilities(const void *raw_face) {
    const SilexFontFaceHandle *face = checked_face(raw_face);
    if (face == NULL) {
        return 0;
    }
    uint32_t capabilities = SILEX_FONT_CAPABILITY_SHAPING;
    if (FT_IS_SCALABLE(face->freetype_face)) {
        capabilities |= SILEX_FONT_CAPABILITY_OUTLINES;
    }
    if (FT_HAS_MULTIPLE_MASTERS(face->freetype_face)) {
        capabilities |= SILEX_FONT_CAPABILITY_VARIATIONS;
    }
    if (FT_HAS_COLOR(face->freetype_face)) {
        capabilities |= SILEX_FONT_CAPABILITY_COLOR;
    }
    return capabilities;
}

const char *silex_font_face_family_name(const void *raw_face) {
    const SilexFontFaceHandle *face = checked_face(raw_face);
    if (face == NULL || face->freetype_face->family_name == NULL) {
        return "";
    }
    return face->freetype_face->family_name;
}

const char *silex_font_face_style_name(const void *raw_face) {
    const SilexFontFaceHandle *face = checked_face(raw_face);
    if (face == NULL || face->freetype_face->style_name == NULL) {
        return "";
    }
    return face->freetype_face->style_name;
}

int32_t silex_font_last_error_code(void) {
    return silex_font_error_code;
}

const char *silex_font_last_error_detail(void) {
    return silex_font_error_detail;
}

uint32_t silex_font_live_library_count(void) {
    state_lock();
    uint32_t result = silex_font_state.libraries;
    state_unlock();
    return result;
}

uint32_t silex_font_live_face_count(void) {
    state_lock();
    uint32_t result = silex_font_state.faces;
    state_unlock();
    return result;
}
