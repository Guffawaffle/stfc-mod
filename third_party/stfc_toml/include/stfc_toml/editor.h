#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <toml++/toml.h>
#include <utility>
#include <vector>

// This source-only core owns no files, processes, game state or network access.
namespace stfc::toml_edit
{
using Path = std::vector<std::string>;
struct Failure : std::runtime_error {
  std::optional<std::uint32_t> line;
  explicit Failure(const char* code, std::optional<std::uint32_t> at = {})
      : std::runtime_error(code)
      , line(at)
  {
  }
};
struct Override {
  Path          path;
  std::string   value, semantic_value;
  std::uint32_t line;
};
struct Table {
  Path          path;
  std::uint32_t line;
};
struct Snapshot {
  std::vector<Override> overrides;
  std::vector<Table>    tables;
};

inline void ValidateUtf8(std::string_view text)
{
  for (std::size_t i = 0; i < text.size();) {
    const auto first = static_cast<unsigned char>(text[i++]);
    if (first < 0x80)
      continue;
    unsigned      count;
    std::uint32_t code;
    if (first >= 0xc2 && first <= 0xdf) {
      count = 1;
      code  = first & 0x1f;
    } else if (first >= 0xe0 && first <= 0xef) {
      count = 2;
      code  = first & 0x0f;
    } else if (first >= 0xf0 && first <= 0xf4) {
      count = 3;
      code  = first & 7;
    } else
      throw Failure("InvalidUtf8");
    if (i + count > text.size())
      throw Failure("InvalidUtf8");
    for (unsigned n = 0; n < count; ++n) {
      const auto next = static_cast<unsigned char>(text[i++]);
      if ((next & 0xc0) != 0x80)
        throw Failure("InvalidUtf8");
      code = (code << 6) | (next & 0x3f);
    }
    if ((count == 1 && code < 0x80) || (count == 2 && code < 0x800) || (count == 3 && code < 0x10000) || code > 0x10ffff
        || (code >= 0xd800 && code <= 0xdfff))
      throw Failure("InvalidUtf8");
  }
}
inline toml::table Parse(std::string_view text)
{
  ValidateUtf8(text);
  try {
    return toml::parse(text);
  } catch (const toml::parse_error& error) {
    const auto description = error.description();
    const auto duplicate   = description.find("redefine") != std::string_view::npos
                             || description.find("duplicate") != std::string_view::npos;
    throw Failure(duplicate ? "DuplicateTarget" : "InvalidDocument", error.source().begin.line);
  }
}
inline std::string Render(const toml::node& value)
{
  std::ostringstream output;
  output.exceptions(std::ios::badbit | std::ios::failbit);
  output << toml::toml_formatter(value, toml::format_flags::none);
  return output.str();
}
inline toml::table ValueDocument(std::string_view value)
{
  try {
    auto result = Parse(std::string("__stfc_value__ = ") + std::string(value));
    if (result.size() != 1 || !result.contains("__stfc_value__"))
      throw Failure("InvalidValue");
    return result;
  } catch (const Failure&) {
    throw Failure("InvalidValue");
  }
}
inline std::string NormalizeValue(std::string_view value)
{
  const auto parsed = ValueDocument(value);
  return Render(*parsed.get("__stfc_value__"));
}
inline std::string EncodeKey(std::string_view value)
{
  const bool bare = !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
  });
  return bare ? std::string(value) : Render(toml::value(std::string(value)));
}
inline std::string CanonicalPath(const Path& path)
{
  std::string result;
  for (const auto& part : path) {
    if (!result.empty())
      result += '.';
    result += EncodeKey(part);
  }
  return result;
}
inline bool Prefix(const Path& prefix, const Path& path)
{ return prefix.size() <= path.size() && std::equal(prefix.begin(), prefix.end(), path.begin()); }
inline Path Append(Path prefix, const Path& suffix)
{
  prefix.insert(prefix.end(), suffix.begin(), suffix.end());
  return prefix;
}
inline Path Relative(const Path& path, std::size_t prefix)
{ return Path(path.begin() + prefix, path.end()); }
inline void CheckPath(const Path& path)
{
  if (path.empty())
    throw Failure("InvalidPath");
  for (const auto& part : path)
    ValidateUtf8(part);
}

