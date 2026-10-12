# Changelog

## [Unreleased]

- Bundle monochrome Noto Emoji and Noto Sans CJK SC with licenses and provenance; Face.emoji() and Face.cjk() open the bundled faces without system font discovery.

- Adopts one `Result` declaration: an ordinary call returns the value or
  stops the program on failure; `try call()` keeps the `Result`. Existing calls
  storing results and propagating failures are made explicit. This revision
  requires the development toolchain with these `try` semantics; publication
  must coordinate compiler and package versions.
  `Face.try_open(bytes, index)` becomes `Face.decode(bytes, face_index)`;
  `Face.open(path, ...)` remains the file operation.
