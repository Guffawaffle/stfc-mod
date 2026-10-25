#!/usr/bin/env python3
"""Verify the Windows proxy's architecture, exports, static CRT and provenance marker."""
import argparse
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_MARKER = "stfc-identity-v1;distribution=netniv.stfc-community-mod;"
CRT_PREFIXES = ("msvcp", "vcruntime", "ucrtbase", "api-ms-win-crt-")
PROFILES_EXPORTS = {"STFCProfilesBrowserLaunchW", "STFCProfilesExplicitLaunchContractV1"}


def definition_exports(path: Path) -> set[str]:
    names = set()
    for line in path.read_text().split("EXPORTS", 1)[1].splitlines():
        fields = line.split(";", 1)[0].split()
        if not fields:
            continue
        if fields[1:] not in ([], ["DATA"]):
            raise SystemExit(f"Unexpected export declaration: {line}")
        if fields[0] in names:
            raise SystemExit(f"Duplicate export declaration: {fields[0]}")
        names.add(fields[0])
    return names


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", nargs="?", default="zig-out/bin/version.dll")
    parser.add_argument("--identity", help="Expected complete stfc-identity-v1 version-resource marker")
    parser.add_argument(
        "--profiles-host", action="store_true",
        help="Expect the explicitly enabled Profiles host's two additional exports",
    )
    args = parser.parse_args()
    data = Path(args.binary).read_bytes()

    def u16(position: int) -> int:
        return struct.unpack_from("<H", data, position)[0]

    def u32(position: int) -> int:
        return struct.unpack_from("<I", data, position)[0]

    # Offsets follow the PE/COFF layout: e_lfanew at 0x3c points at the "PE\0\0" signature, the COFF header follows
    # it, then the PE32+ optional header, whose data directories start at +112 (exports) and +120 (imports).
    pe = u32(60)
    if data[:2] != b"MZ" or data[pe:pe + 4] != b"PE\0\0":
        raise SystemExit("Expected a PE executable")
    if u16(pe + 4) != 0x8664:
        raise SystemExit("Expected x86_64 machine architecture")
    section_count = u16(pe + 6)
    optional_size = u16(pe + 20)
    optional = pe + 24
    if u16(optional) != 0x20B:
        raise SystemExit("Expected a PE32+ optional header")

    # Each 40-byte section header holds VirtualSize, VirtualAddress, SizeOfRawData and PointerToRawData at +8.
    section_table = optional + optional_size
    sections = [struct.unpack_from("<IIII", data, section_table + i * 40 + 8) for i in range(section_count)]

    def offset(rva: int) -> int:
        for virtual_size, virtual_address, raw_size, raw_address in sections:
            if virtual_address <= rva < virtual_address + max(virtual_size, raw_size):
                return raw_address + rva - virtual_address
        raise ValueError(f"Unmapped RVA {rva:#x}")

    def string(rva: int) -> str:
        start = offset(rva)
        return data[start:data.index(b"\0", start)].decode("ascii")

    # The import directory is a zero-terminated array of 20-byte descriptors; the DLL name's RVA is at +12.
    imports = []
    position = offset(u32(optional + 120))
    while any(data[position:position + 20]):
        imports.append(string(u32(position + 12)))
        position += 20
    if any(name.lower().startswith(CRT_PREFIXES) for name in imports):
        raise SystemExit(f"Dynamic CRT dependency: {imports}")

    # The export directory holds NumberOfNames at +24 and AddressOfNames, a table of name RVAs, at +32.
    export_directory = offset(u32(optional + 112))
    function_count = u32(export_directory + 20)
    name_count = u32(export_directory + 24)
    address_table = offset(u32(export_directory + 28))
    name_table = offset(u32(export_directory + 32))
    exports = {string(u32(name_table + i * 4)) for i in range(name_count)}
    expected = definition_exports(ROOT / "src/platform/windows/version.def")
    if len(expected) != 16:
        raise SystemExit("The base Windows proxy must declare exactly 16 exports")
    if args.profiles_host:
        expected |= PROFILES_EXPORTS
        host_definition = definition_exports(ROOT / "src/platform/windows/profiles-version.def")
        if host_definition != expected:
            raise SystemExit(
                "Profiles host export declaration must contain the base 16 and exactly its two contract exports"
            )
    if exports != expected:
        raise SystemExit(f"Export mismatch: missing={expected - exports}, extra={exports - expected}")
    # Count actual nonempty address slots too, so unnamed ordinal exports cannot
    # hide outside the expected named surface. A DATA marker is an export slot.
    live_exports = sum(u32(address_table + i * 4) != 0 for i in range(function_count))
    if live_exports != len(expected) or name_count != len(expected):
        raise SystemExit(f"Export count mismatch: live={live_exports}, named={name_count}, expected={len(expected)}")

    marker = args.identity or DEFAULT_MARKER
    if marker.encode("utf-16le") not in data:
        raise SystemExit("Missing or incorrect UTF-16 version-resource provenance marker")
    mode = ", Profiles host" if args.profiles_host else ""
    print(f"Verified x86_64 proxy: {len(exports)} exports{mode}, static CRT, provenance marker")


if __name__ == "__main__":
    main()
