#include "storage.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#if _WIN32
#include <Windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace diagnostics
{
namespace
{
  namespace fs                      = std::filesystem;
  constexpr std::string_view kOwner = "stfc-mod-capture-v1\n";

  bool Component(std::string_view value)
  {
    return !value.empty() && value.size() <= 80 && std::all_of(value.begin(), value.end(), [](unsigned char c) {
      return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    });
  }

  std::string RecorderId(std::string_view value)
  {
    if (!Component(value))
      throw std::invalid_argument("Invalid capture recorder ID");
    std::string result(value);
    for (auto& c : result)
      if (c >= 'A' && c <= 'Z')
        c = static_cast<char>(c - 'A' + 'a');
    return result;
  }

  fs::file_status Status(const fs::path& path)
  {
    std::error_code error;
    const auto      status = fs::symlink_status(path, error);
    if (error && error != std::errc::no_such_file_or_directory)
      throw fs::filesystem_error("Inspect capture path", path, error);
#if _WIN32
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
      throw std::runtime_error("Capture storage cannot use reparse points");
#endif
    if (fs::is_symlink(status))
      throw std::runtime_error("Capture storage cannot use symbolic links");
    return status;
  }

  void Directory(const fs::path& path)
  {
    auto status = Status(path);
    if (!fs::exists(status)) {
      fs::create_directory(path);
      status = Status(path);
    }
    if (!fs::is_directory(status))
      throw std::runtime_error("Capture storage path is not a directory");
  }

  bool EventFile(std::string_view name)
  {
    return name.size() == 21 && name.starts_with("events-") && name.ends_with(".jsonl")
           && std::all_of(name.begin() + 7, name.begin() + 15, [](char c) { return c >= '0' && c <= '9'; });
  }

  std::string Owner(std::string_view recorder, std::string_view session)
  { return std::string(kOwner) + std::string(recorder) + '\n' + std::string(session) + '\n'; }

  uint64_t Add(uint64_t a, uint64_t b)
  {
    if (b > (std::numeric_limits<uint64_t>::max)() - a)
      throw std::runtime_error("Capture storage size overflow");
    return a + b;
  }

  bool OwnerMatches(const fs::path& path, std::string_view expected)
  {
    std::array<char, 257> bytes{};
    size_t                count{};
#if _WIN32
    const auto handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                    FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
      throw std::runtime_error("Cannot read capture ownership marker");
    BY_HANDLE_FILE_INFORMATION info{};
    bool valid = GetFileInformationByHandle(handle, &info)
                 && !(info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY));
    while (valid && count < bytes.size()) {
      DWORD read{};
      valid = ReadFile(handle, bytes.data() + count, static_cast<DWORD>(bytes.size() - count), &read, nullptr);
      if (!read)
        break;
      count += read;
    }
    CloseHandle(handle);
    if (!valid)
      throw std::runtime_error("Invalid capture ownership marker");
#else
    const int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW);
    if (fd < 0)
      throw std::runtime_error("Cannot read capture ownership marker");
    struct stat info{};
    bool        valid = fstat(fd, &info) == 0 && S_ISREG(info.st_mode);
    while (valid && count < bytes.size()) {
      ssize_t read;
      do {
        read = ::read(fd, bytes.data() + count, bytes.size() - count);
      } while (read < 0 && errno == EINTR);
      if (read < 0)
        valid = false;
      if (read <= 0)
        break;
      count += static_cast<size_t>(read);
    }
    close(fd);
    if (!valid)
      throw std::runtime_error("Invalid capture ownership marker");
#endif
    return count == expected.size() && std::string_view(bytes.data(), count) == expected;
  }

  class File
  {
  public:
    File() = default;
    explicit File(const fs::path& path)
    {
      int fd = -1;
#if _WIN32
      const auto handle = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
      if (handle == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot create capture file exclusively");
      fd = _open_osfhandle(reinterpret_cast<intptr_t>(handle), _O_BINARY | _O_WRONLY);
      if (fd == -1)
        CloseHandle(handle);
#else
      fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
#endif
      if (fd == -1)
        throw std::runtime_error("Cannot create capture file");
#if _WIN32
      file_ = _fdopen(fd, "wb");
      if (!file_)
        _close(fd);
#else
      file_ = fdopen(fd, "wb");
      if (!file_)
        close(fd);
#endif
      if (!file_)
        throw std::runtime_error("Cannot buffer capture file");
    }
    ~File()
    {
      if (file_)
        std::fclose(file_);
    }
    File(File&& other) noexcept
        : file_(std::exchange(other.file_, nullptr))
    {
    }
    File& operator=(File&& other) noexcept
    {
      if (file_)
        std::fclose(file_);
      file_ = std::exchange(other.file_, nullptr);
      return *this;
    }
    File(const File&)               = delete;
    File&    operator=(const File&) = delete;
    explicit operator bool() const
    { return file_ != nullptr; }
    void Write(std::string_view bytes)
    {
      if (!file_ || std::fwrite(bytes.data(), 1, bytes.size(), file_) != bytes.size())
        throw std::runtime_error("Capture write failed");
    }
    void Flush()
    {
      if (file_ && std::fflush(file_) != 0)
        throw std::runtime_error("Capture flush failed");
    }
    void Close()
    {
      auto* file = std::exchange(file_, nullptr);
      if (!file)
        return;
      const auto flushed = std::fflush(file);
      const auto closed  = std::fclose(file);
      if (flushed != 0 || closed != 0)
        throw std::runtime_error("Capture close failed");
    }

  private:
    std::FILE* file_{};
  };

  class Lease
  {
  public:
    explicit Lease(const fs::path& path)
    {
      const auto status = Status(path);
      if (fs::exists(status) && !fs::is_regular_file(status))
        throw std::runtime_error("Invalid capture lease file");
#if _WIN32
      handle_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
      if (handle_ == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Capture destination is already in use or unavailable");
#else
      fd_ = open(path.c_str(), O_RDWR | O_CREAT | O_NOFOLLOW, 0600);
      if (fd_ < 0 || flock(fd_, LOCK_EX | LOCK_NB) != 0) {
        if (fd_ >= 0)
          close(fd_);
        throw std::runtime_error("Capture destination is already in use or unavailable");
      }
#endif
    }
    ~Lease()
    {
#if _WIN32
      if (handle_ != INVALID_HANDLE_VALUE)
        CloseHandle(handle_);
#else
      if (fd_ >= 0)
        close(fd_);
#endif
    }
    Lease(const Lease&)            = delete;
    Lease& operator=(const Lease&) = delete;

  private:
#if _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    int fd_ = -1;
#endif
  };
} // namespace

struct Storage::Impl {
  struct Session {
    fs::path path;
    File     file;
    uint64_t bytes{}, pending{};
    uint32_t part{};
    fs::path current;
  };
  struct Candidate {
    std::string           recorder;
    fs::path              directory;
    std::vector<fs::path> files;
    uint64_t              bytes{};
    fs::file_time_type    time;
  };
  struct Inventory {
    uint64_t                                  total{};
    std::unordered_map<std::string, uint64_t> recorders;
    std::vector<Candidate>                    candidates;
  };
  fs::path                                  root;
  StorageLimits                             limits;
  std::unique_ptr<Lease>                    lease;
  std::unordered_map<std::string, Session>  sessions;
  std::unordered_map<std::string, uint64_t> recorderBytes;
  uint64_t                                  totalBytes{}, availableBytes{}, spaceCharged{};
  bool                                      accounted{}, spaceKnown{};
  std::chrono::steady_clock::time_point     spaceChecked{};

  Impl(fs::path destination, StorageLimits value)
      : limits(value)
  {
    if (destination.empty() || !destination.is_absolute())
      throw std::invalid_argument("Capture destination must be an absolute directory");
    if (!limits.file_bytes || limits.recorder_bytes < limits.file_bytes || limits.total_bytes < limits.recorder_bytes)
      throw std::invalid_argument("Invalid capture storage limits");
    fs::create_directories(destination);
    root = fs::canonical(destination) / "captures-v1";
    Directory(root);
    lease = std::make_unique<Lease>(root / "capture.lock");
  }

  ~Impl()
  {
    for (auto& [name, session] : sessions) {
      try {
        session.file.Close();
      } catch (...) {
      }
    }
  }

  Inventory Scan()
  {
    Directory(root);
    Inventory result;
    for (const auto& recorder : fs::directory_iterator(root)) {
      const auto physicalName = recorder.path().filename().string();
      if (!Component(physicalName) || !fs::is_directory(Status(recorder.path())))
        continue;
      const auto name = RecorderId(physicalName);
      for (const auto& capture : fs::directory_iterator(recorder.path())) {
        const auto id = capture.path().filename().string();
        if (!Component(id) || !fs::is_directory(Status(capture.path())))
          continue;
        const auto owner = capture.path() / "capture.owner";
        if (!fs::is_regular_file(Status(owner)) || fs::file_size(owner) > 256)
          continue;
        const auto active = sessions.find(name);
        bool       open   = active != sessions.end() && active->second.path == capture.path();
        if (!open && active != sessions.end()) {
          if (!fs::is_directory(Status(active->second.path)))
            throw std::runtime_error("Active capture directory changed during retention");
          open = fs::equivalent(active->second.path, capture.path());
        }
        const auto markerSession = open ? active->second.path.filename().string() : id;
        if (!OwnerMatches(owner, Owner(name, markerSession))
            && (physicalName == name || !OwnerMatches(owner, Owner(physicalName, id))))
          continue;
        Candidate closed{name, capture.path(), {}, 0, fs::last_write_time(owner)};
        for (const auto& entry : fs::directory_iterator(capture.path())) {
          const auto filename = entry.path().filename().string();
          if (filename != "capture.owner" && filename != "manifest.json" && !EventFile(filename))
            continue;
          if (!fs::is_regular_file(Status(entry.path())))
            continue;
          const auto bytes       = fs::file_size(entry.path());
          result.total           = Add(result.total, bytes);
          result.recorders[name] = Add(result.recorders[name], bytes);
          if (!open) {
            if (filename == "capture.owner")
              continue; // Delete the ownership marker last, after other owned files.
            closed.files.push_back(entry.path());
            closed.bytes = Add(closed.bytes, bytes);
          } else if (EventFile(filename)) {
            bool current = active->second.file && entry.path() == active->second.current;
            if (!current && active->second.file && !active->second.current.empty()) {
              if (!fs::is_regular_file(Status(active->second.current)))
                throw std::runtime_error("Active capture file changed during retention");
              current = fs::equivalent(entry.path(), active->second.current);
            }
            if (!current)
              result.candidates.push_back(
                  {name, capture.path(), {entry.path()}, bytes, fs::last_write_time(entry.path())});
          }
        }
        if (!open) {
          closed.files.push_back(owner);
          closed.bytes = Add(closed.bytes, fs::file_size(owner));
          result.candidates.push_back(std::move(closed));
        }
      }
    }
    std::sort(result.candidates.begin(), result.candidates.end(),
              [](const auto& a, const auto& b) { return a.time < b.time; });
    return result;
  }

  void Invalidate() noexcept
  { accounted = spaceKnown = false; }

  Inventory Reconcile()
  {
    try {
      for (auto& [name, session] : sessions) {
        session.file.Flush();
        session.pending = 0;
      }
      auto inventory = Scan();
      totalBytes     = inventory.total;
      recorderBytes  = inventory.recorders;
      accounted      = true;
      spaceKnown     = false;
      return inventory;
    } catch (...) {
      Invalidate();
      throw;
    }
  }

  uint64_t FreeSpace()
  {
    const auto now = std::chrono::steady_clock::now();
    if (!spaceKnown || now - spaceChecked >= std::chrono::seconds(1) || spaceCharged >= 256 * 1024) {
      uint64_t pending{};
      for (const auto& [name, session] : sessions)
        pending = Add(pending, session.pending);
      const auto available = fs::space(root).available;
      availableBytes       = available > pending ? available - pending : 0;
      spaceCharged         = 0;
      spaceChecked         = now;
      spaceKnown           = true;
    }
    return availableBytes > spaceCharged ? availableBytes - spaceCharged : 0;
  }

  void Account(std::string_view recorder, uint64_t bytes)
  {
    totalBytes   = Add(totalBytes, bytes);
    auto& used   = recorderBytes[std::string(recorder)];
    used         = Add(used, bytes);
    spaceCharged = Add(spaceCharged, bytes);
  }

  void Capacity(std::string_view recorder, uint64_t additional)
  {
    if (additional > limits.recorder_bytes || additional > limits.total_bytes)
      throw std::runtime_error("Capture record exceeds storage quota");
    if (!accounted)
      Reconcile();
    const auto full = [&] {
      return Add(recorderBytes[std::string(recorder)], additional) > limits.recorder_bytes
             || Add(totalBytes, additional) > limits.total_bytes
             || FreeSpace() < Add(limits.min_free_bytes, additional);
    };
    while (full()) {
      // The common append path uses logical accounting, including buffered
      // bytes. Only retention reconciles the bounded journals with the disk.
      const auto inventory = Reconcile();
      if (!full())
        return;
      const bool recorderFull = Add(recorderBytes[std::string(recorder)], additional) > limits.recorder_bytes;
      const auto candidate = std::find_if(inventory.candidates.begin(), inventory.candidates.end(), [&](const auto& c) {
        return (!recorderFull || c.recorder == recorder) && c.bytes;
      });
      if (candidate == inventory.candidates.end())
        throw std::runtime_error("Capture quota or free-space reserve exhausted; no closed capture can be retired");
      Directory(root);
      Directory(candidate->directory.parent_path());
      Directory(candidate->directory);
      for (const auto& path : candidate->files) {
        if (!fs::is_regular_file(Status(path)) || !fs::remove(path))
          throw std::runtime_error("Closed capture changed during retention");
      }
      // Unknown files are preserved; remove only directories that became empty.
      if (fs::is_empty(candidate->directory))
        fs::remove(candidate->directory);
      Invalidate();
      Reconcile();
    }
  }

  Session& Get(std::string_view recorder)
  {
    const auto it = sessions.find(std::string(recorder));
    if (it == sessions.end())
      throw std::runtime_error("Recorder capture is not open");
    return it->second;
  }
};

Storage::Storage(fs::path destination, StorageLimits limits)
    : impl_(std::make_unique<Impl>(std::move(destination), limits))
{
}
Storage::~Storage()                             = default;
Storage::Storage(Storage&&) noexcept            = default;
Storage& Storage::operator=(Storage&&) noexcept = default;

fs::path Storage::Open(std::string_view recorder, std::string_view session, std::string_view manifest_json)
{
  const auto key = RecorderId(recorder);
  if (!Component(session) || impl_->sessions.contains(key))
    throw std::invalid_argument("Invalid or already active recorder/session");
  std::string manifest(manifest_json);
  if (manifest.empty() || manifest.back() != '\n')
    manifest += '\n';
  const auto marker = Owner(key, session);
  if (manifest.size() > impl_->limits.file_bytes || marker.size() > impl_->limits.file_bytes)
    throw std::runtime_error("Capture manifest exceeds file limit");
  impl_->Capacity(key, Add(manifest.size(), marker.size()));
  const auto parent = impl_->root / key;
  Directory(parent);
  const auto directory = parent / std::string(session);
  if (!fs::create_directory(directory))
    throw std::runtime_error("Capture session already exists");
  Directory(directory);
  try {
    for (const auto& [name, content] : std::array<std::pair<std::string_view, std::string_view>, 2>{
             {{"capture.owner", marker}, {"manifest.json", manifest}}}) {
      File file(directory / name);
      file.Write(content);
      file.Close();
    }
    impl_->sessions.emplace(key, Impl::Session{directory});
    impl_->Account(key, Add(marker.size(), manifest.size()));
  } catch (...) {
    impl_->Invalidate();
    throw;
  }
  return directory;
}

void Storage::Append(std::string_view recorder, std::string_view json_line)
{
  const auto key = RecorderId(recorder);
  if (json_line.empty() || json_line.back() != '\n'
      || json_line.substr(0, json_line.size() - 1).find('\n') != std::string_view::npos)
    throw std::invalid_argument("Capture record must be one newline-terminated line");
  if (json_line.size() > impl_->limits.file_bytes)
    throw std::runtime_error("Capture record exceeds file limit");
  auto& session = impl_->Get(key);
  try {
    if (session.file && Add(session.bytes, json_line.size()) > impl_->limits.file_bytes) {
      session.file.Close();
      session.current.clear();
      session.bytes = session.pending = 0;
    }
    impl_->Capacity(key, json_line.size());
    if (!session.file) {
      Directory(session.path.parent_path());
      Directory(session.path);
      if (session.part >= 99999999)
        throw std::runtime_error("Capture part limit reached");
      char filename[32]{};
      std::snprintf(filename, sizeof(filename), "events-%08u.jsonl", session.part++);
      session.current = session.path / filename;
      session.file    = File(session.current);
    }
    session.file.Write(json_line);
    session.bytes   = Add(session.bytes, json_line.size());
    session.pending = Add(session.pending, json_line.size());
    impl_->Account(key, json_line.size());
  } catch (...) {
    impl_->Invalidate();
    throw;
  }
}

void Storage::Flush(std::string_view recorder)
{
  auto& session = impl_->Get(RecorderId(recorder));
  try {
    session.file.Flush(); // Flush buffers to the OS, not fsync/durable persistence.
    session.pending = 0;
  } catch (...) {
    impl_->Invalidate();
    throw;
  }
}

void Storage::Close(std::string_view recorder)
{
  const auto it = impl_->sessions.find(RecorderId(recorder));
  if (it == impl_->sessions.end())
    return;
  try {
    it->second.file.Close();
  } catch (...) {
    impl_->Invalidate();
    impl_->sessions.erase(it);
    throw;
  }
  impl_->sessions.erase(it);
}
} // namespace diagnostics
