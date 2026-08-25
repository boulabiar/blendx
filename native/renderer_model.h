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

namespace blendx {

struct Dimension {
  bool set = false;
  bool percent = false;
  double value = 0.0;
  double resolve(double available, double fallback) const {
    if (!set) return fallback;
    return percent ? available * value * 0.01 : value;
  }
};

struct Style {
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
  std::optional<uint32_t> background;
  std::optional<uint32_t> color;
  std::optional<uint32_t> border_color;
  double font_size = 16.0;
  double line_height = 0.0;
  double border_radius = 0.0;
  double border_width = 0.0;
  double opacity = 1.0;
  bool visible = true;
  enum class WhiteSpace { kNoWrap, kNormal, kPre, kPreWrap } white_space = WhiteSpace::kNoWrap;
  enum class Overflow { kVisible, kHidden, kScroll } overflow = Overflow::kVisible;
  enum class Position { kRelative, kAbsolute, kFixed } position = Position::kRelative;
  enum class Align { kStart, kCenter, kEnd, kStretch } align_items = Align::kStretch;
  enum class Justify { kStart, kCenter, kEnd, kSpaceBetween } justify = Justify::kStart;
  std::optional<double> left;
  std::optional<double> right;
  std::optional<double> top;
  std::optional<double> bottom;
  double z_index = 0.0;
  bool layout_contain = false;
  Dimension max_width;
  Dimension max_height;

  bool same_layout(const Style& other) const {
    auto same_dimension = [](const Dimension& a, const Dimension& b) {
      return a.set == b.set && a.percent == b.percent && a.value == b.value;
    };
    return same_dimension(width, other.width) && same_dimension(height, other.height) &&
           min_width == other.min_width && min_height == other.min_height && row == other.row &&
           flex_grow == other.flex_grow && flex_shrink == other.flex_shrink && gap == other.gap &&
           padding_left == other.padding_left && padding_right == other.padding_right &&
           padding_top == other.padding_top && padding_bottom == other.padding_bottom &&
           margin_left == other.margin_left && margin_right == other.margin_right &&
           margin_top == other.margin_top && margin_bottom == other.margin_bottom &&
           font_size == other.font_size && line_height == other.line_height &&
           position == other.position && left == other.left && right == other.right &&
           top == other.top && bottom == other.bottom && align_items == other.align_items &&
           justify == other.justify && same_dimension(max_width, other.max_width) &&
           same_dimension(max_height, other.max_height) && white_space == other.white_space &&
           layout_contain == other.layout_contain;
  }

  bool same_visual(const Style& other) const {
    return background == other.background && color == other.color &&
           border_color == other.border_color && border_radius == other.border_radius &&
           border_width == other.border_width && opacity == other.opacity &&
           visible == other.visible && overflow == other.overflow && z_index == other.z_index;
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
  std::unordered_map<std::string, PropValue> props;
  double scroll_y = 0.0;
  double last_applied_scroll_y = 0.0;
  double scroll_target_y = 0.0;
  double content_height = 0.0;
  double item_height = 28.0;
  uint32_t overdraw = 2;
  size_t visible_start = 0;
  size_t visible_end = 0;
  size_t last_child_count = 0;
  size_t selection_start = 0;
  size_t selection_end = 0;
  std::vector<std::string> undo_stack;
  std::vector<std::string> redo_stack;
  std::string composition;
  YGNodeRef yoga = nullptr;
  std::shared_ptr<YogaNodeContext> yoga_context;
};

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
const T* prop_as(const Node& node, const char* name) {
  auto it = node.props.find(name);
  return it == node.props.end() ? nullptr : std::get_if<T>(&it->second);
}
inline double number_prop(const Node& node, const char* name, double fallback = 0.0) {
  if (const double* value = prop_as<double>(node, name)) return *value;
  return fallback;
}
inline bool bool_prop(const Node& node, const char* name, bool fallback = false) {
  if (const bool* value = prop_as<bool>(node, name)) return *value;
  return fallback;
}
inline std::string string_prop(const Node& node, const char* name, const std::string& fallback = {}) {
  if (const std::string* value = prop_as<std::string>(node, name)) return *value;
  return fallback;
}

}  // namespace blendx
