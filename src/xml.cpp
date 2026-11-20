/// @file
/// @brief XML values' nodes through turborel-c [CLIENT XML values].

#include <turborel/xml.hpp>

#include "capi.hpp"

#include <turborel/turborel.h>
#include <turborel/value.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace turborel {

xml_kind xml_node::kind() const noexcept {
  switch (trl_xml_node_kind(node_)) {
  case TRL_XML_NODE_ELEMENT:
    return xml_kind::element;
  case TRL_XML_NODE_ATTRIBUTE:
    return xml_kind::attribute;
  case TRL_XML_NODE_TEXT:
    return xml_kind::text;
  case TRL_XML_NODE_COMMENT:
    return xml_kind::comment;
  case TRL_XML_NODE_PROCESSING_INSTRUCTION:
    return xml_kind::processing_instruction;
  case TRL_XML_NODE_DOCUMENT:
    break;
  }
  return xml_kind::document;
}

std::optional<std::string_view> xml_node::name() const noexcept { return detail::maybe(trl_xml_node_name(node_)); }

std::optional<std::string_view> xml_node::namespace_uri() const noexcept {
  return detail::maybe(trl_xml_node_namespace(node_));
}

std::optional<std::string_view> xml_node::prefix() const noexcept { return detail::maybe(trl_xml_node_prefix(node_)); }

std::optional<std::string_view> xml_node::value() const noexcept { return detail::maybe(trl_xml_node_value(node_)); }

std::size_t xml_node::child_count() const noexcept { return trl_xml_node_child_count(node_); }

xml_node xml_node::child(std::size_t index) const {
  const trl_xml_node* found = trl_xml_node_child(node_, index);
  if (found == nullptr) {
    detail::raise_misuse("the node has no child " + std::to_string(index));
  }
  return xml_node{found};
}

std::size_t xml_node::attribute_count() const noexcept { return trl_xml_node_attribute_count(node_); }

xml_node xml_node::attribute(std::size_t index) const {
  const trl_xml_node* found = trl_xml_node_attribute(node_, index);
  if (found == nullptr) {
    detail::raise_misuse("the node has no attribute " + std::to_string(index));
  }
  return xml_node{found};
}

void xml_tree::freer::operator()(trl_xml* made) const noexcept { trl_xml_free(made); }

xml_tree xml_tree::parse(std::string_view text) {
  trl_xml* made = nullptr;
  const trl_status status = trl_xml_parse(text.empty() ? "" : text.data(), text.size(), &made);
  xml_tree tree{made};
  detail::check(status, trl_xml_failure(tree.tree_.get()));
  return tree;
}

xml_tree xml_tree::of(const value& xml) {
  trl_xml* made = nullptr;
  const trl_status status = trl_value_xml(xml.c_value(), &made);
  xml_tree tree{made};
  detail::check(status, trl_xml_failure(tree.tree_.get()));
  return tree;
}

xml_node xml_tree::root() const noexcept { return xml_node{trl_xml_root(tree_.get())}; }

} // namespace turborel
