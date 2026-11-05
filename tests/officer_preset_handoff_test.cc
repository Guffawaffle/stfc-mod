#include "officer_preset_handoff.h"
#include "officer_preset_order.h"

#include <array>
#include <cstdlib>
#include <iostream>

namespace handoff = officer_preset_handoff;
namespace order   = officer_preset_order;

namespace
{
int  checks = 0;
void Check(bool condition)
{
  ++checks;
  if (!condition) {
    std::cerr << "Officer preset handoff check failed: " << checks << '\n';
    std::abort();
  }
}

constexpr std::size_t  count = 6;
constexpr std::int64_t large = 9007199254740993LL;
struct Item {
  order::Row row;
  int        content = 0;
  int        owner   = 1;
};
struct Receiver {
  Item *item = nullptr;
};
struct Unsupported {
};
enum class Write { Normal, Refuse, AppliedButReportedFailure, ForeignSideEffect };

order::Row Filled(std::int64_t slot, std::int32_t canonical)
{
  return {.qualified        = true,
          .present          = true,
          .slot             = slot,
          .order            = canonical,
          .presentation     = 10 + canonical,
          .name_present     = true,
          .officers_present = true,
          .assigned         = true};
}
order::Row Empty(std::int32_t canonical)
{
  auto row       = Filled(77, canonical);
  row.name_empty = true;
  row.assigned   = false;
  return row;
}

// Independent native seam, with distinct canonical data, display arrays and
// renderer receivers. It reproduces the retained oracle's write-then-lookup
// order; lookup never derives its receiver mapping from the current data source.
struct Host {
  std::array<Item, count> items{
      {{Filled(0, 0), 100}, {Empty(1)}, {Filled(large, 2), 200}, {Empty(3)}, {}, {Filled(9, 5), 300}}};
  std::array<Item *, count>       canonical{{&items[0], &items[1], &items[2], &items[3], nullptr, &items[5]}};
  std::array<Item *, count>       display{}, next_display{};
  std::array<Receiver, count>     receivers{};
  std::array<Item *, count>       tokens = canonical;
  std::array<std::int32_t, count> native_presentations{{10, 11, 12, 13, 0, 15}};
  handoff::Boundary               boundary;
  handoff::Identity               identity{1, &items, &tokens, &receivers, &canonical};
  const void                     *source          = &canonical;
  Write                           canonical_write = Write::Normal, restore_write = Write::Normal;
  bool                            fail_sync = false, sync_side_effect = false, contradiction = false;
  bool                            replace_owner_during_sync = false;
  int                             saves = 0, lookups = 0, binds = 0, source_writes = 0, syncs = 0;
  std::int32_t                    last_lookup = -100;

  Host()
  { boundary.Bind(identity); }

  handoff::Identity Observe() const noexcept
  { return identity; }
  const void *Source() const noexcept
  { return source; }
  std::shared_ptr<const handoff::Projection> MakeProjection(bool next = false)
  {
    std::array<order::Row, count> rows{};
    for (std::size_t i = 0; i < count; ++i)
      rows[i] = canonical[i] ? canonical[i]->row : order::Row{.qualified = true};
    const std::array saved{large, next ? std::int64_t{9} : std::int64_t{0}, next ? std::int64_t{0} : std::int64_t{9}};
    const auto       plan = order::Prepare(rows, saved);
    Check(plan.status == order::Status::Ready);
    auto &target = next ? next_display : display;
    for (std::size_t view = 0; view < count; ++view)
      target[view] = canonical[plan.view_to_canonical[view]];
    return std::make_shared<handoff::Projection>(
        handoff::Projection{next ? static_cast<const void *>(&next_display) : &display, plan.view_to_canonical});
  }
  void Adopt(const handoff::Projection &projection)
  {
    auto &array = projection.source == &display ? display : next_display;
    source      = projection.source;
    for (std::size_t view = 0; view < count; ++view) {
      receivers[view].item = array[view];
      if (array[view])
        array[view]->row.presentation = native_presentations[view];
    }
  }
  void Activate()
  {
    auto       projection = MakeProjection();
    const auto ticket     = boundary.BeginTransition(projection);
    Check(ticket);
    Adopt(*projection);
    Check(boundary.FinishTransition(ticket, identity, source, true));
  }