namespace detail
{
  struct Patch {
    std::size_t begin, end;
    std::string replacement;
  };
  struct Header {
    Path        path;
    std::size_t begin, end, key_begin, key_end;
    bool        array;
  };
  struct Assignment {
    Path        path, context;
    std::size_t begin, end, key_begin, key_end, value_begin, value_end;
  };
  struct Field {
    Path        path;
    std::size_t key_begin, key_end, value_begin, value_end;
  };
  struct Inline {
    Path               path;
    std::size_t        begin, end;
    std::vector<Field> fields;
  };

  class Document
  {
    struct Utf8Adjustment {
      std::size_t   line_start;
      std::uint32_t column_after;
      std::size_t   extra_bytes;
    };
    std::vector<Utf8Adjustment> utf8_adjustments_;

  public:
    std::string                        text;
    toml::table                        tree;
    std::vector<Header>                headers;
    std::vector<Assignment>            assignments;
    std::vector<Inline>                inlines;
    std::map<std::size_t, std::size_t> value_ends;
    std::vector<std::size_t>           lines;

    explicit Document(std::string_view source)
        : text(source)
        , tree(Parse(source))
    {
      IndexCoordinates();

      CollectRegions(tree);
      Scan();
    }
    std::size_t Bom() const
    { return text.starts_with("\xef\xbb\xbf") ? 3 : 0; }
    std::size_t Offset(toml::source_position position) const
    {
      if (!position.line || position.line > lines.size() || !position.column)
        throw Failure("UnsupportedTarget");
      const auto line_start = lines[position.line - 1];
      const auto coordinate = std::pair{line_start, static_cast<std::uint32_t>(position.column)};
      const auto next       = std::upper_bound(utf8_adjustments_.begin(), utf8_adjustments_.end(), coordinate,
                                               [](const auto& requested, const Utf8Adjustment& adjustment) {
                                           return requested.first < adjustment.line_start
                                                  || (requested.first == adjustment.line_start
                                                      && requested.second < adjustment.column_after);
                                               });
      const auto extra      = next != utf8_adjustments_.begin() && std::prev(next)->line_start == line_start
                                  ? std::prev(next)->extra_bytes
                                  : 0;
      const auto offset     = line_start + static_cast<std::size_t>(position.column) - 1 + extra;
      const auto line_end   = position.line < lines.size() ? lines[position.line] - 1 : text.size();
      if (offset > line_end)
        throw Failure("UnsupportedTarget");
      return offset;
    }
    std::uint32_t Line(std::size_t offset) const
    { return static_cast<std::uint32_t>(std::upper_bound(lines.begin(), lines.end(), offset) - lines.begin()); }
    bool PhysicalInline(const toml::node& node) const
    {
      if (!node.is_table() || !node.as_table()->is_inline())
        return false;
      const auto begin = Offset(node.source().begin), end = Offset(node.source().end);
      return begin < end && end <= text.size() && text[begin] == '{' && text[end - 1] == '}';
    }
    bool ArrayOfTables(const toml::node& node) const
    {
      if (!node.is_array_of_tables())
        return false;
      const auto begin = Offset(node.source().begin);
      return begin + 1 < text.size() && text[begin] == '[' && text[begin + 1] == '[';
    }
    const toml::node* Find(const Path& path) const
    {
      const toml::node* current = &tree;
      for (const auto& part : path) {
        if (!current->is_table())
          throw Failure("UnsupportedTarget");
        current = current->as_table()->get(part);
        if (!current)
          return nullptr;
      }
      return current;
    }
    bool Explicit(const Path& path) const
    {
      for (const auto& header : headers)
        if (header.path == path && !header.array)
          return true;
      const auto* node = Find(path);
      return node && PhysicalInline(*node);
    }
    std::string Newline() const
    { return text.find("\r\n") != std::string::npos ? "\r\n" : "\n"; }
    bool Accepts(const std::string& candidate, const toml::table& desired) const
    {
      try {
        return Parse(candidate) == desired;
      } catch (const Failure&) {
        return false;
      }
    }
    std::optional<std::string> Apply(std::vector<Patch> patches, const toml::table& desired) const
    {
      std::sort(patches.begin(), patches.end(), [](const Patch& a, const Patch& b) { return a.begin > b.begin; });
      std::size_t boundary  = text.size();
      auto        candidate = text;
      for (const auto& patch : patches) {
        if (patch.end < patch.begin || patch.end > boundary)
          return {};
        candidate.replace(patch.begin, patch.end - patch.begin, patch.replacement);
        boundary = patch.begin;
      }
      if (Accepts(candidate, desired))
        return candidate;
      return {};
    }
    Snapshot Read() const
    {
      Snapshot result;
      ReadTable(tree, {}, result);
      return result;
    }

