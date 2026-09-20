#if _WIN32
#include "settings/audio_file_picker.h"
#include <Windows.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include <filesystem>
#include <stdexcept>

std::future<std::string> OpenAudioFilePicker()
{
  return std::async(std::launch::async, []() -> std::string {
    struct Apartment {
      HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
      ~Apartment() { if (SUCCEEDED(result)) CoUninitialize(); }
    } apartment;
    auto check = [](HRESULT result) {
      if (FAILED(result)) throw std::runtime_error("Cannot open the sound file picker");
    };
    check(apartment.result);
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    check(CoCreateInstance(__uuidof(FileOpenDialog), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)));
    DWORD options = 0;
    check(dialog->GetOptions(&options));
    check(dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR));
    const COMDLG_FILTERSPEC filters[] = {{L"Audio files (*.wav; *.mp3)", L"*.wav;*.mp3"}};
    check(dialog->SetFileTypes(1, filters));
    check(dialog->SetTitle(L"Choose an alert sound"));
    // Unowned: a modal dialog owned by the Unity window would disable game input.
    const auto result = dialog->Show(nullptr);
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return {};
    check(result);
    Microsoft::WRL::ComPtr<IShellItem> item;
    check(dialog->GetResult(&item));
    PWSTR raw = nullptr;
    check(item->GetDisplayName(SIGDN_FILESYSPATH, &raw));
    struct PathMemory { PWSTR value; ~PathMemory() { CoTaskMemFree(value); } } memory{raw};
    const auto utf8 = std::filesystem::path(raw).u8string();
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
  });
}
#endif
