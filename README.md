# GFX.Font

`GFX.Font` provides GFX's portable fonts. It loads TTF, OTF/CFF, and TTC faces
from bytes or a file, exposes their names, Unicode coverage, metrics, and
variations, then creates immutable instances at a logical size. An instance
shapes a homogeneous Unicode run into positioned glyphs, UTF-8 clusters, and
logical measurements, then exposes their normalized vector outlines or hinted
alpha coverage without depending on a renderer.

```text
silex install GFX.Font
```

## Documentation

- [French documentation](Docs/FR/README.md)
- [English documentation](Docs/EN/README.md)
- [Reproduce the native artifacts](Native/README.md)

The package requires Silex 0.43.0 or newer. It distributes Noto Sans Variable,
Noto Sans Mono, and the retro pixel font Departure Mono, does not discover
system fonts, and exposes no FreeType or HarfBuzz handles.