  private:
    void IndexCoordinates()
    {
      auto line_start = Bom();
      lines.push_back(line_start);
      std::uint32_t column      = 1;
      std::size_t   extra_bytes = 0;
      for (auto offset = line_start; offset < text.size();) {
        if (text[offset] == '\n') {
          lines.push_back(++offset);
          line_start  = offset;
          column      = 1;
          extra_bytes = 0;
          continue;
        }
        const auto begin = offset++;
        while (offset < text.size() && (static_cast<unsigned char>(text[offset]) & 0xc0) == 0x80)
          ++offset;
        ++column;
        if (offset - begin > 1) {
          extra_bytes += offset - begin - 1;
          utf8_adjustments_.push_back({line_start, column, extra_bytes});
        }
      }
    }
    void CollectRegions(const toml::node& node)
    {
      if (node.is_value() || node.is_array() || PhysicalInline(node)) {
        const auto begin = Offset(node.source().begin), end = Offset(node.source().end);
        if (end > begin)
          value_ends[begin] = std::max(value_ends[begin], end);
      }
      if (auto table = node.as_table())
        for (const auto& [key, child] : *table) {
          (void)key;
          CollectRegions(child);
        }
      else if (auto array = node.as_array(); array && ArrayOfTables(node))
        for (const auto& child : *array)
          CollectRegions(child);
    }
    void ReadTable(const toml::table& table, const Path& parent, Snapshot& result) const
    {
      for (const auto& [key, node] : table) {
        auto path = parent;
        path.emplace_back(key.str());
        if (node.is_table()) {
          result.tables.push_back({path, node.source().begin.line});
          if (PhysicalInline(node))
            AddOverride(node, path, result);
          ReadTable(*node.as_table(), path, result);
        } else if (ArrayOfTables(node)) {
          // A declaration is not an atomic TOML value. Indexed reads are a separate API.
          result.tables.push_back({path, node.source().begin.line});
        } else
          AddOverride(node, path, result);
      }
    }
    void AddOverride(const toml::node& node, const Path& path, Snapshot& result) const
    {
      const auto begin = Offset(node.source().begin), end = Offset(node.source().end);
      if (end < begin || end > text.size())
        throw Failure("UnsupportedTarget");
      result.overrides.push_back({path, text.substr(begin, end - begin), Render(node), node.source().begin.line});
    }
    static Path DecodeKey(std::string_view key)
    {
      const auto        parsed  = Parse(std::string(key) + " = 0");
      const toml::node* current = &parsed;
      Path              path;
      while (current->is_table()) {
        const auto& table = *current->as_table();
        if (table.size() != 1)
          throw Failure("InternalError");
        const auto it = table.begin();
        path.emplace_back(it->first.str());
        current = &it->second;
      }
      if (!current->is_integer())
        throw Failure("InternalError");
      return path;
    }
    std::size_t KeyEnd(std::size_t begin, char terminator) const
    {
      char quote   = 0;
      bool escaped = false;
      for (auto i = begin; i < text.size(); ++i) {
        const char c = text[i];
        if (quote) {
          if (quote == '"' && escaped) {
            escaped = false;
            continue;
          }
          if (quote == '"' && c == '\\') {
            escaped = true;
            continue;
          }
          if (c == quote)
            quote = 0;
        } else if (c == '"' || c == '\'')
          quote = c;
        else if (c == terminator)
          return i;
        else if (c == '\n')
          break;
      }
      throw Failure("InternalError");
    }
    std::size_t Horizontal(std::size_t position) const
    {
      while (position < text.size() && (text[position] == ' ' || text[position] == '\t' || text[position] == '\r'))
        ++position;
      return position;
    }
    std::size_t StatementEnd(std::size_t position) const
    {
      position = Horizontal(position);
      if (position < text.size() && text[position] == '#') {
        position = text.find('\n', position);
        if (position == std::string::npos)
          return text.size();
      }
      if (position < text.size() && text[position] == '\n')
        return position + 1;
      if (position == text.size())
        return position;
      throw Failure("InternalError");
    }
    std::size_t ValueEnd(std::size_t begin) const
    {
      const auto found = value_ends.find(begin);
      if (found == value_ends.end())
        throw Failure("InternalError");
      return found->second;
    }
    void ScanInline(const Path& path, std::size_t begin, std::size_t end)
    {
      Inline group{path, begin, end, {}};
      auto   cursor = Horizontal(begin + 1);
      while (cursor < end - 1) {
        const auto key_end     = KeyEnd(cursor, '=');
        auto       field_path  = Append(path, DecodeKey(std::string_view(text).substr(cursor, key_end - cursor)));
        const auto value_begin = Horizontal(key_end + 1), value_end = ValueEnd(value_begin);
        group.fields.push_back({field_path, cursor, key_end, value_begin, value_end});
        if (text[value_begin] == '{')
          ScanInline(field_path, value_begin, value_end);
        cursor = Horizontal(value_end);
        if (cursor < end - 1 && text[cursor] == ',')
          cursor = Horizontal(cursor + 1);
        else if (cursor != end - 1)
          throw Failure("InternalError");
      }
      inlines.push_back(std::move(group));
    }
    void Scan()
    {
      auto cursor = Bom();
      Path context;
      while (cursor < text.size()) {
        const auto statement_begin = cursor;
        cursor                     = Horizontal(cursor);
        if (cursor == text.size())
          break;
        if (text[cursor] == '\n') {
          ++cursor;
          continue;
        }
        if (text[cursor] == '#') {
          cursor = StatementEnd(cursor);
          continue;
        }
        if (text[cursor] == '[') {
          const bool array     = cursor + 1 < text.size() && text[cursor + 1] == '[';
          const auto key_begin = cursor + (array ? 2 : 1), key_end = KeyEnd(key_begin, ']');
          context        = DecodeKey(std::string_view(text).substr(key_begin, key_end - key_begin));
          const auto end = StatementEnd(key_end + (array ? 2 : 1));
          headers.push_back({context, statement_begin, end, key_begin, key_end, array});
          cursor = end;
        } else {
          const auto key_begin = cursor, key_end = KeyEnd(cursor, '=');
          const auto path        = Append(context, DecodeKey(std::string_view(text).substr(cursor, key_end - cursor)));
          const auto value_begin = Horizontal(key_end + 1), value_end = ValueEnd(value_begin),
                     end = StatementEnd(value_end);
          assignments.push_back({path, context, statement_begin, end, key_begin, key_end, value_begin, value_end});
          if (text[value_begin] == '{')
            ScanInline(path, value_begin, value_end);
          cursor = end;
        }
      }
    }
  };

