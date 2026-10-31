# Third-party code and provenance

The offline `xlive.dll` in `xlive/` (the Games for Windows LIVE replacement installed
on Steam/retail copies) is a standalone adaptation of the XLiveLess component maintained in
[ThirteenAG/Ultimate-ASI-Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader).
It is not an original implementation of the complete Games for Windows LIVE API.

The imported source is pinned to commit
`56fa0b03920f2a71174126e03a05c77d48b74744`:

- `source/compat/xlive/xliveless.cpp`
- `source/compat/xlive/xliveless.h`
- `source/compat/xlive/xliveless.rc`
- `source/compat/xlive/resource.h`
- The XLIVE export section of `source/loader/x86.def`.

The upstream repository uses the MIT license. Its complete notice is retained in
`licenses/Ultimate-ASI-Loader-MIT.txt`.
The original `xliveless.cpp` also states:

> -No Copyright- 2010 Stanislav "listener" Golovin
> This file donated to the public domain

These notices are retained. No GOG DLL, Microsoft XLIVE DLL, game executable,
game asset, GOG-derived disassembly or decompiler output is included in the source
repository or generated release packages. The DLL is compiled from the included
open source. Windows import libraries and the MSVC toolchain are build prerequisites.

The standalone DLL host also follows the upstream `LoadXLive` behavior in
`source/loader/proxy/proxy.cpp`: the main image's `.text` and `.rdata` pages become
writable for legacy GFWL compatibility. This does not change the executable file.

Local changes include a standalone build, module identity export, bounded user
and buffer handling, real notification event handles, local profile storage in
LocalAppData, bounded profile reads and atomic per-setting writes, native API tests
and the installer integration. These changes are MIT licensed.

The project is unofficial and is not affiliated with GOG, Microsoft, IO Interactive,
Square Enix or the upstream authors.