  // Return value simulates invocation status. Callers separately inspect the
  // physical field: failure can mean no write, successful write, or other data.
  bool WriteSource(const void *target, Write action) noexcept
  {
    ++source_writes;
    if (action == Write::Refuse)
      return false;
    source = action == Write::ForeignSideEffect ? &source_writes : target;
    return action == Write::Normal;
  }
  bool NativePresentations(const handoff::SaveScope &scope) noexcept
  {
    if (!scope.Current(boundary, identity))
      return false;
    for (std::size_t canonical_index = 0; canonical_index < count; ++canonical_index) {
      auto *item = canonical[canonical_index];
      if (!item)
        continue;
      if (item->owner != 1 || item->row.order != static_cast<std::int32_t>(canonical_index))
        return false;
      if (tokens[canonical_index] != item) {
        native_presentations[canonical_index] = item->row.presentation;
        tokens[canonical_index]               = item;
      }
      item->row.presentation = native_presentations[canonical_index];
    }
    return true;
  }
  bool Canonical(const handoff::SaveScope &scope) noexcept
  {
    // Source safety cannot depend on the optional presentation annotation.
    (void)NativePresentations(scope);
    (void)WriteSource(scope.identity.canonical, canonical_write);
    return source == scope.identity.canonical;
  }
  bool RestoreSource(const void *target) noexcept
  {
    (void)WriteSource(target, restore_write);
    return source == target;
  }
  [[noreturn]] void Unsupported()
  {
    contradiction = true;
    throw ::Unsupported{}; // Explicit rejection substitute, not a native success/fallback.
  }