  inline toml::table& Parent(toml::table& root, const Path& path, bool create)
  {
    auto* table = &root;
    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
      auto* node = table->get(path[i]);
      if (!node && create)
        node = &table->insert(path[i], toml::table{}).first->second;
      if (!node || !node->is_table())
        throw Failure("UnsupportedTarget");
      table = node->as_table();
    }
    return *table;
  }
  inline void Erase(toml::table& root, const Path& path, const Document& document)
  {
    Parent(root, path, false).erase(path.back());
    for (auto count = path.size() - 1; count > 0; --count) {
      const Path parent(path.begin(), path.begin() + count);
      auto&      owner = Parent(root, parent, false);
      auto*      node  = owner.get(parent.back());
      if (!node || !node->is_table() || !node->as_table()->empty() || document.Explicit(parent))
        break;
      owner.erase(parent.back());
    }
  }
  inline std::vector<Patch> Removal(const Document& document, const Path& path)
  {
    std::vector<Patch> patches;
    for (const auto& header : document.headers)
      if (Prefix(path, header.path))
        patches.push_back({header.begin, header.end, {}});
    for (const auto& assignment : document.assignments)
      if (Prefix(path, assignment.path))
        patches.push_back({assignment.begin, assignment.end, {}});
    for (const auto& group : document.inlines) {
      // An ancestor assignment/field already owns removal of this container.
      if (Prefix(path, group.path) || !Prefix(group.path, path))
        continue;
      for (std::size_t first = 0; first < group.fields.size();) {
        if (!Prefix(path, group.fields[first].path)) {
          ++first;
          continue;
        }
        auto last = first;
        while (last + 1 < group.fields.size() && Prefix(path, group.fields[last + 1].path))
          ++last;
        if (last + 1 < group.fields.size())
          patches.push_back({group.fields[first].key_begin, group.fields[last + 1].key_begin, {}});
        else if (first > 0)
          patches.push_back({group.fields[first - 1].value_end, group.fields[last].value_end, {}});
        else
          patches.push_back({group.fields[first].key_begin, group.fields[last].value_end, {}});
        first = last + 1;
      }
    }
    return patches;
  }
  inline std::string Set(const Document& document, const Path& path, const toml::node& value)
  {
    const auto* current = document.Find(path);
    if (current && document.ArrayOfTables(*current))
      throw Failure("UnsupportedTarget");
    auto desired = document.tree;
    Parent(desired, path, true).insert_or_assign(path.back(), value);
    if (desired == document.tree)
      return document.text;
    const auto encoded = Render(value);
    if (current && (!current->is_table() || document.PhysicalInline(*current))) {
      if (auto candidate = document.Apply(
              {{document.Offset(current->source().begin), document.Offset(current->source().end), encoded}}, desired))
        return *candidate;
      throw Failure("UnsupportedTarget");
    }
    if (current)
      throw Failure("UnsupportedTarget");
    const auto newline = document.Newline();
    // A physical inline ancestor permits dotted-key insertion, including implicit
    // dotted descendants whose own source region is only a key token.
    for (auto group = document.inlines.rbegin(); group != document.inlines.rend(); ++group) {
      if (!Prefix(group->path, path) || group->path.size() >= path.size())
        continue;
      const auto assignment = CanonicalPath(Relative(path, group->path.size())) + " = " + encoded;
      const auto insertion  = (group->fields.empty() ? " " : ", ") + assignment + " ";
      if (auto candidate = document.Apply({{group->end - 1, group->end - 1, insertion}}, desired))
        return *candidate;
    }
    // Try relative assignments in the deepest existing declared ancestor first.
    std::vector<const Header*> ancestors;
    for (const auto& header : document.headers)
      if (!header.array && Prefix(header.path, path) && header.path.size() < path.size())
        ancestors.push_back(&header);
    std::sort(ancestors.begin(), ancestors.end(),
              [](const Header* a, const Header* b) { return a->path.size() > b->path.size(); });
    for (const auto* header : ancestors) {
      const auto assignment = CanonicalPath(Relative(path, header->path.size())) + " = " + encoded + newline;
      const bool terminated = header->end && document.text[header->end - 1] == '\n';
      if (auto candidate =
              document.Apply({{header->end, header->end, (terminated ? "" : newline) + assignment}}, desired))
        return *candidate;
    }
    auto append_section = [&]() -> std::optional<std::string> {
      if (path.size() < 2)
        return {};
      const Path parent(path.begin(), path.end() - 1);
      auto suffix = "[" + CanonicalPath(parent) + "]" + newline + EncodeKey(path.back()) + " = " + encoded + newline;
      if (document.text.size() > document.Bom() && document.text.back() != '\n')
        suffix = newline + suffix;
      return document.Apply({{document.text.size(), document.text.size(), suffix}}, desired);
    };
    if (path.size() > 1 && !document.Find(Path(path.begin(), path.end() - 1)))
      if (auto candidate = append_section())
        return *candidate;
    // Keep leading comments at the beginning; a root assignment belongs before
    // the first table header, or after existing root statements and comments.
    const auto root_position = document.headers.empty() ? document.text.size() : document.headers.front().begin;
    auto       rooted        = CanonicalPath(path) + " = " + encoded + newline;
    if (root_position == document.text.size() && document.text.size() > document.Bom() && document.text.back() != '\n')
      rooted = newline + rooted;
    if (auto candidate = document.Apply({{root_position, root_position, rooted}}, desired))
      return *candidate;
    if (auto candidate = append_section())
      return *candidate;
    throw Failure("UnsupportedTarget");
  }
  inline void MakeInline(toml::node& node)
  {
    if (auto* table = node.as_table()) {
      table->is_inline(true);
      for (auto& [key, child] : *table) {
        (void)key;
        MakeInline(child);
      }
    } else if (auto* array = node.as_array())
      for (auto& child : *array)
        MakeInline(child);
  }
  inline std::string Rename(const Document& document, const Path& source, const Path& destination)
  {
    const auto* selected = document.Find(source);
    if (!selected || !selected->is_table())
      throw Failure("UnsupportedTarget");
    if (source == destination)
      return document.text;
    if (document.Find(destination))
      throw Failure("DuplicateTarget");
    if (Prefix(source, destination))
      throw Failure("InvalidPath");
    auto desired = document.tree;
    // Clone before pruning source parents, then add to the destination.
    toml::table moved = *selected->as_table();
    Erase(desired, source, document);
    Parent(desired, destination, true).insert_or_assign(destination.back(), moved);
    auto replacement = [&](const Path& path) { return Append(destination, Relative(path, source.size())); };
    std::vector<Patch> patches;
    bool               direct = true;
    for (const auto& header : document.headers)
      if (Prefix(source, header.path))
        patches.push_back({header.key_begin, header.key_end, CanonicalPath(replacement(header.path))});
    for (const auto& assignment : document.assignments) {
      if (!Prefix(source, assignment.path) || Prefix(source, assignment.context))
        continue;
      const auto target = replacement(assignment.path);
      if (!Prefix(assignment.context, target)) {
        direct = false;
        break;
      }
      patches.push_back(
          {assignment.key_begin, assignment.key_end, CanonicalPath(Relative(target, assignment.context.size()))});
    }
    if (direct)
      for (const auto& group : document.inlines) {
        if (Prefix(source, group.path) || !Prefix(group.path, source))
          continue;
        for (const auto& field : group.fields)
          if (Prefix(source, field.path)) {
            const auto target = replacement(field.path);
            if (!Prefix(group.path, target)) {
              direct = false;
              break;
            }
            patches.push_back({field.key_begin, field.key_end, CanonicalPath(Relative(target, group.path.size()))});
          }
        if (!direct)
          break;
      }
    if (direct)
      if (auto candidate = document.Apply(std::move(patches), desired))
        return *candidate;
    // Cross-container moves may require a different declaration representation.
    // Only the selected subtree is re-rendered; unrelated source stays byte exact.
    auto removed = document.tree;
    Erase(removed, source, document);
    const auto removal = document.Apply(Removal(document, source), removed);
    if (removal) {
      MakeInline(moved);
      try {
        const auto candidate = Set(Document(*removal), destination, moved);
        if (document.Accepts(candidate, desired))
          return candidate;
      } catch (const Failure&) {
      }
    }
    throw Failure("UnsupportedTarget");
  }
} // namespace detail

inline void Validate(std::string_view text)
{ (void)Parse(text); }
inline Snapshot Read(std::string_view text)
{ return detail::Document(text).Read(); }
inline std::string Prepare(std::string_view text, std::string_view operation, const Path& path,
                           std::string_view value = {}, const Path& destination = {})
{
  CheckPath(path);
  const detail::Document document(text);
  if (operation == "set") {
    const auto parsed = ValueDocument(value);
    return detail::Set(document, path, *parsed.get("__stfc_value__"));
  }
  if (operation == "rename_table") {
    CheckPath(destination);
    return detail::Rename(document, path, destination);
  }
  if (operation != "remove" && operation != "remove_table")
    throw Failure("InvalidPath");
  const auto* current = document.Find(path);
  if (!current)
    return document.text;
  if (document.ArrayOfTables(*current))
    throw Failure("UnsupportedTarget");
  if (operation == "remove_table" && !current->is_table())
    throw Failure("UnsupportedTarget");
  auto desired = document.tree;
  detail::Erase(desired, path, document);
  if (auto candidate = document.Apply(detail::Removal(document, path), desired))
    return *candidate;
  throw Failure("UnsupportedTarget");
}
} // namespace stfc::toml_edit
