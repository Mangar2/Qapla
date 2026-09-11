# CityHash, as the Syzygy generator uses it

Copied verbatim from Ronald de Man's table generator, `src/city-c.c`, `src/city-c.h`,
`src/citycrc.h` and `src/crc32c.c` of <https://github.com/syzygy1/tb>, which in turn took
CityHash from Google. CityHash is under the MIT licence, which the headers carry.

It is here for one reason: the sixteen check bytes at the end of a table file are
`CityHashCrc128` over the `CityHashCrc256` of every 16 MB chunk of the file, and that is
what `tbcheck` verifies. Writing anything else there would make our files fail his tool.
The code that uses it is `checksumOf()` in [../tbwrite.cpp](../tbwrite.cpp).

Three deliberate deviations from the copy:

- `city-c.c` is named `city-c.cpp` here, so that the project is built by one compiler
  instead of two. The source needs no change for it: it compiles as C++ without a warning
  under `-Wall -Wextra`, and the files it produces are byte for byte the ones the C build
  produced.
- `crc32c.c` is named `crc32c.inc`. It is `#include`d into `city-c.cpp` rather than
  compiled on its own, and the build globs every source file in the repository - under its
  original name it would be compiled twice and `_mm_crc32_u64` would be defined twice.
- Nothing else. In particular the `#ifdef __SSE_4_2__` in `city-c.cpp` is left as it is,
  although the macro the compiler actually defines is `__SSE4_2__`: the branch it guards
  therefore never fires and the software implementation of `_mm_crc32_u64` in `crc32c.inc`
  is always used. That is how de Man's own generator computed the checksums of the tables
  we compare against, the two paths produce the same values by construction, and a few
  microseconds per file are not worth a difference to the original.
