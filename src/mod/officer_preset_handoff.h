#pragma once

// Preset save/index protocol, shared with the controlled-runtime fixtures. The
// native adapter owns and roots every managed object; these pointers are identity
// tokens only. No hook installs or managed operations live here. Protocol tests
// alone do not establish native adapter or exact-client qualification.
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace officer_preset_handoff
{
struct Identity {
  std::uint64_t generation = 0;
  const void   *controller = nullptr, *context = nullptr, *scroller = nullptr, *canonical = nullptr;

  bool Valid() const noexcept
  { return generation && controller && context && scroller && canonical; }
  friend bool operator==(const Identity &, const Identity &) = default;
};

struct Projection {
  const void              *source = nullptr;
  std::vector<std::size_t> view_to_canonical;
};

inline bool Permutation(std::span<const std::size_t> mapping) noexcept
{
  if (mapping.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
    return false;
  for (std::size_t i = 0; i < mapping.size(); ++i) {
    if (mapping[i] >= mapping.size())
      return false;
    for (std::size_t previous = 0; previous < i; ++previous)
      if (mapping[previous] == mapping[i])
        return false;
  }
  return true;
}

inline std::int32_t DisplayIndex(std::span<const std::size_t> mapping, std::int32_t canonical) noexcept
{
  if (canonical < 0)
    return -1;
  for (std::size_t view = 0; view < mapping.size(); ++view)
    if (mapping[view] == static_cast<std::size_t>(canonical)
        && view <= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
      return static_cast<std::int32_t>(view);
  return -1;
}

enum class Phase { Native, Transition, Projected, Restore, Release, Detached };
struct SaveScope;

class Boundary
{
public:
  // Bind is called before any annotation, source write or native rebuild. A
  // rebind must supply a new generation even when every physical pointer agrees.
  void Bind(Identity identity) noexcept
  {
    identity_ = identity;
    ++revision_;
    phase_ = identity.Valid() ? Phase::Native : Phase::Detached;
    display_.reset();
    candidate_.reset();
  }

  std::uint64_t BeginTransition(std::shared_ptr<const Projection> candidate) noexcept
  {
    if (!identity_.Valid() || save_ || (phase_ != Phase::Native && phase_ != Phase::Projected) || !candidate
        || !candidate->source || candidate->source == identity_.canonical
        || (display_ && candidate->source == display_->source) || !Permutation(candidate->view_to_canonical))
      return 0;
    candidate_ = std::move(candidate);
    phase_     = Phase::Transition;
    return ++revision_;
  }

  // Invocation success is deliberately absent: a failed rebuild can still
  // install its source or partially mutate the renderer map. Only independent
  // source AND completed renderer-map observations may establish Projected.
  bool FinishTransition(std::uint64_t ticket, Identity observed, const void *source, bool renderer_ready) noexcept
  {
    if (!ticket || ticket != revision_ || !Current(observed) || phase_ != Phase::Transition || save_)
      return false;
    if (!candidate_ || source != candidate_->source || !renderer_ready) {
      phase_ = Phase::Restore;
      return false; // Keep both source leases while canonical repair is pending.
    }
    display_ = std::move(candidate_);
    phase_   = Phase::Projected;
    return true;
  }

  void RequestCanonical() noexcept
  {
    if (phase_ != Phase::Detached && phase_ != Phase::Native && phase_ != Phase::Release) {
      phase_ = Phase::Restore;
      ++revision_;
    }
  }
  void BeginRelease() noexcept
  {
    // Keep the guard if native release throws. Detach only after actual owner
    // invalidation; entering a release callback is not proof the view closed.
    if (phase_ != Phase::Detached) {
      phase_ = Phase::Release;
      ++revision_;
    }
  }
  bool ConfirmCanonical(Identity observed, const void *source, bool renderer_ready) noexcept
  {
    if (!Current(observed) || save_ || source != identity_.canonical || !renderer_ready || phase_ == Phase::Transition
        || phase_ == Phase::Release)
      return false;
    phase_ = Phase::Native;
    display_.reset();
    candidate_.reset();
    ++revision_;
    return true;
  }
  void Detach() noexcept
  {
    identity_ = {};
    phase_    = Phase::Detached;
    display_.reset();
    candidate_.reset();
    ++revision_;
  }

  bool Current(Identity observed) const noexcept
  { return identity_.Valid() && identity_ == observed; }
  bool GuardRequired(Identity observed) const noexcept
  {
    // Neither live enable nor the current _data pointer can bypass a guard.
    // During a rebuild _data can still be the OLD display array or a pending
    // NEW source, with neither renderer map qualified yet.
    return Current(observed) && phase_ != Phase::Native && phase_ != Phase::Detached;
  }
  bool OwnsSource(const void *source) const noexcept
  {
    return source
           && (source == identity_.canonical || (display_ && source == display_->source)
               || (candidate_ && source == candidate_->source));
  }
  Phase State() const noexcept
  { return phase_; }

private:
  friend struct SaveScope;
  template <class Host, class Native> friend decltype(auto) SaveOnce(Boundary &, Host &, Native &&);
  template <class Host, class Receiver, class Native>
  friend bool LookupOnce(Boundary &, Host &, const void *, std::int32_t, Receiver *&, Native &&);

  Identity                          identity_;
  Phase                             phase_    = Phase::Detached;
  std::uint64_t                     revision_ = 0;
  std::shared_ptr<const Projection> display_, candidate_;
  SaveScope                        *save_ = nullptr;
};

struct SaveScope {
  Identity                          identity;
  std::uint64_t                     revision;
  Phase                             phase;
  std::shared_ptr<const Projection> display, candidate;
  const void                       *source;
  SaveScope                        *previous;
  bool                              guarded;

  bool Current(const Boundary &boundary, Identity observed) const noexcept
  { return boundary.Current(observed) && identity == observed; }
  bool Translatable(const Boundary &boundary, Identity observed) const noexcept
  {
    return Current(boundary, observed) && revision == boundary.revision_ && phase == Phase::Projected
           && boundary.phase_ == Phase::Projected && display && boundary.display_ == display;
  }
  bool OwnsSource(const void *value) const noexcept
  {
    return value
           && (value == identity.canonical || (display && value == display->source)
               || (candidate && value == candidate->source));
  }
};

// Required native-adapter contract:
// - Observe/Source and all helpers used here are noexcept and do not allocate.
// - Canonical(scope) attempts native presentations, installs canonical _data by
//   a qualified synchronous NON-REENTRANT field operation, then verifies it.
// - Roots for the owner, canonical list and BOTH transition sources outlive the
//   scope; Sync/SyncAll use checked reads, barrier writes, identity readbacks and
//   distinguish new row tokens from the old row's display annotation.
// - RestoreSource verifies the write. It must not rebuild or invoke managed
//   callbacks. Failed synchronization keeps canonical _data and requests repair.
// Unsupported is an explicit experiment rejection, NOT a native fallback. There
// is no safe original-once continuation from an unrepairable canonical-write
// contradiction. Product activation requires establishing that this cannot
// happen for the qualified field operation; a no-op write trial is insufficient.
template <class Host, class Native> decltype(auto) SaveOnce(Boundary &boundary, Host &host, Native &&native)
{
  static_assert(noexcept(host.Observe()) && noexcept(host.Source()));
  const auto observed = host.Observe();
  const bool guarded  = boundary.GuardRequired(observed);

  // Track even native passthrough saves: a reentrant bind may not publish its
  // FIRST display while an earlier native save is still using canonical data.
  SaveScope scope{observed,
                  boundary.revision_,
                  guarded ? boundary.phase_ : Phase::Native,
                  guarded ? boundary.display_ : nullptr,
                  guarded ? boundary.candidate_ : nullptr,
                  guarded ? host.Source() : nullptr,
                  boundary.save_,
                  guarded};
  boundary.save_ = &scope; // Register before the first write; do not select by _data.
  struct Exit {
    Boundary  &boundary;
    Host      &host;
    SaveScope &scope;
    ~Exit() noexcept
    {
      if (scope.guarded && scope.Current(boundary, host.Observe()) && host.Source() == scope.identity.canonical) {
        const bool reusable = scope.revision == boundary.revision_ && scope.phase == Phase::Projected
                              && boundary.phase_ == Phase::Projected;
        const bool synced   = reusable && host.SyncAll(scope);
        // Recheck after reads/writes, including same-size canonical replacement.
        if (scope.Current(boundary, host.Observe()) && host.Source() == scope.identity.canonical) {
          // A save during rebuilding invalidates that transition. Old and new
          // arrays share row objects, so their destination Presentation values
          // cannot both be valid. Keep canonical data and demand canonical
          // renderer repair after the outer rebuild returns; never certify its
          // stale completion ticket just because it eventually adopts _data.
          const bool display_allowed = synced && scope.revision == boundary.revision_ && scope.phase == Phase::Projected
                                       && boundary.phase_ == Phase::Projected;
          if (display_allowed && scope.OwnsSource(scope.source)) {
            if (!host.RestoreSource(scope.source)) {
              if (scope.Current(boundary, host.Observe()) && host.Source() == scope.identity.canonical)
                (void)host.NativePresentations(scope);
              boundary.RequestCanonical();
            } else if (scope.source == scope.identity.canonical) {
              (void)host.NativePresentations(scope); // A nested save returns to its parent's canonical scope.
            }
          } else {
            (void)host.NativePresentations(scope); // Undo any partial display annotations.
            boundary.RequestCanonical();
          }
        }
      } else if (scope.guarded && scope.Current(boundary, host.Observe())) {
        boundary.RequestCanonical(); // A foreign source also invalidates renderer qualification.
      }
      boundary.save_ = scope.previous;
    }
  } exit{boundary, host, scope};

  static_assert(noexcept(host.Canonical(scope)) && noexcept(host.SyncAll(scope))
                && noexcept(host.RestoreSource(scope.source)) && noexcept(host.NativePresentations(scope)));
  if (!guarded)
    return std::forward<Native>(native)();
  if (!host.Canonical(scope) || !scope.Current(boundary, host.Observe()) || host.Source() != scope.identity.canonical) {
    host.Unsupported();
    std::terminate(); // Never silently run a canonical save against display _data.
  }
  return std::forward<Native>(native)(); // Exactly once; original exceptions escape unchanged.
}

template <class Host, class Receiver, class Native>
bool LookupOnce(Boundary &boundary, Host &host, const void *scroller, std::int32_t canonical, Receiver *&receiver,
                Native &&native)
{
  static_assert(noexcept(host.Observe()) && noexcept(host.Sync(std::declval<const SaveScope &>(), canonical, canonical))
                && noexcept(host.Matches(std::declval<const SaveScope &>(), receiver, canonical)));
  auto *scope = boundary.save_;
  while (scope && scope->identity.scroller != scroller)
    scope = scope->previous;
  if (!scope)
    return std::forward<Native>(native)(canonical, receiver);
  if (!scope->guarded)
    return std::forward<Native>(native)(canonical, receiver);

  auto view =
      scope->Translatable(boundary, host.Observe()) ? DisplayIndex(scope->display->view_to_canonical, canonical) : -1;
  if (view >= 0 && !host.Sync(*scope, canonical, view))
    view = -1;
  // Rebind/release or a sync failure must suppress this owned lookup, not fall
  // back to an unmodified canonical index in the old display renderer map.
  if (!scope->Translatable(boundary, host.Observe()))
    view = -1;
  const bool found = std::forward<Native>(native)(view, receiver);
  if (!found || view < 0 || !receiver || !scope->Translatable(boundary, host.Observe())
      || !host.Matches(*scope, receiver, canonical)) {
    // A valid virtualized miss is ordinary native behavior. A failed sync or
    // mismatched returned receiver invalidates our renderer qualification.
    if ((found || view < 0) && scope->Current(boundary, host.Observe()))
      boundary.RequestCanonical();
    receiver = nullptr;
    return false;
  }
  return true;
}
} // namespace officer_preset_handoff
