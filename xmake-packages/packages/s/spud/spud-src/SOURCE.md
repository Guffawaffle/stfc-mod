# SPUD snapshot provenance

`include/`, `src/` and `LICENSE` are copied without modification from the
Ripper-provided SPUD snapshot associated with commit
`e4d0c78be89ef2587af33dd634ae5e8071935be7`.
The source repository is private; this does not claim a public release checkout.
The source tree in the pinned migration reference is
`b443aa0ef4ce1a43049fcb840478206c7e569d10`.

`CMakeLists.txt` is downstream-owned packaging, not an upstream build file.
It retains the existing Zydis 4.1.0 and optional Capstone 6.0.0-Alpha5 pins,
uses the new context assembly, and removes the obsolete AsmJit dependency.
The Xmake package version is deliberately distinct from the old package cache.
