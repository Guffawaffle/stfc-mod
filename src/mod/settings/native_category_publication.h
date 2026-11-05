#pragma once

#include <cstddef>
#include <optional>
#include <utility>

namespace mod_settings::native
{
enum class CategoryPublicationState { Idle, AwaitingReturn, Pending, Published, Removing, Retired };

// A complete tree is staged off-screen. Its exact root object and query delegate are known BEFORE AddChild
// publishes it; an append-then-throw cannot lose that identity. This is not the Boolean row insertion protocol.
template <class Handle> class CategoryPublication
{
public:
  using Object = decltype(std::declval<const Handle&>().Get());
  bool Begin(Object parent, Object root, Object query)
  {
    if (state_ != CategoryPublicationState::Idle)
      return false;
    parent_ = Handle::Weak(parent);
    root_   = Handle::Weak(root);
    query_  = Handle::Weak(query);
    if (!parent_ || !root_ || !query_)
      return false;
    state_ = CategoryPublicationState::AwaitingReturn;
    return true;
  }
  void Complete(bool succeeded)
  {
    if (state_ != CategoryPublicationState::AwaitingReturn)
      return;
    remove_ = remove_ || !succeeded;
    state_  = CategoryPublicationState::Pending;
  }
  void Revoke()
  {
    remove_ = true;
    if (state_ == CategoryPublicationState::Published)
      state_ = CategoryPublicationState::Removing;
  }
  CategoryPublicationState state() const
  { return state_; }
  Object root() const
  { return root_.Get(); }

  // Owned verifies exact query identity, Occurrences reads the whole native list, and Parent is a checked read.
  // Unknown observations preserve custody. Removal is complete only after the exact root is observed absent.
  template <class Host> void Retry(Host& host)
  {
    if (state_ == CategoryPublicationState::Idle || state_ == CategoryPublicationState::AwaitingReturn
        || state_ == CategoryPublicationState::Retired)
      return;
    const auto parent = parent_.Get(), root = root_.Get(), query = query_.Get();
    if (!parent || !root || !query) {
      state_ = CategoryPublicationState::Retired;
      return;
    }
    auto parent_root = Handle::Strong(parent), row_root = Handle::Strong(root), query_root = Handle::Strong(query);
    if (!parent_root || !row_root || !query_root)
      return;
    const auto owned = host.Owned(root, query);
    if (!owned)
      return;
    if (!*owned) {
      state_ = CategoryPublicationState::Retired;
      return; // Replaced callback identity never authorizes removal.
    }
    const auto count = host.Occurrences(parent, root);
    if (!count)
      return;
    if (*count == 0) {
      state_ = CategoryPublicationState::Retired;
      return;
    }
    const auto current_parent = host.Parent(root);
    if (!current_parent)
      return;
    // A changed parent may now own this root. Do not clear its relationship through another parent's removal.
    if (*current_parent && *current_parent != parent) {
      state_ = CategoryPublicationState::Retired;
      return;
    }
    if (!remove_ && *count == 1) {
      if (*current_parent == parent) {
        state_ = CategoryPublicationState::Published;
        return;
      }
    }
    remove_ = true;
    state_  = CategoryPublicationState::Removing;
    host.Remove(parent, root);
    const auto after = host.Occurrences(parent, root);
    if (after && *after == 0)
      state_ = CategoryPublicationState::Retired;
  }

private:
  Handle                   parent_, root_, query_;
  CategoryPublicationState state_  = CategoryPublicationState::Idle;
  bool                     remove_ = false;
};
} // namespace mod_settings::native
