/// @file
/// @brief XML values' nodes [CLIENT XML values]: a serialization parsed as
/// well-formed XML content into a tree of a document node, elements,
/// attributes, text, comments and processing instructions, namespaces
/// resolved (RULES.md D2).

#ifndef TURBOREL_XML_HPP
#define TURBOREL_XML_HPP

#include <turborel/export.hpp>
#include <turborel/value.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

struct trl_xml;
struct trl_xml_node;

namespace turborel {

/// The kinds of node.
enum class xml_kind : std::uint8_t {
  /// The document node, the tree's root.
  document,
  /// An element.
  element,
  /// An attribute of an element.
  attribute,
  /// Text, CDATA sections and references among it.
  text,
  /// A comment.
  comment,
  /// A processing instruction.
  processing_instruction,
};

/// A node of a tree: a view, valid while its tree is.
class TURBOREL_CPP_EXPORT xml_node {
public:
  /// A view of turborel-c's node.
  /// @param node The node.
  explicit xml_node(const trl_xml_node* node) noexcept : node_{node} {}

  /// Its kind.
  /// @return The kind.
  [[nodiscard]] xml_kind kind() const noexcept;

  /// An element's or attribute's local name, or a processing instruction's
  /// target.
  /// @return The name; none of another kind.
  [[nodiscard]] std::optional<std::string_view> name() const noexcept;

  /// An element's or attribute's namespace URI.
  /// @return The URI; none if it is in none.
  [[nodiscard]] std::optional<std::string_view> namespace_uri() const noexcept;

  /// An element's or attribute's prefix.
  /// @return The prefix; none if it has none.
  [[nodiscard]] std::optional<std::string_view> prefix() const noexcept;

  /// A text's, comment's, processing instruction's or attribute's value.
  /// @return The value; none of an element or the document.
  [[nodiscard]] std::optional<std::string_view> value() const noexcept;

  /// How many children the document node or an element has.
  /// @return The count.
  [[nodiscard]] std::size_t child_count() const noexcept;

  /// A child, in document order.
  /// @param index Which, from 0.
  /// @return The child.
  /// @throws turborel::error Past the last (-910).
  [[nodiscard]] xml_node child(std::size_t index) const;

  /// How many attributes an element has, namespace declarations not
  /// counted.
  /// @return The count.
  [[nodiscard]] std::size_t attribute_count() const noexcept;

  /// An attribute, in the order written.
  /// @param index Which, from 0.
  /// @return The attribute.
  /// @throws turborel::error Past the last (-910).
  [[nodiscard]] xml_node attribute(std::size_t index) const;

private:
  const trl_xml_node* node_;
};

/// A tree of nodes parsed from an XML serialization.
class TURBOREL_CPP_EXPORT xml_tree {
public:
  /// Parses a serialization as well-formed XML content.
  /// @param text The serialization, UTF-8.
  /// @return The tree.
  /// @throws turborel::error Of text that is not (-301, 2200N).
  [[nodiscard]] static xml_tree parse(std::string_view text);

  /// Parses an XML value's serialization.
  /// @param xml An XML value.
  /// @return The tree.
  /// @throws turborel::error As parse() does; of a value of another kind
  ///   (-910).
  [[nodiscard]] static xml_tree of(const value& xml);

  /// The document node.
  /// @return The node.
  [[nodiscard]] xml_node root() const noexcept;

private:
  /// Frees a tree.
  struct freer {
    /// Frees `made`.
    /// @param made The tree.
    void operator()(trl_xml* made) const noexcept;
  };

  explicit xml_tree(trl_xml* made) noexcept : tree_{made} {}

  std::unique_ptr<trl_xml, freer> tree_;
};

} // namespace turborel

#endif // TURBOREL_XML_HPP
