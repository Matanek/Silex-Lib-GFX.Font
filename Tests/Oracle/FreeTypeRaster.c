#include <stdint.h>
#include <stdio.h>

#include <ft2build.h>
#include FT_FREETYPE_H

int main(int argument_count, char **arguments) {
    if (argument_count != 2) {
        fprintf(stderr, "usage: FreeTypeRaster /path/to/font.ttf\n");
        return 2;
    }
    FT_Library library = NULL;
    FT_Face face = NULL;
    FT_Error error = FT_Init_FreeType(&library);
    if (error == FT_Err_Ok) error = FT_New_Face(library, arguments[1], 0, &face);
    if (error == FT_Err_Ok) error = FT_Set_Char_Size(face, 0, 17 * 64, 72, 72);
    FT_UInt glyph = error == FT_Err_Ok ? FT_Get_Char_Index(face, 'A') : 0;
    if (error == FT_Err_Ok) {
        error = FT_Load_Glyph(face, glyph, FT_LOAD_DEFAULT | FT_LOAD_TARGET_NORMAL);
    }
    if (error == FT_Err_Ok && face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
        error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
    }
    if (error != FT_Err_Ok || face->glyph->bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) {
        fprintf(stderr, "FreeType raster oracle failed: %d\n", error);
        if (face != NULL) FT_Done_Face(face);
        if (library != NULL) FT_Done_FreeType(library);
        return 1;
    }
    FT_Bitmap *bitmap = &face->glyph->bitmap;
    uint64_t sum = 0;
    uint64_t weighted = 0;
    for (unsigned int row = 0; row < bitmap->rows; ++row) {
        const uint8_t *pixels = bitmap->buffer + (size_t)row * (size_t)bitmap->pitch;
        for (unsigned int column = 0; column < bitmap->width; ++column) {
            uint64_t value = pixels[column];
            sum += value;
            weighted += value * ((uint64_t)row * bitmap->width + column + 1);
        }
    }
    printf(
        "freetype-2.14.3 glyph=%u width=%u height=%u left=%d top=%d sum=%llu weighted=%llu\n",
        glyph,
        bitmap->width,
        bitmap->rows,
        face->glyph->bitmap_left,
        face->glyph->bitmap_top,
        (unsigned long long)sum,
        (unsigned long long)weighted
    );
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return 0;
}
