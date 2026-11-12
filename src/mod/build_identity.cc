#include "build_identity.h"
#include "version.h"

#include <cstdlib>
#include <string>
#include <vector>

#if _WIN32
#include <Windows.h>
#include <cstring>
#include <wincrypt.h>
#include <wintrust.h>
#endif

namespace build_identity
{
namespace
{
#if _WIN32
  int build_identity_module_anchor;

  struct SignatureObjects {
    HCERTSTORE store   = nullptr;
    HCRYPTMSG  message = nullptr;
    ~SignatureObjects()
    {
      if (message)
        CryptMsgClose(message);
      if (store)
        CertCloseStore(store, 0);
    }
  };

  std::string ProgramDescription(const CRYPT_ATTR_BLOB& value, DWORD encoding)
  {
    DWORD size = 0;
    if (!value.pbData || !value.cbData
        || !CryptDecodeObjectEx(encoding, SPC_SP_OPUS_INFO_OBJID, value.pbData, value.cbData, 0, nullptr, nullptr,
                                &size)
        || size < sizeof(SPC_SP_OPUS_INFO) || size > 1024 * 1024)
      return {};
    std::vector<BYTE> decoded(size);
    if (!CryptDecodeObjectEx(encoding, SPC_SP_OPUS_INFO_OBJID, value.pbData, value.cbData, 0, nullptr, decoded.data(),
                             &size)
        || size < sizeof(SPC_SP_OPUS_INFO) || size > decoded.size())
      return {};
    const auto* info = reinterpret_cast<const SPC_SP_OPUS_INFO*>(decoded.data());
    if (!info->pwszProgramName)
      return {};
    // With copying decode flags, pointed-to strings occupy the returned buffer.
    // Keep a malformed/nonterminated description from becoming an unbounded read.
    const auto begin = reinterpret_cast<uintptr_t>(decoded.data());
    const auto name  = reinterpret_cast<uintptr_t>(info->pwszProgramName);
    if (name < begin || name >= begin + size || name % alignof(wchar_t))
      return {};
    const auto available = (begin + size - name) / sizeof(wchar_t);
    int        length    = 0;
    while (length <= 180 && static_cast<std::size_t>(length) < available && info->pwszProgramName[length])
      ++length;
    if (!length || length > 180 || static_cast<std::size_t>(length) == available)
      return {};
    const auto bytes =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, info->pwszProgramName, length, nullptr, 0, nullptr, nullptr);
    if (!bytes || bytes > 200)
      return {};
    std::string result(static_cast<std::size_t>(bytes), '\0');
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, info->pwszProgramName, length, result.data(), bytes,
                               nullptr, nullptr)
                   == bytes
               ? result
               : std::string{};
  }

  std::wstring ModulePath()
  {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&build_identity_module_anchor), &module))
      return {};
    std::wstring path(32768, L'\0');
    const auto   size = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (!size || size >= path.size())
      return {};
    path.resize(size);
    return path;
  }

  std::string SignatureDescription(const std::wstring& path, SignatureState& state)
  {
    state = SignatureState::Unknown;
    if (path.empty())
      return {};
    // CryptQueryObject can also report NO_MATCH for a missing input file.
    // Establish an observable file before treating that result as absence.
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
      return {};
    SignatureObjects objects;
    DWORD            encoding = 0;
    // Read only the embedded signature metadata. This does not perform trust,
    // revocation, publisher validation, or any network operation.
    if (!CryptQueryObject(CERT_QUERY_OBJECT_FILE, path.c_str(), CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                          CERT_QUERY_FORMAT_FLAG_BINARY, 0, &encoding, nullptr, nullptr, &objects.store,
                          &objects.message, nullptr)) {
      if (GetLastError() == static_cast<DWORD>(CRYPT_E_NO_MATCH))
        state = SignatureState::Absent;
      return {};
    }
    state         = SignatureState::Present;
    DWORD signers = 0, count_size = sizeof(signers);
    if (!CryptMsgGetParam(objects.message, CMSG_SIGNER_COUNT_PARAM, 0, &signers, &count_size) || signers != 1)
      return {};
    DWORD signer_size = 0;
    if (!CryptMsgGetParam(objects.message, CMSG_SIGNER_INFO_PARAM, 0, nullptr, &signer_size)
        || signer_size < sizeof(CMSG_SIGNER_INFO) || signer_size > 1024 * 1024)
      return {};
    std::vector<BYTE> signer(signer_size);
    if (!CryptMsgGetParam(objects.message, CMSG_SIGNER_INFO_PARAM, 0, signer.data(), &signer_size))
      return {};
    const auto* info = reinterpret_cast<const CMSG_SIGNER_INFO*>(signer.data());
    if (info->AuthAttrs.cAttr && !info->AuthAttrs.rgAttr)
      return {};
    bool        found = false;
    std::string result;
    for (DWORD index = 0; index < info->AuthAttrs.cAttr; ++index) {
      const auto& attribute = info->AuthAttrs.rgAttr[index];
      if (!attribute.pszObjId || std::strcmp(attribute.pszObjId, SPC_SP_OPUS_INFO_OBJID) != 0)
        continue;
      if (found || attribute.cValue != 1 || !attribute.rgValue)
        return {};
      found  = true;
      result = ProgramDescription(attribute.rgValue[0], encoding);
    }
    return result;
  }
#elif __APPLE__
  std::string EnvironmentValue(const char* name, std::size_t limit)
  {
    const auto* value = std::getenv(name);
    if (!value)
      return {};
    std::size_t size = 0;
    while (size <= limit && value[size])
      ++size;
    return size > limit ? std::string{} : std::string(value, size);
  }
#endif
} // namespace

std::string_view DisplayVersion()
{
  static const std::string display = [] {
#if _WIN32
    SignatureState state;
    const auto     description = SignatureDescription(ModulePath(), state);
    return DescriptionVersion(VER_FILE_VERSION_STR, description, state, CompiledInvocation(), VER_FORK_VERSION_STR);
#elif __APPLE__
    return HandoffVersion(
        VER_FILE_VERSION_STR, EnvironmentValue("STFC_MOD_DISPLAY_VERSION", 160),
        EnvironmentValue("STFC_MOD_DISPLAY_SOURCE", 40), VERSION_COMMIT_HASH, STFC_BUILD_INVOCATION_ID,
        std::getenv("STFC_MOD_DISPLAY_VERSION") || std::getenv("STFC_MOD_DISPLAY_SOURCE"), VER_FORK_VERSION_STR);
#else
    return FallbackVersion(VER_FORK_VERSION_STR, STFC_BUILD_INVOCATION_ID);
#endif
  }();
  return display;
}
} // namespace build_identity
