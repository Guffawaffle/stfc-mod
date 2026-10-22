#ifdef NDEBUG
#undef NDEBUG
#endif
#include "dev/diagnostics.h"
#include <cassert>

int main()
{
  using namespace dev::diagnostics;
  auto source = RegisterSource("snapshot-fixture");
  SetAwake(true);
  Clear();
  const auto initial = ReadSnapshotIfChanged(0);
  assert(initial && initial->entries.empty());
  for (int i = 0; i < 131; ++i)
    Publish(source, Severity::Info, "entry");
  const auto full = ReadSnapshotIfChanged(initial->revision);
  assert(full && full->entries.size() == 128 && full->dropped == 3);
  for (int i = 0; i < 10000; ++i)
    assert(!ReadSnapshotIfChanged(full->revision));
  const auto forced = ReadSnapshotIfChanged(full->revision, true);
  assert(forced && forced->entries.size() == 128 && forced->revision == full->revision);
  Clear();
  const auto cleared = ReadSnapshotIfChanged(full->revision);
  assert(cleared && cleared->entries.empty() && cleared->dropped == 0);
  Clear(); // Even clearing an empty ring invalidates a consumer's render revision.
  assert(ReadSnapshotIfChanged(cleared->revision));
  const auto revision = ReadSnapshot().revision;
  SetSourceEnabled(source, false);
  Publish(source, Severity::Info, "disabled");
  assert(!ReadSnapshotIfChanged(revision));
  SetAwake(false);
  assert(!ReadSnapshotIfChanged(0, true));
}
