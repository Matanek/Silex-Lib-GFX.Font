# Hinting raster benchmark

These measurements exercise the public `GlyphRun.rasterize` path with 14-unit
Noto Sans Mono text. Every fifth row contains Unicode accents and combining
marks; the other rows contain dense ASCII source-like text. The benchmark
separates a new instance, a retained run coverage, composition from warm glyph
bitmaps, and a burst that changes ten rows.

## Environment

- Date: 2026-08-31
- Host: macOS Darwin 25.6.0, ARM64 (`T6030` kernel platform)
- Silex: 0.43.0
- Mode: Release (`--release`)
- FreeType: 2.14.3
- HarfBuzz: 14.2.1
- Repetitions: 5 per phase

The command is:

```sh
silex run Benchmarks/RasterTerminal.sx --release
```

## Results

| Cells | Phase | Samples (ms) | Mean (ms) | Standard deviation (ms) |
| --- | --- | --- | ---: | ---: |
| 80×24 | cold shape + raster + compose | 8.992, 8.131, 8.072, 8.100, 8.017 | 8.262 | 0.367 |
| 80×24 | retained run coverage | 3.236, 3.079, 3.116, 2.954, 3.046 | 3.086 | 0.092 |
| 80×24 | warm glyph recomposition | 7.146, 7.240, 7.329, 7.249, 7.372 | 7.267 | 0.078 |
| 80×24 | ten-row update burst | 3.043, 3.109, 3.158, 3.099, 3.172 | 3.116 | 0.046 |
| 200×60 | cold shape + raster + compose | 35.701, 35.097, 35.273, 35.900, 35.793 | 35.553 | 0.312 |
| 200×60 | retained run coverage | 12.888, 12.877, 12.701, 12.915, 12.692 | 12.815 | 0.097 |
| 200×60 | warm glyph recomposition | 35.806, 36.454, 37.175, 38.356, 39.038 | 37.366 | 1.190 |
| 200×60 | ten-row update burst | 6.473, 6.537, 6.555, 6.612, 6.629 | 6.561 | 0.056 |

These numbers measure CPU shaping, alpha composition, and the copy into the
public coverage array. They do not include a Canvas upload or GPU draw. The
corresponding boundary tests assert that retained lines do not rerasterize
glyphs, that transient coverage handles return to zero, and that glyph and run
caches remain below their separate 16 MiB and 8 MiB ceilings. Canvas migration
must measure upload and drawing separately before replacing its SDL_ttf path.
