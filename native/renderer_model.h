#pragma once

#include <blend2d/blend2d.h>
#include <yoga/Yoga.h>

#include <algorithm>
#include <optional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "generated_custom_properties.h"

namespace blendx {

inline constexpr double kUnconstrainedMeasureWidth = 1'000'000.0;

struct DefaultPalette {
  static constexpr uint32_t text_strong = 0xFFFFFFFFu;
  static constexpr uint32_t code_chip = 0xFF273142u;
  static constexpr uint32_t link = 0xFF60A5FAu;
};

struct Dimension {
  bool set = false;
  bool percent = false;
  double value = 0.0;
  double resolve(double available, double fallback) const {
    if (!set) return fallback;
    return percent ? available * value * 0.01 : value;
  }
  bool operator==(const Dimension&) const = default;
};

struct LayoutStyle {
  Dimension width;
  Dimension height;
  double min_width = 0.0;
  double min_height = 0.0;
  bool row = false;
  double flex_grow = 0.0;
  double flex_shrink = 1.0;
  double gap = 0.0;
  double padding_left = 0.0;
  double padding_right = 0.0;
  double padding_top = 0.0;
  double padding_bottom = 0.0;
  double margin_left = 0.0;
  double margin_right = 0.0;
  double margin_top = 0.0;
  double margin_bottom = 0.0;
  double font_size = 16.0;
  double line_height = 0.0;
  enum class WhiteSpace { kNoWrap, kNormal, kPre, kPreWrap } white_space = WhiteSpace::kNoWrap;
  enum class Position { kRelative, kAbsolute, kFixed } position = Position::kRelative;
  enum class Align { kStart, kCenter, kEnd, kStretch } align_items = Align::kStretch;
  enum class Justify { kStart, kCenter, kEnd, kSpaceBetween } justify = Justify::kStart;
  std::optional<double> left;
  std::optional<double> right;
  std::optional<double> top;
  std::optional<double> bottom;
  bool layout_contain = false;
  Dimension max_width;
  Dimension max_height;

  bool operator==(const LayoutStyle&) const = default;
};

struct VisualStyle {
  std::optional<uint32_t> background;
  std::optional<uint32_t> color;
  std::optional<uint32_t> border_color;
  double border_radius = 0.0;
  double border_width = 0.0;
  double opacity = 1.0;
  bool visible = true;
  enum class Overflow { kVisible, kHidden, kScroll } overflow = Overflow::kVisible;
  double z_index = 0.0;

  bool operator==(const VisualStyle&) const = default;
};

struct Style : LayoutStyle, VisualStyle {

  bool same_layout(const Style& other) const {
    return static_cast<const LayoutStyle&>(*this) == static_cast<const LayoutStyle&>(other);
  }

  bool same_visual(const Style& other) const {
    return static_cast<const VisualStyle&>(*this) == static_cast<const VisualStyle&>(other);
  }
};

struct Box {
  double x = 0.0;
  double y = 0.0;
  double w = 0.0;
  double h = 0.0;
  bool contains(double px, double py) const {
    return px >= x && py >= y && px < x + w && py < y + h;
  }
};

struct ElementHandler {
  enum class Kind {
    kContainer, kText, kVirtualList, kImage, kSvg, kCanvas, kSeparator, kProgress,
    kAnchored, kMarkdown, kCode, kDiff, kInput,
  } kind;
  const char* name;
};

struct CanvasCommand {
  std::string kind;
  double x = 0.0;
  double y = 0.0;
  double x2 = 0.0;
  double y2 = 0.0;
  double width = 0.0;
  double height = 0.0;
  double radius = 0.0;
  double stroke_width = 1.0;
  double font_size = 14.0;
  uint32_t color = 0xFFFFFFFFu;
  bool fill = true;
  std::string text;
};

struct YogaNodeContext {
  void* renderer = nullptr;
  uint64_t id = 0;
};

struct ScrollState {
  double offset_y = 0.0;
  double last_applied_offset_y = 0.0;
  double target_y = 0.0;
  double content_height = 0.0;
};

struct VirtualListState {
  double item_height = 28.0;
  uint32_t overdraw = 2;
  size_t visible_start = 0;
  size_t visible_end = 0;
  size_t last_child_count = 0;
};

struct InputState {
  size_t selection_start = 0;
  size_t selection_end = 0;
  std::vector<std::string> undo_stack;
  std::vector<std::string> redo_stack;
  std::string composition;
};

using PropValue = std::variant<std::monostate, double, bool, std::string,
                               std::vector<CanvasCommand>, BLPoint>;

struct Node {
  uint64_t id = 0;
  std::string type;
  const ElementHandler* handler = nullptr;
  std::string text;
  Style style;
  Box box;
  Box paint_bounds;
  uint64_t parent = 0;
  std::vector<uint64_t> children;
  std::unordered_set<std::string> events;
  std::unordered_map<CustomPropertyId, PropValue> props;
  std::unique_ptr<ScrollState> scroll;
  std::unique_ptr<VirtualListState> virtual_list;
  std::unique_ptr<InputState> input;
  YGNodeRef yoga = nullptr;
  std::shared_ptr<YogaNodeContext> yoga_context;
};

inline ScrollState& ensure_scroll_state(Node& node) {
  if (!node.scroll) node.scroll = std::make_unique<ScrollState>();
  return *node.scroll;
}
inline VirtualListState& ensure_virtual_list_state(Node& node) {
  if (!node.virtual_list) node.virtual_list = std::make_unique<VirtualListState>();
  return *node.virtual_list;
}
inline InputState& ensure_input_state(Node& node) {
  if (!node.input) node.input = std::make_unique<InputState>();
  return *node.input;
}

struct Size { double w = 0.0; double h = 0.0; };
struct AccessibilityNode {
  uint64_t id = 0;
  std::string role;
  std::string label;
  std::string description;
  std::string value;
  std::string checked;
  bool disabled = false;
  bool selected = false;
  Box box;
};

template<typename T>
const T* prop_as(const Node& node, CustomPropertyId id) {
  auto it = node.props.find(id);
  return it == node.props.end() ? nullptr : std::get_if<T>(&it->second);
}
inline double number_prop(const Node& node, CustomPropertyId id, double fallback = 0.0) {
  if (const double* value = prop_as<double>(node, id)) return *value;
  return fallback;
}
inline bool bool_prop(const Node& node, CustomPropertyId id, bool fallback = false) {
  if (const bool* value = prop_as<bool>(node, id)) return *value;
  return fallback;
}
inline std::string string_prop(const Node& node, CustomPropertyId id, const std::string& fallback = {}) {
  if (const std::string* value = prop_as<std::string>(node, id)) return *value;
  return fallback;
}

}  // namespace blendx
