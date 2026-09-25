#if _WIN32
#include "patches/notification_audio_platform.h"

#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace
{
struct MediaSession {
  HRESULT               com       = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  HMODULE               platform  = LoadLibraryExW(L"mfplat.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  HMODULE               readwrite = LoadLibraryExW(L"mfreadwrite.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  decltype(&MFStartup)  startup   = nullptr;
  decltype(&MFShutdown) shutdown  = nullptr;
  decltype(&MFCreateMFByteStreamOnStream)       create_stream = nullptr;
  decltype(&MFCreateMediaType)                  create_type   = nullptr;
  decltype(&MFCreateSourceReaderFromByteStream) create_reader = nullptr;
  HRESULT                                       media         = E_FAIL;
  MediaSession()
  {
    // Media Foundation is optional on Windows N. Missing codecs must disable
    // custom clips, not prevent the mod DLL (and built-in cues) from loading.
    if (!platform || !readwrite || (FAILED(com) && com != RPC_E_CHANGED_MODE))
      return;
    startup       = reinterpret_cast<decltype(startup)>(GetProcAddress(platform, "MFStartup"));
    shutdown      = reinterpret_cast<decltype(shutdown)>(GetProcAddress(platform, "MFShutdown"));
    create_stream = reinterpret_cast<decltype(create_stream)>(GetProcAddress(platform, "MFCreateMFByteStreamOnStream"));
    create_type   = reinterpret_cast<decltype(create_type)>(GetProcAddress(platform, "MFCreateMediaType"));
    create_reader =
        reinterpret_cast<decltype(create_reader)>(GetProcAddress(readwrite, "MFCreateSourceReaderFromByteStream"));
    if (startup && shutdown && create_stream && create_type && create_reader)
      media = startup(MF_VERSION, MFSTARTUP_NOSOCKET);
  }
  ~MediaSession()
  {
    if (SUCCEEDED(media))
      shutdown();
    if (readwrite)
      FreeLibrary(readwrite);
    if (platform)
      FreeLibrary(platform);
    if (SUCCEEDED(com))
      CoUninitialize();
  }
};

void Append16(std::vector<uint8_t>& out, uint16_t value)
{
  out.push_back(static_cast<uint8_t>(value));
  out.push_back(static_cast<uint8_t>(value >> 8));
}
void Append32(std::vector<uint8_t>& out, uint32_t value)
{
  Append16(out, static_cast<uint16_t>(value));
  Append16(out, static_cast<uint16_t>(value >> 16));
}
} // namespace

std::vector<uint8_t> notification_audio_platform_prepare(std::span<const uint8_t> bytes, double& duration_seconds)
{
  duration_seconds = 0;
  if (bytes.empty() || bytes.size() > kNotificationAudioMaxBytes)
    return {};
  MediaSession session;
  if (FAILED(session.media))
    return {};
  // An in-memory byte stream cannot resolve web URLs or read external paths.
  ComPtr<IStream> stream;
  stream.Attach(SHCreateMemStream(bytes.data(), static_cast<UINT>(bytes.size())));
  if (!stream)
    return {};
  ComPtr<IMFByteStream> byte_stream;
  if (FAILED(session.create_stream(stream.Get(), &byte_stream)))
    return {};
  ComPtr<IMFSourceReader> reader;
  if (FAILED(session.create_reader(byte_stream.Get(), nullptr, &reader)))
    return {};
  if (FAILED(reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE))
      || FAILED(reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE)))
    return {};

  ComPtr<IMFMediaType> type;
  if (FAILED(session.create_type(&type)) || FAILED(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio))
      || FAILED(type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM))
      || FAILED(type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16))
      || FAILED(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, type.Get())))
    return {};
  type.Reset();
  if (FAILED(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &type)))
    return {};
  UINT32 channels = 0, rate = 0, bits = 0, align = 0;
  if (FAILED(type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels))
      || FAILED(type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate))
      || FAILED(type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits))
      || FAILED(type->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &align)) || channels < 1 || channels > 2 || rate == 0
      || rate > 192000 || bits != 16 || align != channels * 2)
    return {};
  const auto max_pcm =
      std::min<size_t>(kNotificationAudioMaxBytes - 44,
                       static_cast<size_t>(rate) * align * static_cast<size_t>(kNotificationAudioMaxSeconds));
  std::vector<uint8_t> pcm;
  bool                 ended = false;
  // Bound empty samples as well as decoded bytes for malformed streams.
  for (size_t iteration = 0; iteration < 100000; ++iteration) {
    ComPtr<IMFSample> sample;
    DWORD             flags     = 0;
    LONGLONG          timestamp = 0;
    if (FAILED(reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, &timestamp, &sample))
        || (flags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)))
      return {};
    if (sample) {
      ComPtr<IMFMediaBuffer> buffer;
      if (FAILED(sample->ConvertToContiguousBuffer(&buffer)))
        return {};
      BYTE* data   = nullptr;
      DWORD length = 0;
      if (FAILED(buffer->Lock(&data, nullptr, &length)))
        return {};
      const bool fits = length <= max_pcm - pcm.size();
      if (fits)
        pcm.insert(pcm.end(), data, data + length);
      buffer->Unlock();
      if (!fits)
        return {};
    }
    if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
      ended = true;
      break;
    }
  }
  if (!ended || pcm.empty() || pcm.size() % align)
    return {};
  std::vector<uint8_t> wav;
  wav.reserve(44 + pcm.size());
  auto tag = [&](const char* text) { wav.insert(wav.end(), text, text + 4); };
  tag("RIFF");
  Append32(wav, static_cast<uint32_t>(36 + pcm.size()));
  tag("WAVE");
  tag("fmt ");
  Append32(wav, 16);
  Append16(wav, 1);
  Append16(wav, static_cast<uint16_t>(channels));
  Append32(wav, rate);
  Append32(wav, rate * align);
  Append16(wav, static_cast<uint16_t>(align));
  Append16(wav, 16);
  tag("data");
  Append32(wav, static_cast<uint32_t>(pcm.size()));
  wav.insert(wav.end(), pcm.begin(), pcm.end());
  duration_seconds = static_cast<double>(pcm.size()) / (rate * align);
  return wav;
}
#endif
