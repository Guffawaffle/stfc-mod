# Third-party dependencies

<p align="center">
  <img src="https://repository-images.githubusercontent.com/693298224/6a87716f-3dbb-48a5-80e0-709e0a1ad133" alt="STFC Community Mod">
</p>

`build.zig.zon` pins fetched libraries by immutable URL and Zig content hash.
Zig fetches a `protoc` executable only for the build host.
Preserve upstream licenses and notices when distributing source or binaries.

## Fetched libraries

| Dependency | Pin | License or notices | Purpose |
| --- | --- | --- | --- |
| [spdlog](https://github.com/gabime/spdlog) | 1.17.0 | MIT, bundled fmt notices | Logging |
| [toml++](https://github.com/marzer/tomlplusplus) | 3.4.0 | MIT | Settings |
| [JSON for Modern C++](https://github.com/nlohmann/json) | 3.12.0 | MIT | Sync |
| [protobuf](https://github.com/protocolbuffers/protobuf) | 35.1 | BSD-3-Clause | Lite runtime and host `protoc` |
| Bundled utf8_range | Protobuf 35.1 archive | MIT | UTF-8 validation |
| [Abseil](https://github.com/abseil/abseil-cpp) | 20250512.1 | Apache-2.0 | Protobuf runtime |
| [CPR](https://github.com/libcpr/cpr) | 1.14.2 | MIT | HTTP client |
| [curl](https://github.com/curl/curl) | 8.21.0 | curl license | Windows HTTPS with Schannel; macOS uses Apple's system libcurl |
| [zlib](https://github.com/madler/zlib) | 1.3.2 | Zlib | HTTP decompression |
| [Zydis](https://github.com/zyantific/zydis) | 4.1.0 | MIT | SPUD x86 disassembly |
| [Zycore](https://github.com/zyantific/zycore-c) | 1.5.0 | MIT | Zydis |
| [Capstone](https://github.com/capstone-engine/capstone) | 6.0.0-Alpha5 | BSD-3-Clause and component notices | SPUD ARM64 disassembly, on ARM64 builds only |
| [inifile-cpp](https://github.com/Rookfighter/inifile-cpp) | 7e49789411beba98ca2c98940c40f7ca7e4b23b5 | MIT | macOS loader |
| [librsync](https://github.com/librsync/librsync) | 2.3.4 | LGPL-2.1-or-later | Launcher delta patches |
| [STFC Profiles](https://github.com/Guffawaffle/stfc-profiles) | 1cd7a9ccdba5df872b95b9342c0eb8ef8a7cb6eb | GPL-3.0; retained derivation notices | Optional shared profile catalog, isolation adapter and matching CLI |
| [XMake recipes](https://github.com/xmake-io/xmake-repo) | fc22e7373c18d9920f54c6b93c19c2dd91fa0b1f | Apache-2.0; individual package notices | Optional Windows archive dependency producer |
| [libarchive](https://libarchive.org/) | 3.8.9 | BSD-2-Clause and retained component notices | Optional Windows Profiles archive support; source-pinned notices |
| [XZ Utils](https://tukaani.org/xz/) | 5.8.3 | liblzma: 0BSD; distribution component notices retained | Optional Windows Profiles LZMA support; source-pinned notices |

`-Dprofiles=true` enables the pinned Profiles sources. Ordinary builds do not fetch these optional packages.
The adapter uses this mod's SPUD and spdlog; the Profiles standalone bootstrap and vendored SPUD are not built.
The macOS launcher links its own catalog/C API archive, compiled against the SDK's libc++ headers.

On Windows only, Profiles also needs static libarchive 3.8.9, XZ/liblzma 5.8.3 and zlib 1.3.2.
The optional build uses XMake 3.0.8 and Visual Studio with the Profiles-owned libarchive recipe and pinned
XMake dependency recipes. libarchive is BSD-2-Clause; retain its bundled notices, XZ's `COPYING` and component
license notices, and zlib's license when distributing a Profiles-enabled binary. This producer runs only on a
Windows host, uses the static MSVC runtime and disables OpenSSL and unneeded archive dependencies.
The Windows source release SHA-256 pins are recorded in those immutable recipes, including libarchive's
`f5a6539059cf5e597dbeda37bfa4874b1e8dea063c8d93bf85a2b44af90a5bd4`.
Enabled Windows installs copy the pinned library notices under `share/stfc-profiles/notices/windows`.
The macOS Profiles core does not compile the Windows archive updater.

## Vendored sources

| Directory | Source and changes |
| --- | --- |
| `third_party/spud` | SPUD at `e4d0c78be89ef2587af33dd634ae5e8071935be7`. Unmodified `include/`, `src/`, and MIT `LICENSE`. |
| `third_party/libil2cpp` | Unity IL2CPP SDK headers. Two sparsehash includes use its public include path. Individual Unity notices are retained. |
| `third_party/sparsehash/include/sparsehash` | Header snapshot with BSD-3-Clause notices. |

SPUD uses its C++ sources only. Clang reads the hook context register inline, so the MSVC-only MASM helper is not assembled.
The build consumes IL2CPP headers only.

The macOS launcher extracts 7z updates with Apple's `/usr/bin/tar`, which uses the system libarchive.

## Generated files and SDKs

The protobuf C++ build selects the sync message roots and their transitive field types from the complete schemas.
It copies retained message descriptors verbatim, including field numbers and options.
The Abseil source manifest covers protobuf-lite's linked symbols on supported platforms, including each platform's waiter implementation.
Recheck Windows, ARM64 macOS, Intel macOS, and native test links when updating these pins.
Logging and HTTP sources compile in compatible batches to share header parsing. Abseil and protobuf-lite also use batches on macOS.
Their upstream source files are unchanged.

macOS links Apple's system libcurl and uses its SDK headers. Its curl and TLS versions follow macOS updates rather than `build.zig.zon`.
Windows compiles the pinned curl sources with Schannel. Neither platform builds OpenSSL.
Librsync uses a checked-in configuration header.
Ordinary builds need no Perl, CMake, or MASM. The optional Windows Profiles archive producer uses CMake through XMake.

`zig/toolchains.zon` pins the Windows SDK archive separately. Its temporary URL is not committed.
The macOS SDK pin is empty until an archive is supplied.
See [SDK provisioning](../CONTRIBUTING.md#provision-sdks) for archive formats and host requirements.