  bool SyncArray(const handoff::SaveScope &scope, const handoff::Projection &projection, std::int32_t canonical_index,
                 std::int32_t view) noexcept
  {
    if (!scope.Current(boundary, identity) || canonical_index < 0 || view < 0
        || static_cast<std::size_t>(canonical_index) >= count || static_cast<std::size_t>(view) >= count)
      return false;
    auto *item = canonical[static_cast<std::size_t>(canonical_index)];
    if (item
        && (item->owner != 1 || item->row.order != canonical_index
            || item->row.slot != items[static_cast<std::size_t>(canonical_index)].row.slot))
      return false; // Unrelated same-size row replacement is not an admitted save replacement.
    auto &array = projection.source == &display ? display : next_display;
    ++syncs;
    if (fail_sync && !sync_side_effect)
      return false;
    if (item && tokens[static_cast<std::size_t>(canonical_index)] != item) {
      native_presentations[static_cast<std::size_t>(canonical_index)] = item->row.presentation;
      tokens[static_cast<std::size_t>(canonical_index)]               = item;
    }
    array[static_cast<std::size_t>(view)] = item; // Real adapter uses a checked GC-barrier write.
    if (item)
      item->row.presentation = native_presentations[static_cast<std::size_t>(view)];
    if (replace_owner_during_sync)
      ++identity.generation;
    return !fail_sync;
  }
  bool Sync(const handoff::SaveScope &scope, std::int32_t canonical_index, std::int32_t view) noexcept
  { return scope.display && SyncArray(scope, *scope.display, canonical_index, view); }
  bool SyncAll(const handoff::SaveScope &scope) noexcept
  {
    bool result = true;
    for (const auto &projection : {scope.display, scope.candidate})
      if (projection)
        for (std::size_t view = 0; view < projection->view_to_canonical.size(); ++view)
          result = SyncArray(scope, *projection, static_cast<std::int32_t>(projection->view_to_canonical[view]),
                             static_cast<std::int32_t>(view))
                   && result;
    return result;
  }
  bool Matches(const handoff::SaveScope &scope, Receiver *receiver, std::int32_t canonical_index) const noexcept
  {
    auto *item = receiver ? receiver->item : nullptr;
    return scope.Current(boundary, identity) && item && item->owner == 1 && item->row.order == canonical_index
           && canonical_index >= 0 && static_cast<std::size_t>(canonical_index) < count
           && item->row.slot == canonical[static_cast<std::size_t>(canonical_index)]->row.slot;
  }
  bool NativeLookup(std::int32_t view, Receiver *&receiver) noexcept
  {
    ++lookups;
    last_lookup = view;
    receiver =
        view >= 0 && static_cast<std::size_t>(view) < count ? &receivers[static_cast<std::size_t>(view)] : nullptr;
    return receiver;
  }
  bool Lookup(std::int32_t canonical_index, Receiver *&receiver)
  {
    return handoff::LookupOnce(boundary, *this, identity.scroller, canonical_index, receiver,
                               [&](std::int32_t view, Receiver *&result) { return NativeLookup(view, result); });
  }
  void NativeSave(std::int32_t index, Item *replacement)
  {
    ++saves;
    Check(source == &canonical);
    canonical[static_cast<std::size_t>(index)] = replacement;
    Receiver *receiver                         = nullptr;
    if (Lookup(index, receiver)) {
      ++binds;
      receiver->item = replacement; // The retained callback binds the returned receiver once.
    }
  }
  void Save(std::int32_t index, Item *replacement)
  {
    handoff::SaveOnce(boundary, *this, [&] { NativeSave(index, replacement); });
  }
};

void ExactOnceAndReplacement()
{
  Host host;
  host.Activate();
  const auto identities = host.items;
  Item       replacement{Filled(0, 0), 999};
  replacement.row.presentation = 22;
  host.Save(0, &replacement);
  Check(host.saves == 1 && host.lookups == 1 && host.binds == 1 && host.last_lookup == 2);
  Check(host.canonical[0] == &replacement && host.display[2] == &replacement);
  Check(host.receivers[2].item == &replacement && host.receivers[0].item == &host.items[2]);
  Check(host.source == &host.display && replacement.row.presentation == 12 && host.native_presentations[0] == 22);
  Check(host.canonical[1] == &host.items[1] && host.canonical[3] == &host.items[3] && !host.canonical[4]);
  for (std::size_t i = 0; i < count; ++i) {
    Check(host.items[i].row.slot == identities[i].row.slot);
    Check(host.items[i].row.order == identities[i].row.order);
  }

  int exception_identity = 0;
  int calls              = 0;
  try {
    handoff::SaveOnce(host.boundary, host, [&]() -> void {
      ++calls;
      Check(host.source == &host.canonical);
      throw &exception_identity;
    });
    Check(false);
  } catch (int *observed) {
    Check(observed == &exception_identity);
  }
  Check(calls == 1 && host.source == &host.display);

  Item large_replacement{Filled(large, 2), 777};
  host.Save(2, &large_replacement);
  Check(host.last_lookup == 0 && host.canonical[2]->row.slot == large);
  Check(host.canonical[0]->row.slot == 0 && host.native_presentations[2] == 12);
  Check(host.display[0] == &large_replacement);
}

void TransitionReentrancy()
{
  Host host;
  host.Activate();
  auto       next   = host.MakeProjection(true);
  const auto ticket = host.boundary.BeginTransition(next);
  Check(ticket);
  Check(host.source == &host.display); // NEW episode, OLD installed display: the parked prototype hole.
  Check(host.boundary.GuardRequired(host.identity));
  Item replacement{Filled(0, 0), 456};
  host.Save(0, &replacement);
  Check(host.saves == 1 && host.last_lookup == -1 && host.binds == 0);
  Check(host.source == &host.canonical && host.canonical[0] == &replacement);
  Check(host.display[2] == &host.items[0] && host.next_display[5] == &host.items[0]);
  Check(host.boundary.State() == handoff::Phase::Restore && replacement.row.presentation == 10);
  host.Adopt(*next); // Native rebuild resumes after the nested save.
  Item second{Filled(9, 5), 654};
  host.Save(5, &second);
  Check(host.saves == 2 && host.last_lookup == -1 && host.binds == 0);
  Check(host.source == &host.canonical && host.canonical[5] == &second);
  Check(!host.boundary.FinishTransition(ticket - 1, host.identity, host.source, true));
  Check(!host.boundary.FinishTransition(ticket, host.identity, host.source, true));
  // Only a separately verified canonical renderer repair retires quarantine.
  Check(host.boundary.ConfirmCanonical(host.identity, host.source, true));
  next             = host.MakeProjection(true); // Fresh rows from the canonical native result.
  const auto fresh = host.boundary.BeginTransition(next);
  Check(fresh);
  host.Adopt(*next);
  Check(host.boundary.FinishTransition(fresh, host.identity, host.source, true));
  host.Save(5, &second);
  Check(host.last_lookup == 2 && host.binds == 1);

  Host       first;
  auto       first_projection = first.MakeProjection();
  const auto first_ticket     = first.boundary.BeginTransition(first_projection);
  Check(first_ticket);
  Check(first.source == &first.canonical);
  Item first_replacement{Filled(0, 0), 10};
  first.Save(0, &first_replacement); // Reentry before the first display is installed.
  Check(first.saves == 1 && first.last_lookup == -1 && first.source == &first.canonical);
  Check(first.canonical[0] == &first_replacement && first.display[2] == &first.items[0]);

  // A resumed rebuild cannot certify the ticket invalidated by a nested save.
  first.Adopt(*first_projection);
  Check(!first.boundary.FinishTransition(first_ticket, first.identity, first.source, false));
  Check(first.boundary.State() == handoff::Phase::Restore);
  first.Save(0, &first_replacement);
  Check(first.saves == 2 && first.last_lookup == -1 && first.source == &first.canonical);

  Host unready;
  unready.Activate();
  auto       pending        = unready.MakeProjection(true);
  const auto pending_ticket = unready.boundary.BeginTransition(pending);
  Check(pending_ticket);
  unready.Adopt(*pending); // A failed rebuild can still adopt data and mutate map.
  Check(!unready.boundary.FinishTransition(pending_ticket, unready.identity, unready.source, false));
  Check(unready.boundary.GuardRequired(unready.identity));
  unready.Save(0, &first_replacement);
  Check(unready.saves == 1 && unready.last_lookup == -1 && unready.source == &unready.canonical);
}

void DisableReleaseAndStaleScopes()
{
  Host host;
  host.Activate();
  host.boundary.RequestCanonical(); // Disable is independent of active guard ownership.
  Item replacement{Filled(0, 0), 44};
  host.Save(0, &replacement);
  Check(host.saves == 1 && host.last_lookup == -1 && host.source == &host.canonical);
  Check(!host.boundary.ConfirmCanonical(host.identity, host.source, false));
  Check(host.boundary.GuardRequired(host.identity));
  Check(host.boundary.ConfirmCanonical(host.identity, host.source, true));
  Check(!host.boundary.GuardRequired(host.identity));

  Host releasing;
  releasing.Activate();
  releasing.boundary.BeginRelease(); // Register before any reentrant release-side restoration.
  releasing.Save(0, &replacement);
  Check(releasing.saves == 1 && releasing.last_lookup == -1 && releasing.source == &releasing.canonical);
  Check(!releasing.boundary.ConfirmCanonical(releasing.identity, releasing.source, true));
  int release_calls = 0, exception_identity = 0;
  try {
    ++release_calls; // The native release original is outside optional exception containment.
    throw &exception_identity;
  } catch (int *observed) {
    Check(observed == &exception_identity);
  }
  Check(release_calls == 1 && releasing.boundary.GuardRequired(releasing.identity));
  releasing.identity.context = nullptr; // Actual native ownership invalidation is a separate observation.
  releasing.boundary.Detach();
  Check(!releasing.boundary.GuardRequired(releasing.identity));

  Host rebound;
  rebound.Activate();
  const auto old_scroller = rebound.identity.scroller;
  handoff::SaveOnce(rebound.boundary, rebound, [&] {
    ++rebound.saves;
    ++rebound.identity.generation; // Same physical source pointers, new bound generation.
    rebound.boundary.Bind(rebound.identity);
    rebound.source     = &rebound.next_display;
    Receiver *receiver = nullptr;
    Check(
        !handoff::LookupOnce(rebound.boundary, rebound, old_scroller, 0, receiver,
                             [&](std::int32_t view, Receiver *&result) { return rebound.NativeLookup(view, result); }));
    Check(rebound.last_lookup == -1 && !receiver);
  });
  Check(rebound.saves == 1 && rebound.source == &rebound.next_display && rebound.source_writes == 1);

  Host foreign;
  foreign.Activate();
  handoff::SaveOnce(foreign.boundary, foreign, [&] {
    ++foreign.saves;
    foreign.source = &foreign.source_writes; // An unrelated source must not receive old cleanup.
  });
  Check(foreign.saves == 1 && foreign.source == &foreign.source_writes && foreign.source_writes == 1);
  Check(foreign.boundary.State() == handoff::Phase::Restore);
}

void FailureReadbacks()
{
  Host applied;
  applied.Activate();
  applied.canonical_write = Write::AppliedButReportedFailure;
  applied.restore_write   = Write::AppliedButReportedFailure;
  Item replacement{Filled(0, 0), 55};
  applied.Save(0, &replacement);
  Check(applied.saves == 1 && applied.source == &applied.display && !applied.contradiction);

  for (bool side_effect : {false, true}) {
    Host sync;
    sync.Activate();
    sync.fail_sync        = true;
    sync.sync_side_effect = side_effect;
    sync.Save(0, &replacement);
    Check(sync.saves == 1 && sync.lookups == 1 && sync.last_lookup == -1 && sync.binds == 0);
    Check(sync.source == &sync.canonical && sync.boundary.State() == handoff::Phase::Restore);
  }

  Host cleanup;
  cleanup.Activate();
  cleanup.restore_write = Write::Refuse;
  cleanup.Save(0, &replacement);
  Check(cleanup.saves == 1 && cleanup.source == &cleanup.canonical);
  Check(cleanup.boundary.State() == handoff::Phase::Restore && cleanup.boundary.GuardRequired(cleanup.identity));
  Check(replacement.row.presentation == cleanup.native_presentations[0]);

  Host throw_cleanup;
  throw_cleanup.Activate();
  throw_cleanup.restore_write = Write::Refuse;
  int exception_token = 0, original_calls = 0;
  try {
    handoff::SaveOnce(throw_cleanup.boundary, throw_cleanup, [&]() -> void {
      ++original_calls;
      throw &exception_token;
    });
    Check(false);
  } catch (int *observed) {
    Check(observed == &exception_token);
  }
  Check(original_calls == 1 && throw_cleanup.source == &throw_cleanup.canonical);
  Check(throw_cleanup.boundary.State() == handoff::Phase::Restore);

  Host cleanup_side_effect;
  cleanup_side_effect.Activate();
  cleanup_side_effect.restore_write = Write::ForeignSideEffect;
  cleanup_side_effect.Save(0, &replacement);
  Check(cleanup_side_effect.saves == 1 && cleanup_side_effect.source == &cleanup_side_effect.source_writes);
  Check(cleanup_side_effect.boundary.GuardRequired(cleanup_side_effect.identity));

  // These are explicit UNSUPPORTED native-adapter contracts. There is no claim
  // that a native original was preserved or safely executed on these paths.
  // Root review must keep product hooks inactive while this remains possible.
  for (auto action : {Write::Refuse, Write::ForeignSideEffect}) {
    Host unsupported;
    unsupported.Activate();
    unsupported.canonical_write = action;
    try {
      unsupported.Save(0, &replacement);
      Check(false);
    } catch (const Unsupported &) {
      Check(unsupported.contradiction && unsupported.saves == 0);
    }
  }

  Host same_size;
  same_size.Activate();
  Item unrelated{Filled(123, 0), 88};
  same_size.Save(0, &unrelated);
  Check(same_size.saves == 1 && same_size.binds == 0 && same_size.last_lookup == -1);
  Check(same_size.source == &same_size.canonical && same_size.boundary.State() == handoff::Phase::Restore);

  Host during_sync;
  during_sync.Activate();
  during_sync.replace_owner_during_sync = true;
  during_sync.Save(0, &replacement);
  Check(during_sync.saves == 1 && during_sync.binds == 0 && during_sync.last_lookup == -1);
  Check(during_sync.source == &during_sync.canonical && during_sync.source_writes == 1);
}

void NestedAndPassthrough()
{
  Host host;
  host.Activate();
  int        calls  = 0;
  const auto result = handoff::SaveOnce(host.boundary, host, [&] {
    ++calls;
    Check(host.source == &host.canonical);
    const auto nested = handoff::SaveOnce(host.boundary, host, [&] {
      ++calls;
      Check(host.source == &host.canonical);
      return 17;
    });
    Check(nested == 17 && host.source == &host.canonical);
    Check(host.items[0].row.presentation == 10 && host.items[2].row.presentation == 12);
    Receiver *receiver = nullptr;
    Check(host.Lookup(0, receiver) && host.last_lookup == 2);
    return 23;
  });
  Check(calls == 2 && result == 23 && host.source == &host.display);
  const auto same_source = std::make_shared<handoff::Projection>(
      handoff::Projection{&host.display, std::vector<std::size_t>{2, 1, 0, 3, 4, 5}});
  Check(!host.boundary.BeginTransition(same_source)); // Active source cannot be mutated in place.

  Host native;
  int  native_calls = 0;
  Check(handoff::SaveOnce(native.boundary, native,
                          [&] {
                            ++native_calls;
                            return 42;
                          })
        == 42);
  Check(native_calls == 1 && native.source_writes == 0 && native.syncs == 0);

  // An unprojected save also excludes a first projection until original returns.
  auto first_projection = native.MakeProjection();
  handoff::SaveOnce(native.boundary, native, [&] {
    ++native_calls;
    Check(!native.boundary.BeginTransition(first_projection));
    Receiver *native_receiver = nullptr;
    Check(native.Lookup(0, native_receiver) && native.last_lookup == 0);
    ++native.identity.generation;
    native.boundary.Bind(native.identity);
    Check(!native.boundary.BeginTransition(first_projection));
  });
  Check(native_calls == 2 && native.source_writes == 0);
  Check(native.boundary.BeginTransition(first_projection));

  Receiver *receiver = nullptr;
  Check(handoff::LookupOnce(host.boundary, host, &native, 1, receiver,
                            [&](std::int32_t view, Receiver *&result_receiver) {
                              Check(view == 1);
                              return host.NativeLookup(view, result_receiver);
                            }));
  Check(receiver == &host.receivers[1]); // Other scrollers and out-of-save calls pass through.

  auto malformed = std::make_shared<handoff::Projection>(handoff::Projection{&native.display, {0, 0}});
  Check(!native.boundary.BeginTransition(malformed));
  Check(!handoff::Permutation(std::array<std::size_t, 2>{0, 2}));
  Check(handoff::DisplayIndex(std::array<std::size_t, 3>{2, 0, 1}, -1) == -1);
  Check(handoff::DisplayIndex(std::array<std::size_t, 3>{2, 0, 1}, 3) == -1);
}

void ReceiverQualification()
{
  Host wrong;
  wrong.Activate();
  Item foreign{Filled(0, 0), 7, 2};
  wrong.receivers[2].item = &foreign; // Same canonical number from an unrelated owner.
  Item replacement{Filled(0, 0), 8};
  wrong.Save(0, &replacement);
  Check(wrong.saves == 1 && wrong.lookups == 1 && wrong.binds == 0 && wrong.last_lookup == 2);
  Check(wrong.canonical[0] == &replacement && wrong.source == &wrong.canonical);
  Check(wrong.boundary.State() == handoff::Phase::Restore);

  Host rebound;
  rebound.Activate();
  int native_lookups = 0;
  handoff::SaveOnce(rebound.boundary, rebound, [&] {
    Receiver *receiver = nullptr;
    Check(!handoff::LookupOnce(rebound.boundary, rebound, rebound.identity.scroller, 0, receiver,
                               [&](std::int32_t view, Receiver *&result) {
                                 ++native_lookups;
                                 Check(view == 2);
                                 result = &rebound.receivers[2];
                                 ++rebound.identity.generation;
                                 rebound.boundary.Bind(rebound.identity);
                                 rebound.source = &rebound.next_display;
                                 return true;
                               }));
    Check(!receiver);
  });
  Check(native_lookups == 1 && rebound.source == &rebound.next_display && rebound.source_writes == 1);

  Host virtualized;
  virtualized.Activate();
  handoff::SaveOnce(virtualized.boundary, virtualized, [&] {
    Receiver *receiver = nullptr;
    Check(!handoff::LookupOnce(virtualized.boundary, virtualized, virtualized.identity.scroller, 0, receiver,
                               [&](std::int32_t view, Receiver *&) {
                                 Check(view == 2);
                                 return false; // Native has no visible receiver for this valid display index.
                               }));
    Check(!receiver && virtualized.boundary.State() == handoff::Phase::Projected);
  });
  Check(virtualized.source == &virtualized.display);
}
} // namespace

int main()
{
  ExactOnceAndReplacement();
  TransitionReentrancy();
  DisableReleaseAndStaleScopes();
  FailureReadbacks();
  NestedAndPassthrough();
  ReceiverQualification();
  std::cout << "Officer preset handoff experiment passed: " << checks
            << " checks; contradictory canonical writers remain unsupported\n";
}
