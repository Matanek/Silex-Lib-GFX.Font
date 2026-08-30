# Font fixtures

These fixtures are committed so format and variation coverage does not depend
on fonts installed on the host.

- `Minimal.ttf` is a Latin subset of the bundled variable Noto Sans asset.
- `NotoSansMono-Latin.ttf` is a Latin subset produced with
  `hb-subset --unicodes=U+0020-007E` from the bundled Noto Sans Mono asset.
- `TwoFaces.ttc` combines those two subsets. Rebuild it with
  `python3 make_collection.py Minimal.ttf NotoSansMono-Latin.ttf TwoFaces.ttc`.
- `SourceCodePro-Regular.otf` is Adobe Source Code Pro 2.042 from the official
  `release/OTF` directory and supplies cubic CFF outlines.

Noto fixtures use `Licenses/NotoFonts.txt`. Source Code Pro uses
`Licenses/SourceCodePro.txt`.

SHA-256 values:

```text
09e75a480cc51b05afd5530df59458b1020ac1a709cba7c93b1b52ac7b517bb7  Minimal.ttf
2f6b5d003682a161028c7ca5896d38e7770fb3963f3547cee7510c7f43b7da10  NotoSansMono-Latin.ttf
da13544b3ad9043344f2b95a010cbfb3ed31b1cde474dd2f1f690f082208988d  TwoFaces.ttc
9f9664e2edf6f045c11e774f9bd0be6993971f2544a39061a5ce478b96b051f8  SourceCodePro-Regular.otf
```
