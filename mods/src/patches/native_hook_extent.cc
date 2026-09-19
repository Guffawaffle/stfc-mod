#include "native_hook_extent.h"

#if __APPLE__
#include <algorithm>
#include <capstone/capstone.h>
#include <cstring>
#include <dlfcn.h>
#include <limits>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <spdlog/spdlog.h>

namespace
{
// LC_FUNCTION_STARTS records ULEB128 deltas from __TEXT. Require an exact
// entry and a following entry: the last function has no proven extent here.
size_t FunctionExtent(const void* method)
{
  Dl_info info{};
  if (!method || !dladdr(method, &info) || !info.dli_fbase)
    return 0;
  const auto* header = static_cast<const mach_header_64*>(info.dli_fbase);
  if (header->magic != MH_MAGIC_64)
    return 0;
  const auto*                  cursor   = reinterpret_cast<const uint8_t*>(header + 1);
  const auto*                  end      = cursor + header->sizeofcmds;
  const segment_command_64*    text     = nullptr;
  const segment_command_64*    linkedit = nullptr;
  const linkedit_data_command* starts   = nullptr;
  for (uint32_t i = 0; i < header->ncmds; ++i) {
    if (size_t(end - cursor) < sizeof(load_command))
      return 0;
    const auto* command = reinterpret_cast<const load_command*>(cursor);
    if (command->cmdsize < sizeof(load_command) || command->cmdsize > size_t(end - cursor))
      return 0;
    if (command->cmd == LC_SEGMENT_64 && command->cmdsize >= sizeof(segment_command_64)) {
      const auto* segment = reinterpret_cast<const segment_command_64*>(command);
      if (std::strncmp(segment->segname, "__TEXT", 16) == 0)
        text = segment;
      if (std::strncmp(segment->segname, "__LINKEDIT", 16) == 0)
        linkedit = segment;
    }
    if (command->cmd == LC_FUNCTION_STARTS && command->cmdsize >= sizeof(linkedit_data_command))
      starts = reinterpret_cast<const linkedit_data_command*>(command);
    cursor += command->cmdsize;
  }
  if (!text || !linkedit || !starts || starts->dataoff < linkedit->fileoff)
    return 0;
  const uint64_t offset = starts->dataoff - linkedit->fileoff;
  if (offset > linkedit->filesize || starts->datasize > linkedit->filesize - offset || offset > linkedit->vmsize
      || starts->datasize > linkedit->vmsize - offset)
    return 0;
  const uintptr_t base   = reinterpret_cast<uintptr_t>(header);
  const uintptr_t target = reinterpret_cast<uintptr_t>(method);
  if (target < base || target - base >= text->vmsize)
    return 0;
  const auto slide = base - text->vmaddr;
  cursor           = reinterpret_cast<const uint8_t*>(slide + linkedit->vmaddr + offset);
  end              = cursor + starts->datasize;
  uint64_t address = 0;
  bool     found   = false;
  while (cursor < end) {
    uint64_t delta = 0;
    unsigned shift = 0;
    uint8_t  byte;
    do {
      if (cursor == end || shift >= 64)
        return 0;
      byte = *cursor++;
      if (shift == 63 && (byte & 0x7e))
        return 0;
      delta |= uint64_t(byte & 0x7f) << shift;
      shift += 7;
    } while (byte & 0x80);
    if (!delta || delta > text->vmsize - address)
      return 0;
    address += delta;
    if (found)
      return address - (target - base);
    if (address > target - base)
      return 0;
    found = address == target - base;
  }
  return 0;
}

bool HasPrologue(const void* method, size_t extent)
{
  // SPUD: x64 mov r11 + indirect jmp + address = 24 bytes. ARM64 ldr,
  // up to four mov instructions, br, address = at most 32 bytes.
  // Reject early returns/tail jumps, including alignment after tiny thunks.
#if defined(__aarch64__)
  constexpr auto   architecture = CS_ARCH_AARCH64;
  constexpr auto   mode         = CS_MODE_LITTLE_ENDIAN;
  constexpr size_t overwrite    = 32;
#elif defined(__x86_64__)
  constexpr auto   architecture = CS_ARCH_X86;
  constexpr auto   mode         = CS_MODE_64;
  constexpr size_t overwrite    = 24;
#else
  return false;
#endif
#if defined(__aarch64__) || defined(__x86_64__)
  csh handle{};
  if (cs_open(architecture, mode, &handle) != CS_ERR_OK)
    return false;
  cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
  cs_insn*   instructions = nullptr;
  const auto count        = cs_disasm(handle, static_cast<const uint8_t*>(method), std::min(extent, size_t{64}),
                                      reinterpret_cast<uintptr_t>(method), 0, &instructions);
  size_t     bytes        = 0;
  for (size_t i = 0; i < count && bytes < overwrite; ++i) {
    const auto& instruction = instructions[i];
    if (cs_insn_group(handle, &instruction, CS_GRP_RET) || cs_insn_group(handle, &instruction, CS_GRP_INT)
        || std::strcmp(instruction.mnemonic, "b") == 0 || std::strcmp(instruction.mnemonic, "br") == 0
        || std::strcmp(instruction.mnemonic, "jmp") == 0)
      break;
    bytes += instruction.size;
  }
  cs_free(instructions, count);
  cs_close(&handle);
  return bytes >= overwrite;
#endif
}
} // namespace
#endif

bool native_hooks::MacHookFits(const void* method)
{
#if __APPLE__
  const auto extent = FunctionExtent(method);
  const bool fits   = extent >= 64 && HasPrologue(method, extent);
  spdlog::info("[MacHookExtent] entry={} bytes={} accepted={}", method, extent, fits);
  return fits;
#else
  (void)method;
  return false;
#endif
}
