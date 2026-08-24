#include <hermes/napi/node_api.h>

#include <SDL.h>
#include <blend2d/blend2d.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace {

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
  enum class Overflow { kVisible, kHidden, kScroll } overflow = Overflow::kVisible;
  enum class Position { kRelative, kAbsolute, kFixed } position = Position::kRelative;
  enum class Align { kStart, kCenter, kEnd, kStretch } align_items = Align::kStretch;
  enum class Justify { kStart, kCenter, kEnd, kSpaceBetween } justify = Justify::kStart;
  std::optional<double> left;
  std::optional<double> right;
  std::optional<double> top;
  std::optional<double> bottom;
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
           margin_left == other.margin_left &&
           margin_right == other.margin_right && margin_top == other.margin_top &&
           margin_bottom == other.margin_bottom && font_size == other.font_size &&
           line_height == other.line_height && position == other.position &&
           left == other.left && right == other.right && top == other.top &&
           bottom == other.bottom && align_items == other.align_items &&
           justify == other.justify && same_dimension(max_width, other.max_width) &&
           same_dimension(max_height, other.max_height);
  }


  bool same_visual(const Style& other) const {
    return background == other.background && color == other.color &&
           border_color == other.border_color && border_radius == other.border_radius &&
           border_width == other.border_width && opacity == other.opacity &&
           visible == other.visible && overflow == other.overflow;
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
    kContainer,
    kText,
    kVirtualList,
    kImage,
    kSvg,
    kCanvas,
    kSeparator,
    kProgress,
    kAnchored,
    kMarkdown,
    kCode,
    kDiff,
    kInput,
  } kind;
  const char* name;
};

class ElementRegistry {
 public:
  ElementRegistry() {
    register_handler("div", {ElementHandler::Kind::kContainer, "div"});
    register_handler("text", {ElementHandler::Kind::kText, "text"});
    register_handler("virtual-list", {ElementHandler::Kind::kVirtualList, "virtual-list"});
    register_handler("img", {ElementHandler::Kind::kImage, "img"});
    register_handler("svg", {ElementHandler::Kind::kSvg, "svg"});
    register_handler("canvas", {ElementHandler::Kind::kCanvas, "canvas"});
    register_handler("button", {ElementHandler::Kind::kContainer, "button"});
    register_handler("badge", {ElementHandler::Kind::kContainer, "badge"});
    register_handler("separator", {ElementHandler::Kind::kSeparator, "separator"});
    register_handler("progress", {ElementHandler::Kind::kProgress, "progress"});
    register_handler("anchored", {ElementHandler::Kind::kAnchored, "anchored"});
    register_handler("markdown", {ElementHandler::Kind::kMarkdown, "markdown"});
    register_handler("code", {ElementHandler::Kind::kCode, "code"});
    register_handler("diff", {ElementHandler::Kind::kDiff, "diff"});
    register_handler("input", {ElementHandler::Kind::kInput, "input"});
    register_handler("textarea", {ElementHandler::Kind::kInput, "textarea"});
  }

  void register_handler(std::string name, ElementHandler handler) {
    handlers_.insert_or_assign(std::move(name), handler);
  }

  const ElementHandler* resolve(const std::string& name) const {
    auto it = handlers_.find(name);
    if (it != handlers_.end()) return &it->second;
    return &handlers_.at("div");
  }

 private:
  std::unordered_map<std::string, ElementHandler> handlers_;
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

using PropValue = std::variant<std::monostate, double, bool, std::string,
                               std::vector<CanvasCommand>, BLPoint>;

struct Node {
  uint64_t id = 0;
  std::string type;
  const ElementHandler* handler = nullptr;
  std::string text;
  Style style;
  Box box;
  uint64_t parent = 0;
  std::vector<uint64_t> children;
  std::unordered_set<std::string> events;
  std::unordered_map<std::string, PropValue> props;
  double scroll_y = 0.0;
  double scroll_target_y = 0.0;
  double content_height = 0.0;
  double item_height = 28.0;
  uint32_t overdraw = 2;
  size_t visible_start = 0;
  size_t visible_end = 0;
  size_t last_child_count = 0;
};

struct Size {
  double w = 0.0;
  double h = 0.0;
};

template<typename T>
const T* prop_as(const Node& node, const char* name) {
  auto it = node.props.find(name);
  return it == node.props.end() ? nullptr : std::get_if<T>(&it->second);
}

double number_prop(const Node& node, const char* name, double fallback = 0.0) {
  if (const double* value = prop_as<double>(node, name)) return *value;
  return fallback;
}

bool bool_prop(const Node& node, const char* name, bool fallback = false) {
  if (const bool* value = prop_as<bool>(node, name)) return *value;
  return fallback;
}

std::string string_prop(const Node& node, const char* name,
                        const std::string& fallback = {}) {
  if (const std::string* value = prop_as<std::string>(node, name)) return *value;
  return fallback;
}

class Renderer {
 public:
  ~Renderer() { shutdown(); }

  bool init(const std::string& title, int width, int height, uint32_t threads,
            const std::string& font_path, bool headless, std::string& error) {
    shutdown();
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);
    threads_ = threads;
    headless_ = headless;
    running_ = true;

    if (font_face_.create_from_file(font_path.c_str()) != BL_SUCCESS) {
      error = "Could not load font: " + font_path;
      return false;
    }

    if (!headless_) {
      if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        error = std::string("SDL_Init failed: ") + SDL_GetError();
        return false;
      }
      sdl_initialized_ = true;
      window_ = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED,
                                 SDL_WINDOWPOS_CENTERED, width_, height_,
                                 SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
      if (!window_) {
        error = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
        shutdown();
        return false;
      }
    }

    if (!resize_framebuffer(width_, height_, error)) {
      shutdown();
      return false;
    }
    dirty_ = true;
    force_full_repaint_ = true;
    last_poll_at_ = std::chrono::steady_clock::now();
    return true;
  }

  void shutdown() {
    if (window_) SDL_DestroyWindow(window_);
    window_ = nullptr;
    if (sdl_initialized_) SDL_Quit();
    sdl_initialized_ = false;
    framebuffer_.reset();
    image_cache_.clear();
    svg_cache_.clear();
    nodes_.clear();
    root_id_ = 0;
    running_ = false;
    dirty_ = false;
    frame_count_ = 0;
    render_time_ms_ = 0.0;
    frame_samples_.clear();
    focused_id_ = 0;
    hovered_id_ = 0;
    dirty_regions_.clear();
    dirty_nodes_.clear();
    scrolling_nodes_.clear();
  }

  bool resize_framebuffer(int width, int height, std::string& error) {
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);
    if (framebuffer_.create(width_, height_, BL_FORMAT_PRGB32) != BL_SUCCESS) {
      error = "Blend2D could not allocate the framebuffer";
      return false;
    }
    dirty_ = true;
    force_full_repaint_ = true;
    return true;
  }

  void create_node(uint64_t id, std::string type) {
    Node node;
    node.id = id;
    node.type = std::move(type);
    node.handler = element_registry_.resolve(node.type);
    nodes_[id] = std::move(node);
    dirty_nodes_.insert(id);
  }

  void destroy_node(uint64_t id, std::vector<uint64_t>& destroyed) {
    auto it = nodes_.find(id);
    if (it == nodes_.end()) return;
    add_dirty_box(it->second.box);
    const auto children = it->second.children;
    const uint64_t parent = it->second.parent;
    for (uint64_t child : children) destroy_node(child, destroyed);
    if (parent) {
      auto parent_it = nodes_.find(parent);
      if (parent_it != nodes_.end()) {
        invalidate_layout(parent);
        auto& siblings = parent_it->second.children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), id), siblings.end());
      }
    }
    destroyed.push_back(id);
    nodes_.erase(it);
    scrolling_nodes_.erase(id);
    if (focused_id_ == id) focused_id_ = 0;
    if (hovered_id_ == id) hovered_id_ = 0;
    if (root_id_ == id) root_id_ = 0;
    dirty_ = true;
  }

  void append_child(uint64_t parent, uint64_t child) {
    detach(child);
    auto p = nodes_.find(parent);
    auto c = nodes_.find(child);
    if (p == nodes_.end() || c == nodes_.end()) return;
    p->second.children.push_back(child);
    c->second.parent = parent;
    invalidate_layout(parent);
  }

  void remove_child(uint64_t parent, uint64_t child) {
    auto p = nodes_.find(parent);
    if (p != nodes_.end()) {
      auto& children = p->second.children;
      children.erase(std::remove(children.begin(), children.end(), child), children.end());
    }
    auto c = nodes_.find(child);
    if (c != nodes_.end() && c->second.parent == parent) c->second.parent = 0;
    invalidate_layout(parent);
  }

  void insert_before(uint64_t parent, uint64_t child, uint64_t before) {
    detach(child);
    auto p = nodes_.find(parent);
    auto c = nodes_.find(child);
    if (p == nodes_.end() || c == nodes_.end()) return;
    auto& children = p->second.children;
    auto where = std::find(children.begin(), children.end(), before);
    children.insert(where, child);
    c->second.parent = parent;
    invalidate_layout(parent);
  }

  Node* node(uint64_t id) {
    auto it = nodes_.find(id);
    return it == nodes_.end() ? nullptr : &it->second;
  }

  void set_root(uint64_t id) {
    root_id_ = id;
    force_full_repaint_ = true;
    dirty_nodes_.insert(id);
  }
  void set_style(uint64_t id, Style style) {
    Node* target = node(id);
    if (!target) return;
    if (target->style.same_layout(style) && target->style.same_visual(style)) return;
    const bool layout_changed = !target->style.same_layout(style);
    if (layout_changed) invalidate_layout(target->parent ? target->parent : id);
    else invalidate_paint(id);
    target->style = std::move(style);
  }
  void set_text(uint64_t id, std::string text) {
    Node* target = node(id);
    if (!target || target->text == text) return;
    invalidate_layout(target->parent ? target->parent : id);
    target->text = std::move(text);
  }
  void set_custom_prop(uint64_t id, const std::string& name, PropValue value) {
    Node* target = node(id);
    if (!target) return;
    invalidate_layout(id);
    if (name == "itemHeight" || name == "estimatedItemHeight") {
      if (const double* number = std::get_if<double>(&value)) {
        target->item_height = std::max(1.0, *number);
      } else {
        target->item_height = 28.0;
      }
    } else if (name == "overdraw") {
      if (const double* number = std::get_if<double>(&value)) {
        target->overdraw = static_cast<uint32_t>(std::max(0.0, *number));
      } else {
        target->overdraw = 2u;
      }
    } else if (name == "autoFocus" && std::get_if<bool>(&value) && std::get<bool>(value)) {
      focused_id_ = id;
      if (!headless_) SDL_StartTextInput();
    }
    if (std::holds_alternative<std::monostate>(value)) {
      target->props.erase(name);
    } else {
      target->props.insert_or_assign(name, std::move(value));
    }
  }
  void set_event(uint64_t id, const std::string& kind, bool enabled) {
    Node* target = node(id);
    if (!target) return;
    if (enabled) target->events.insert(kind);
    else target->events.erase(kind);
  }
  void focus_element(napi_env env, napi_ref callback_ref, uint64_t id) {
    Node* target = node(id);
    if (id && (!target || bool_prop(*target, "disabled") ||
               number_prop(*target, "tabIndex", target->type == "button" ||
                                                   target->handler->kind == ElementHandler::Kind::kInput ? 0.0 : -1.0) < 0.0)) {
      return;
    }
    if (focused_id_ == id) return;
    const uint64_t previous = focused_id_;
    focused_id_ = id;
    if (previous) emit_to(env, callback_ref, previous, "blur");
    if (focused_id_) emit_to(env, callback_ref, focused_id_, "focus");
    if (!headless_) {
      Node* focused = node(focused_id_);
      if (focused && focused->handler->kind == ElementHandler::Kind::kInput) SDL_StartTextInput();
      else SDL_StopTextInput();
    }
    if (previous) invalidate_paint(previous);
    if (focused_id_) invalidate_paint(focused_id_);
    dirty_ = true;
  }
  void dispatch_pointer(napi_env env, napi_ref callback_ref, const std::string& kind,
                        double x, double y, int button) {
    if (kind == "mouseMove") {
      update_hover(env, callback_ref, x, y);
      return;
    }
    if (kind == "click") {
      emit_pointer(env, callback_ref, "click", x, y, button);
      return;
    }
    emit_pointer(env, callback_ref, kind, x, y, button);
    if (kind == "mouseDown") {
      emit_outside(env, callback_ref, x, y);
      focus_element(env, callback_ref, hit_test_focusable(root_id_, x, y));
    } else if (kind == "mouseUp") {
      emit_pointer(env, callback_ref, "click", x, y, button);
    }
  }
  void dispatch_key(napi_env env, napi_ref callback_ref, const std::string& key) {
    if (!focused_id_) return;
    const uint64_t target_id = focused_id_;
    emit_to(env, callback_ref, target_id, "keyDown", {}, key);
    if (focused_id_ != target_id) return;
    if (key == "Enter" || key == "Space") {
      Node* target = node(target_id);
      if (target && target->handler->kind != ElementHandler::Kind::kInput &&
          !bool_prop(*target, "disabled")) {
        emit_to(env, callback_ref, focused_id_, "click");
      }
    }
  }
  void scroll_to_item(uint64_t id, size_t index) {
    Node* target = node(id);
    if (!target || target->handler->kind != ElementHandler::Kind::kVirtualList) return;
    const double max_scroll = std::max(0.0, target->content_height - target->box.h);
    target->scroll_target_y = std::clamp(index * target->item_height, 0.0, max_scroll);
    scrolling_nodes_.insert(id);
  }
  Box element_box(uint64_t id) const {
    auto it = nodes_.find(id);
    return it == nodes_.end() ? Box{} : it->second.box;
  }
  BLResult capture_screenshot(const std::string& path) const {
    return framebuffer_.write_to_file(path.c_str());
  }
  void record_mutations(size_t count) { mutations_last_commit_ = count; }
  void mark_dirty() { dirty_ = true; }
  size_t node_count() const { return nodes_.size(); }
  int width() const { return width_; }
  int height() const { return height_; }
  uint64_t frame_count() const { return frame_count_; }
  double render_time_ms() const { return render_time_ms_; }
  double layout_time_ms() const { return layout_time_ms_; }
  double paint_time_ms() const { return paint_time_ms_; }
  double present_time_ms() const { return present_time_ms_; }
  uint64_t painted_pixels() const { return painted_pixels_; }
  uint64_t painted_nodes() const { return painted_nodes_; }
  size_t dirty_rect_count() const { return last_dirty_rect_count_; }
  size_t mutations_last_commit() const { return mutations_last_commit_; }
  double frame_percentile(double percentile) const {
    if (frame_samples_.empty()) return 0.0;
    std::vector<double> sorted = frame_samples_;
    std::sort(sorted.begin(), sorted.end());
    const size_t index = std::min(
        sorted.size() - 1,
        static_cast<size_t>(std::floor((sorted.size() - 1) * percentile)));
    return sorted[index];
  }
  uint32_t threads() const { return threads_; }

  bool poll(napi_env env, napi_ref event_callback) {
    if (!running_) return false;
    const auto now = std::chrono::steady_clock::now();
    const double elapsed = std::clamp(
        std::chrono::duration<double>(now - last_poll_at_).count(), 0.0, 0.05);
    last_poll_at_ = now;
    if (!headless_) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
          running_ = false;
        } else if (event.type == SDL_WINDOWEVENT &&
                   event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
          std::string error;
          if (!resize_framebuffer(event.window.data1, event.window.data2, error)) {
            napi_throw_error(env, nullptr, error.c_str());
            return false;
          }
        } else if (event.type == SDL_WINDOWEVENT &&
                   event.window.event == SDL_WINDOWEVENT_EXPOSED) {
          dirty_ = true;
        } else if (event.type == SDL_MOUSEBUTTONDOWN ||
                   event.type == SDL_MOUSEBUTTONUP) {
          const char* kind = event.type == SDL_MOUSEBUTTONDOWN ? "mouseDown" : "mouseUp";
          emit_pointer(env, event_callback, kind, event.button.x, event.button.y,
                       event.button.button);
          if (event.type == SDL_MOUSEBUTTONDOWN) {
            emit_outside(env, event_callback, event.button.x, event.button.y);
            const uint64_t focus_id = hit_test_focusable(root_id_, event.button.x, event.button.y);
            focus_element(env, event_callback, focus_id);
          }
          if (event.type == SDL_MOUSEBUTTONUP) {
            emit_pointer(env, event_callback, "click", event.button.x, event.button.y,
                         event.button.button);
          }
        } else if (event.type == SDL_MOUSEMOTION) {
          update_hover(env, event_callback, event.motion.x, event.motion.y);
        } else if (event.type == SDL_MOUSEWHEEL) {
          int mouse_x = 0;
          int mouse_y = 0;
          SDL_GetMouseState(&mouse_x, &mouse_y);
          double wheel_y = static_cast<double>(event.wheel.preciseY);
          if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) wheel_y = -wheel_y;
          const double delta_y = -wheel_y * 120.0;
          const uint64_t target_id = find_scroll_target(root_id_, mouse_x, mouse_y);
          if (Node* target = node(target_id)) {
            target->scroll_target_y = std::clamp(
                target->scroll_target_y + delta_y, 0.0,
                std::max(0.0, target->content_height - target->box.h));
            scrolling_nodes_.insert(target_id);
            emit_pointer(env, event_callback, "scroll", mouse_x, mouse_y, 0, delta_y);
          }
        } else if (event.type == SDL_TEXTINPUT && focused_id_) {
          if (Node* target = node(focused_id_); target && !bool_prop(*target, "readOnly")) {
            std::string value = string_prop(*target, "value");
            value += event.text.text;
            target->props.insert_or_assign("value", value);
            invalidate_paint(focused_id_);
            emit_to(env, event_callback, focused_id_, "change", value);
          }
        } else if (event.type == SDL_KEYDOWN && focused_id_) {
          const uint64_t key_target_id = focused_id_;
          Node* target = node(key_target_id);
          if (!target) continue;
          const SDL_Keycode code = event.key.keysym.sym;
          std::string key = SDL_GetKeyName(code);
          if (code == SDLK_RETURN) key = "Enter";
          else if (code == SDLK_BACKSPACE) key = "Backspace";
          else if (code == SDLK_ESCAPE) key = "Escape";
          else if (code == SDLK_SPACE) key = "Space";
          else if (code == SDLK_UP) key = "ArrowUp";
          else if (code == SDLK_DOWN) key = "ArrowDown";
          else if (code == SDLK_TAB) key = "Tab";
          emit_to(env, event_callback, key_target_id, "keyDown", {}, key);
          if (focused_id_ != key_target_id || !(target = node(key_target_id))) continue;
          if (code == SDLK_TAB) {
            focus_next(env, event_callback, (event.key.keysym.mod & KMOD_SHIFT) ? -1 : 1);
            continue;
          }
          if ((code == SDLK_RETURN || code == SDLK_SPACE) &&
              target->handler->kind != ElementHandler::Kind::kInput &&
              !bool_prop(*target, "disabled")) {
            emit_to(env, event_callback, focused_id_, "click");
          }
          if (bool_prop(*target, "readOnly")) continue;
          std::string value = string_prop(*target, "value");
          if (code == SDLK_BACKSPACE && !value.empty()) {
            size_t start = value.size() - 1;
            while (start > 0 && (static_cast<unsigned char>(value[start]) & 0xC0u) == 0x80u) --start;
            value.erase(start);
            target->props.insert_or_assign("value", value);
            invalidate_paint(focused_id_);
            emit_to(env, event_callback, focused_id_, "change", value, key);
          } else if (code == SDLK_RETURN) {
            const bool multiline = target->type == "textarea";
            const bool submit = !multiline || (event.key.keysym.mod & KMOD_CTRL) != 0;
            if (submit) {
              emit_to(env, event_callback, focused_id_, "submit", value, key);
            } else {
              value.push_back('\n');
              target->props.insert_or_assign("value", value);
              invalidate_paint(focused_id_);
              emit_to(env, event_callback, focused_id_, "change", value, key);
            }
          }
        } else if (event.type == SDL_KEYUP && focused_id_) {
          const SDL_Keycode code = event.key.keysym.sym;
          std::string key = SDL_GetKeyName(code);
          if (code == SDLK_RETURN) key = "Enter";
          else if (code == SDLK_ESCAPE) key = "Escape";
          else if (code == SDLK_SPACE) key = "Space";
          else if (code == SDLK_UP) key = "ArrowUp";
          else if (code == SDLK_DOWN) key = "ArrowDown";
          else if (code == SDLK_TAB) key = "Tab";
          emit_to(env, event_callback, focused_id_, "keyUp", {}, key);
        }
      }
    }
    const double scroll_blend = 1.0 - std::exp(-18.0 * elapsed);
    for (auto active = scrolling_nodes_.begin(); active != scrolling_nodes_.end();) {
      const uint64_t id = *active;
      Node* target = node(id);
      if (!target) {
        active = scrolling_nodes_.erase(active);
        continue;
      }
      const double remaining = target->scroll_target_y - target->scroll_y;
      if (std::abs(remaining) < 0.05) {
        target->scroll_y = target->scroll_target_y;
        active = scrolling_nodes_.erase(active);
        continue;
      }
      add_dirty_box(target->box);
      target->scroll_y += remaining * scroll_blend;
      dirty_nodes_.insert(id);
      dirty_ = true;
      ++active;
    }
    if (dirty_) render_frame(env);
    return running_;
  }

  void render_frame(napi_env env) {
    if (!running_ || !root_id_) return;
    const auto started = std::chrono::steady_clock::now();

    const auto layout_started = std::chrono::steady_clock::now();
    layout_node(root_id_, 0.0, 0.0, static_cast<double>(width_),
                static_cast<double>(height_), width_, height_);
    for (uint64_t id : dirty_nodes_) {
      if (Node* target = node(id)) add_dirty_box(target->box);
    }
    layout_time_ms_ = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - layout_started)
                          .count();

    if (force_full_repaint_ || dirty_regions_.empty()) {
      dirty_regions_.clear();
      dirty_regions_.emplace_back(0, 0, width_, height_);
    }

    BLContextCreateInfo create_info{};
    create_info.thread_count = threads_;
    create_info.flags = BL_CONTEXT_CREATE_FLAG_FALLBACK_TO_SYNC;
    BLContext context(framebuffer_, create_info);
    if (!context) {
      napi_throw_error(env, nullptr, "Could not create Blend2D context");
      return;
    }
    const auto paint_started = std::chrono::steady_clock::now();
    painted_pixels_ = 0;
    painted_nodes_ = 0;
    for (const BLRectI& region : dirty_regions_) {
      painted_pixels_ += static_cast<uint64_t>(region.w) * static_cast<uint64_t>(region.h);
      context.save();
      context.clip_to_rect(region);
      context.fill_rect(region, BLRgba32(0xFF11131Au));
      paint_node(context, root_id_, 0xFFE8EAF0u, 16.0, &region);
      context.restore();
    }
    context.end();
    paint_time_ms_ = std::chrono::duration<double, std::milli>(
                         std::chrono::steady_clock::now() - paint_started)
                         .count();

    const auto present_started = std::chrono::steady_clock::now();
    if (!headless_) present(env, dirty_regions_);
    present_time_ms_ = std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now() - present_started)
                           .count();
    last_dirty_rect_count_ = dirty_regions_.size();
    dirty_regions_.clear();
    dirty_nodes_.clear();
    force_full_repaint_ = false;
    dirty_ = false;
    ++frame_count_;
    render_time_ms_ = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - started)
                          .count();
    frame_samples_.push_back(render_time_ms_);
    if (frame_samples_.size() > 240) frame_samples_.erase(frame_samples_.begin());
  }

 private:
  static bool boxes_touch(const BLRectI& a, const BLRectI& b) {
    return a.x <= b.x + b.w && b.x <= a.x + a.w &&
           a.y <= b.y + b.h && b.y <= a.y + a.h;
  }

  void add_dirty_box(const Box& box) {
    if (box.w <= 0.0 || box.h <= 0.0) return;
    int x0 = std::max(0, static_cast<int>(std::floor(box.x)) - 1);
    int y0 = std::max(0, static_cast<int>(std::floor(box.y)) - 1);
    int x1 = std::min(width_, static_cast<int>(std::ceil(box.x + box.w)) + 1);
    int y1 = std::min(height_, static_cast<int>(std::ceil(box.y + box.h)) + 1);
    if (x1 <= x0 || y1 <= y0) return;
    BLRectI incoming(x0, y0, x1 - x0, y1 - y0);
    for (size_t i = 0; i < dirty_regions_.size();) {
      if (!boxes_touch(incoming, dirty_regions_[i])) {
        ++i;
        continue;
      }
      const BLRectI current = dirty_regions_[i];
      const int nx0 = std::min(incoming.x, current.x);
      const int ny0 = std::min(incoming.y, current.y);
      const int nx1 = std::max(incoming.x + incoming.w, current.x + current.w);
      const int ny1 = std::max(incoming.y + incoming.h, current.y + current.h);
      incoming = BLRectI(nx0, ny0, nx1 - nx0, ny1 - ny0);
      dirty_regions_.erase(dirty_regions_.begin() + static_cast<std::ptrdiff_t>(i));
    }
    dirty_regions_.push_back(incoming);
    if (dirty_regions_.size() > 64) force_full_repaint_ = true;
  }

  void invalidate_paint(uint64_t id) {
    Node* target = node(id);
    if (target) add_dirty_box(target->box);
    dirty_nodes_.insert(id);
    dirty_ = true;
  }

  void invalidate_layout(uint64_t id) {
    Node* target = node(id);
    if (target) add_dirty_box(target->box);
    dirty_nodes_.insert(id);
    dirty_ = true;
  }

  void detach(uint64_t child) {
    auto c = nodes_.find(child);
    if (c == nodes_.end() || !c->second.parent) return;
    auto p = nodes_.find(c->second.parent);
    if (p != nodes_.end()) {
      auto& children = p->second.children;
      children.erase(std::remove(children.begin(), children.end(), child), children.end());
    }
    c->second.parent = 0;
  }

  BLFont make_font(double size) const {
    BLFont font;
    font.create_from_face(font_face_, static_cast<float>(size));
    return font;
  }

  Size measure_text(const std::string& text, double size) const {
    BLFont font = make_font(size);
    BLGlyphBuffer glyphs;
    glyphs.set_utf8_text(text.data(), text.size());
    font.shape(glyphs);
    BLTextMetrics metrics{};
    font.get_text_metrics(glyphs, metrics);
    const auto& fm = font.metrics();
    return {std::ceil(metrics.advance.x), std::ceil(fm.ascent + fm.descent + fm.line_gap)};
  }

  const BLImage* image_for(const std::string& path) const {
    if (path.empty()) return nullptr;
    auto found = image_cache_.find(path);
    if (found != image_cache_.end()) return found->second.is_empty() ? nullptr : &found->second;
    BLImage image;
    image.read_from_file(path.c_str());
    auto [it, _] = image_cache_.emplace(path, std::move(image));
    return it->second.is_empty() ? nullptr : &it->second;
  }

  static size_t line_count(const std::string& text) {
    return text.empty() ? 1u : 1u + static_cast<size_t>(std::count(text.begin(), text.end(), '\n'));
  }

  std::string svg_source_for(const std::string& source_or_path) const {
    if (source_or_path.empty() || source_or_path.front() == '<') return source_or_path;
    auto found = svg_cache_.find(source_or_path);
    if (found != svg_cache_.end()) return found->second;
    std::ifstream input(source_or_path);
    std::ostringstream contents;
    contents << input.rdbuf();
    return svg_cache_.emplace(source_or_path, contents.str()).first->second;
  }

  static std::string xml_attribute(const std::string& tag, const char* name) {
    const std::string needle = std::string(name) + "=";
    size_t at = tag.find(needle);
    if (at == std::string::npos) return {};
    at += needle.size();
    if (at >= tag.size() || (tag[at] != '\"' && tag[at] != '\'')) return {};
    const char quote = tag[at++];
    const size_t end = tag.find(quote, at);
    return end == std::string::npos ? std::string{} : tag.substr(at, end - at);
  }

  static double xml_number(const std::string& tag, const char* name, double fallback = 0.0) {
    const std::string value = xml_attribute(tag, name);
    if (value.empty()) return fallback;
    char* end = nullptr;
    const double result = std::strtod(value.c_str(), &end);
    return end == value.c_str() ? fallback : result;
  }

  static BLPath svg_path(const std::string& data) {
    BLPath path;
    const char* cursor = data.c_str();
    char command = 0;
    double current_x = 0.0;
    double current_y = 0.0;
    double start_x = 0.0;
    double start_y = 0.0;
    auto skip = [&]() {
      while (*cursor && (std::isspace(static_cast<unsigned char>(*cursor)) || *cursor == ',')) ++cursor;
    };
    auto read = [&](double& value) {
      skip();
      char* end = nullptr;
      value = std::strtod(cursor, &end);
      if (end == cursor) return false;
      cursor = end;
      return true;
    };
    while (*cursor) {
      skip();
      if (!*cursor) break;
      if (std::isalpha(static_cast<unsigned char>(*cursor))) command = *cursor++;
      if (!command) { ++cursor; continue; }
      const bool relative = std::islower(static_cast<unsigned char>(command));
      const char op = static_cast<char>(std::toupper(static_cast<unsigned char>(command)));
      if (op == 'Z') {
        path.close(); current_x = start_x; current_y = start_y; command = 0; continue;
      }
      double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0;
      if (op == 'M' || op == 'L' || op == 'T') {
        if (!read(a) || !read(b)) { command = 0; continue; }
        if (relative) { a += current_x; b += current_y; }
        if (op == 'M') {
          path.move_to(a, b); start_x = a; start_y = b;
          command = relative ? 'l' : 'L';
        } else if (op == 'T') path.smooth_quad_to(a, b);
        else path.line_to(a, b);
        current_x = a; current_y = b;
      } else if (op == 'H') {
        if (!read(a)) { command = 0; continue; }
        if (relative) a += current_x;
        path.line_to(a, current_y); current_x = a;
      } else if (op == 'V') {
        if (!read(a)) { command = 0; continue; }
        if (relative) a += current_y;
        path.line_to(current_x, a); current_y = a;
      } else if (op == 'C') {
        if (!read(a) || !read(b) || !read(c) || !read(d) || !read(e) || !read(f)) { command = 0; continue; }
        if (relative) { a += current_x; b += current_y; c += current_x; d += current_y; e += current_x; f += current_y; }
        path.cubic_to(a, b, c, d, e, f); current_x = e; current_y = f;
      } else if (op == 'S') {
        if (!read(a) || !read(b) || !read(c) || !read(d)) { command = 0; continue; }
        if (relative) { a += current_x; b += current_y; c += current_x; d += current_y; }
        path.smooth_cubic_to(a, b, c, d); current_x = c; current_y = d;
      } else if (op == 'Q') {
        if (!read(a) || !read(b) || !read(c) || !read(d)) { command = 0; continue; }
        if (relative) { a += current_x; b += current_y; c += current_x; d += current_y; }
        path.quad_to(a, b, c, d); current_x = c; current_y = d;
      } else if (op == 'A') {
        if (!read(a) || !read(b) || !read(c) || !read(d) || !read(e) || !read(f) || !read(g)) { command = 0; continue; }
        if (relative) { f += current_x; g += current_y; }
        path.elliptic_arc_to(a, b, c * 3.14159265358979323846 / 180.0, d != 0.0, e != 0.0, f, g);
        current_x = f; current_y = g;
      } else {
        ++cursor; command = 0;
      }
    }
    return path;
  }

  void paint_svg(BLContext& context, const Node& n, uint32_t color) const {
    const std::string source = svg_source_for(string_prop(n, "src"));
    if (source.empty()) return;
    std::string svg_tag;
    const size_t svg_start = source.find("<svg");
    if (svg_start != std::string::npos) {
      const size_t svg_end = source.find('>', svg_start);
      svg_tag = source.substr(svg_start, svg_end - svg_start + 1);
    }
    double view_x = 0.0, view_y = 0.0, view_w = 24.0, view_h = 24.0;
    const std::string view_box = xml_attribute(svg_tag, "viewBox");
    if (!view_box.empty()) {
      std::istringstream values(view_box);
      values >> view_x >> view_y >> view_w >> view_h;
    }
    const double scale = std::min(n.box.w / std::max(1.0, view_w), n.box.h / std::max(1.0, view_h));
    context.save();
    context.translate(n.box.x + (n.box.w - view_w * scale) * 0.5 - view_x * scale,
                      n.box.y + (n.box.h - view_h * scale) * 0.5 - view_y * scale);
    context.scale(scale);
    context.set_stroke_width(xml_number(svg_tag, "stroke-width", 2.0));
    const bool fill_none = xml_attribute(svg_tag, "fill") == "none";
    const bool has_stroke = !xml_attribute(svg_tag, "stroke").empty();
    auto render_path = [&](const BLPath& path, const std::string& tag) {
      const std::string tag_fill = xml_attribute(tag, "fill");
      const bool fill = tag_fill.empty() ? !fill_none : tag_fill != "none";
      const std::string tag_stroke = xml_attribute(tag, "stroke");
      const bool stroke = tag_stroke.empty() ? has_stroke : tag_stroke != "none";
      if (fill) context.fill_path(path, BLRgba32(color));
      if (stroke) context.stroke_path(path, BLRgba32(color));
    };
    size_t at = 0;
    while ((at = source.find('<', at)) != std::string::npos) {
      const size_t end = source.find('>', at);
      if (end == std::string::npos) break;
      const std::string tag = source.substr(at, end - at + 1);
      if (tag.rfind("<path", 0) == 0) {
        render_path(svg_path(xml_attribute(tag, "d")), tag);
      } else if (tag.rfind("<line", 0) == 0) {
        context.stroke_line(xml_number(tag, "x1"), xml_number(tag, "y1"),
                            xml_number(tag, "x2"), xml_number(tag, "y2"), BLRgba32(color));
      } else if (tag.rfind("<circle", 0) == 0) {
        const double cx = xml_number(tag, "cx"), cy = xml_number(tag, "cy"), r = xml_number(tag, "r");
        if (fill_none) context.stroke_circle(cx, cy, r, BLRgba32(color));
        else context.fill_circle(cx, cy, r, BLRgba32(color));
      } else if (tag.rfind("<rect", 0) == 0) {
        const BLRect rect(xml_number(tag, "x"), xml_number(tag, "y"), xml_number(tag, "width"), xml_number(tag, "height"));
        const double radius = xml_number(tag, "rx");
        if (fill_none) context.stroke_round_rect(rect, radius, radius, BLRgba32(color));
        else context.fill_round_rect(rect, radius, radius, BLRgba32(color));
      } else if (tag.rfind("<polyline", 0) == 0 || tag.rfind("<polygon", 0) == 0) {
        std::string points = xml_attribute(tag, "points");
        std::replace(points.begin(), points.end(), ',', ' ');
        std::istringstream values(points);
        BLPath path;
        double x = 0.0, y = 0.0;
        if (values >> x >> y) path.move_to(x, y);
        while (values >> x >> y) path.line_to(x, y);
        if (tag.rfind("<polygon", 0) == 0) path.close();
        render_path(path, tag);
      }
      at = end + 1;
    }
    context.restore();
  }

  Size natural_size(uint64_t id, double available_width) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end()) return {};
    const Node& n = it->second;
    if (n.handler->kind == ElementHandler::Kind::kText) {
      Size measured = measure_text(n.text, n.style.font_size);
      if (n.style.line_height > 0.0) measured.h = n.style.line_height;
      measured.w = std::max(measured.w, n.style.min_width);
      measured.h = std::max(measured.h, n.style.min_height);
      return measured;
    }

    if (n.handler->kind == ElementHandler::Kind::kVirtualList) {
      const double natural_h = n.children.size() * n.item_height +
                               n.style.padding_top + n.style.padding_bottom;
      return {
          std::max(n.style.min_width, n.style.width.resolve(available_width, available_width)),
          std::max(n.style.min_height, n.style.height.resolve(natural_h, natural_h)),
      };
    }

    if (n.handler->kind == ElementHandler::Kind::kImage) {
      Size result{100.0, 100.0};
      if (const BLImage* image = image_for(string_prop(n, "src"))) {
        result = {static_cast<double>(image->width()), static_cast<double>(image->height())};
      }
      result.w = n.style.width.resolve(available_width, result.w);
      result.h = n.style.height.resolve(result.h, result.h);
      return result;
    }
    if (n.handler->kind == ElementHandler::Kind::kSvg) {
      return {n.style.width.resolve(available_width, 24.0), n.style.height.resolve(24.0, 24.0)};
    }
    if (n.handler->kind == ElementHandler::Kind::kCanvas) {
      return {n.style.width.resolve(available_width, 300.0), n.style.height.resolve(150.0, 150.0)};
    }
    if (n.handler->kind == ElementHandler::Kind::kSeparator) {
      return {n.style.width.resolve(available_width, available_width), n.style.height.resolve(1.0, 1.0)};
    }
    if (n.handler->kind == ElementHandler::Kind::kProgress) {
      return {n.style.width.resolve(available_width, 120.0), n.style.height.resolve(8.0, 8.0)};
    }
    if (n.handler->kind == ElementHandler::Kind::kInput) {
      const double rows = std::max(1.0, number_prop(n, "minRows", n.type == "textarea" ? 3.0 : 1.0));
      const double line = n.style.line_height > 0.0 ? n.style.line_height : n.style.font_size * 1.35;
      return {n.style.width.resolve(available_width, std::min(available_width, 320.0)),
              n.style.height.resolve(rows * line + n.style.padding_top + n.style.padding_bottom,
                                     rows * line + n.style.padding_top + n.style.padding_bottom)};
    }
    if (n.handler->kind == ElementHandler::Kind::kMarkdown ||
        n.handler->kind == ElementHandler::Kind::kCode ||
        n.handler->kind == ElementHandler::Kind::kDiff) {
      const char* prop = n.handler->kind == ElementHandler::Kind::kMarkdown ? "source" :
                         n.handler->kind == ElementHandler::Kind::kCode ? "code" : "patch";
      const std::string content = string_prop(n, prop);
      const double line = n.style.line_height > 0.0 ? n.style.line_height : n.style.font_size * 1.45;
      return {n.style.width.resolve(available_width, available_width),
              n.style.height.resolve(line_count(content) * line + n.style.padding_top + n.style.padding_bottom,
                                     line_count(content) * line + n.style.padding_top + n.style.padding_bottom)};
    }

    const double provisional_w = n.style.width.resolve(available_width, available_width);
    const double inner_w = std::max(0.0, provisional_w - n.style.padding_left -
                                             n.style.padding_right);
    double main = 0.0;
    double cross = 0.0;
    size_t flow_count = 0;
    for (size_t i = 0; i < n.children.size(); ++i) {
      auto child_it = nodes_.find(n.children[i]);
      if (child_it == nodes_.end() ||
          child_it->second.style.position != Style::Position::kRelative ||
          child_it->second.handler->kind == ElementHandler::Kind::kAnchored) {
        continue;
      }
      const Node& child_node = child_it->second;
      const Size child = natural_size(n.children[i], inner_w);
      if (n.style.row) {
        main += child.w + child_node.style.margin_left + child_node.style.margin_right;
        cross = std::max(cross, child.h + child_node.style.margin_top +
                                    child_node.style.margin_bottom);
      } else {
        main += child.h + child_node.style.margin_top + child_node.style.margin_bottom;
        cross = std::max(cross, child.w + child_node.style.margin_left +
                                    child_node.style.margin_right);
      }
      ++flow_count;
    }
    if (flow_count > 1) main += n.style.gap * (flow_count - 1);
    Size result = n.style.row
                      ? Size{main + n.style.padding_left + n.style.padding_right,
                             cross + n.style.padding_top + n.style.padding_bottom}
                      : Size{cross + n.style.padding_left + n.style.padding_right,
                             main + n.style.padding_top + n.style.padding_bottom};
    result.w = n.style.width.resolve(available_width, result.w);
    result.h = n.style.height.resolve(0.0, result.h);
    result.w = std::max(result.w, n.style.min_width);
    result.h = std::max(result.h, n.style.min_height);
    if (n.style.max_width.set) result.w = std::min(result.w, n.style.max_width.resolve(available_width, result.w));
    if (n.style.max_height.set) result.h = std::min(result.h, n.style.max_height.resolve(result.h, result.h));
    return result;
  }

  void layout_node(uint64_t id, double x, double y, double available_w,
                   double available_h, double forced_w = -1.0,
                   double forced_h = -1.0) {
    Node* n = node(id);
    if (!n) return;
    const Size natural = natural_size(id, available_w);
    // The parent has already resolved basis, grow/shrink, percentages, and
    // stretching into a forced size. Applying the child's explicit width a
    // second time would discard that flex result (notably width: 0 + grow).
    double w = std::max(n->style.min_width,
                        forced_w >= 0.0 ? forced_w
                                        : n->style.width.resolve(available_w, natural.w));
    double h = std::max(n->style.min_height,
                        forced_h >= 0.0 ? forced_h
                                        : n->style.height.resolve(available_h, natural.h));
    if (n->style.max_width.set) w = std::min(w, n->style.max_width.resolve(available_w, w));
    if (n->style.max_height.set) h = std::min(h, n->style.max_height.resolve(available_h, h));
    n->box = {x, y, std::max(0.0, w), std::max(0.0, h)};
    if (n->handler->kind == ElementHandler::Kind::kText || n->children.empty()) return;

    const double inner_x = x + n->style.padding_left;
    const double inner_y = y + n->style.padding_top;
    const double inner_w = std::max(0.0, w - n->style.padding_left - n->style.padding_right);
    const double inner_h = std::max(0.0, h - n->style.padding_top - n->style.padding_bottom);
    if (n->handler->kind == ElementHandler::Kind::kVirtualList) {
      n->content_height = n->children.size() * n->item_height +
                          n->style.padding_top + n->style.padding_bottom;
      const double max_scroll = std::max(0.0, n->content_height - h);
      if (bool_prop(*n, "followTail") && n->children.size() != n->last_child_count) {
        n->scroll_y = max_scroll;
        n->scroll_target_y = max_scroll;
      }
      n->last_child_count = n->children.size();
      n->scroll_y = std::clamp(n->scroll_y, 0.0, max_scroll);
      n->scroll_target_y = std::clamp(n->scroll_target_y, 0.0, max_scroll);
      const size_t first = static_cast<size_t>(std::floor(n->scroll_y / n->item_height));
      const size_t count = static_cast<size_t>(std::ceil(inner_h / n->item_height));
      n->visible_start = first > n->overdraw ? first - n->overdraw : 0;
      n->visible_end = std::min(n->children.size(), first + count + n->overdraw + 1);
      for (size_t i = n->visible_start; i < n->visible_end; ++i) {
        const double bottom_alignment = string_prop(*n, "alignment") == "bottom"
                                            ? std::max(0.0, inner_h - n->children.size() * n->item_height)
                                            : 0.0;
        const double child_y = inner_y + bottom_alignment + i * n->item_height - n->scroll_y;
        layout_node(n->children[i], inner_x, child_y, inner_w, n->item_height,
                    inner_w, n->item_height);
      }
      return;
    }
    const double main_available = n->style.row ? inner_w : inner_h;
    size_t flow_count = 0;
    double occupied = 0.0;
    double total_grow = 0.0;
    double total_shrink = 0.0;
    std::vector<Size> sizes;
    sizes.reserve(n->children.size());
    for (uint64_t child_id : n->children) {
      Node* child = node(child_id);
      Size size = natural_size(child_id, inner_w);
      if (child) {
        size.w = child->style.width.resolve(inner_w, size.w);
        size.h = child->style.height.resolve(inner_h, size.h);
        if (child->style.max_width.set) size.w = std::min(size.w, child->style.max_width.resolve(inner_w, size.w));
        if (child->style.max_height.set) size.h = std::min(size.h, child->style.max_height.resolve(inner_h, size.h));
        const bool in_flow = child->style.position == Style::Position::kRelative &&
                             child->handler->kind != ElementHandler::Kind::kAnchored;
        if (in_flow) {
          total_grow += child->style.flex_grow;
          total_shrink += (n->style.row ? size.w : size.h) * child->style.flex_shrink;
          occupied += n->style.row
                          ? size.w + child->style.margin_left + child->style.margin_right
                          : size.h + child->style.margin_top + child->style.margin_bottom;
          ++flow_count;
        }
      }
      sizes.push_back(size);
    }
    if (flow_count > 1) occupied += n->style.gap * (flow_count - 1);
    if (occupied > main_available && total_shrink > 0.0) {
      const double deficit = occupied - main_available;
      for (size_t i = 0; i < n->children.size(); ++i) {
        Node* child = node(n->children[i]);
        if (!child || child->style.position != Style::Position::kRelative ||
            child->handler->kind == ElementHandler::Kind::kAnchored) continue;
        double& main_size = n->style.row ? sizes[i].w : sizes[i].h;
        const double share = deficit * main_size * child->style.flex_shrink / total_shrink;
        main_size = std::max(0.0, main_size - share);
      }
      occupied = main_available;
    }
    const double extra = std::max(0.0, main_available - occupied);
    n->content_height = n->style.row
                            ? inner_h
                            : occupied + n->style.padding_top + n->style.padding_bottom;
    const double max_scroll = std::max(0.0, n->content_height - h);
    n->scroll_y = std::clamp(n->scroll_y, 0.0, max_scroll);
    n->scroll_target_y = std::clamp(n->scroll_target_y, 0.0, max_scroll);
    double effective_gap = n->style.gap;
    double main_offset = 0.0;
    if (total_grow == 0.0) {
      if (n->style.justify == Style::Justify::kCenter) main_offset = extra * 0.5;
      else if (n->style.justify == Style::Justify::kEnd) main_offset = extra;
      else if (n->style.justify == Style::Justify::kSpaceBetween && flow_count > 1) {
        effective_gap += extra / static_cast<double>(flow_count - 1);
      }
    }
    double cursor = (n->style.row ? inner_x : inner_y - n->scroll_y) + main_offset;
    for (size_t i = 0; i < n->children.size(); ++i) {
      Node* child = node(n->children[i]);
      if (!child) continue;
      double child_w = sizes[i].w;
      double child_h = sizes[i].h;
      const bool anchored = child->handler->kind == ElementHandler::Kind::kAnchored;
      if (child->style.position != Style::Position::kRelative || anchored) {
        const bool fixed = child->style.position == Style::Position::kFixed;
        const double positioning_x = fixed ? 0.0 : inner_x;
        const double positioning_y = fixed ? 0.0 : inner_y;
        const double positioning_w = fixed ? static_cast<double>(width_) : inner_w;
        const double positioning_h = fixed ? static_cast<double>(height_) : inner_h;
        double absolute_x = positioning_x + child->style.left.value_or(0.0);
        double absolute_y = positioning_y + child->style.top.value_or(0.0);
        if (!child->style.width.set && child->style.left && child->style.right) {
          child_w = std::max(0.0, positioning_w - *child->style.left - *child->style.right);
        } else if (!child->style.left && child->style.right) {
          absolute_x = positioning_x + positioning_w - *child->style.right - child_w;
        }
        if (!child->style.height.set && child->style.top && child->style.bottom) {
          child_h = std::max(0.0, positioning_h - *child->style.top - *child->style.bottom);
        } else if (!child->style.top && child->style.bottom) {
          absolute_y = positioning_y + positioning_h - *child->style.bottom - child_h;
        }
        if (anchored) {
          BLPoint anchor_point;
          bool has_anchor = false;
          if (const double* anchor_id = prop_as<double>(*child, "anchorId")) {
            if (Node* anchor_node = node(static_cast<uint64_t>(*anchor_id))) {
              const std::string side = string_prop(*child, "side", "bottom");
              const std::string align = string_prop(*child, "align", "start");
              anchor_point.x = align == "center" ? anchor_node->box.x + anchor_node->box.w * 0.5
                               : align == "end" ? anchor_node->box.x + anchor_node->box.w
                                                : anchor_node->box.x;
              anchor_point.y = side == "top" ? anchor_node->box.y
                               : side == "bottom" ? anchor_node->box.y + anchor_node->box.h
                               : align == "center" ? anchor_node->box.y + anchor_node->box.h * 0.5
                               : align == "end" ? anchor_node->box.y + anchor_node->box.h
                                                : anchor_node->box.y;
              if (side == "left") anchor_point.x = anchor_node->box.x;
              else if (side == "right") anchor_point.x = anchor_node->box.x + anchor_node->box.w;
              has_anchor = true;
            }
          }
          if (!has_anchor) {
            if (const BLPoint* point = prop_as<BLPoint>(*child, "position")) {
              anchor_point = *point;
              has_anchor = true;
            }
          }
          if (has_anchor) {
            absolute_x = anchor_point.x;
            absolute_y = anchor_point.y;
            const std::string side = string_prop(*child, "side", "bottom");
            const std::string align = string_prop(*child, "align", "start");
            const double anchor_gap = number_prop(*child, "anchorGap", 4.0);
            if (side == "top" || side == "bottom") {
              if (align == "center") absolute_x -= child_w * 0.5;
              else if (align == "end") absolute_x -= child_w;
              absolute_y += side == "top" ? -(child_h + anchor_gap) : anchor_gap;
            } else {
              if (align == "center") absolute_y -= child_h * 0.5;
              else if (align == "end") absolute_y -= child_h;
              absolute_x += side == "left" ? -(child_w + anchor_gap) : anchor_gap;
            }
            if (const BLPoint* offset = prop_as<BLPoint>(*child, "offset")) {
              absolute_x += offset->x;
              absolute_y += offset->y;
            }
            absolute_x = std::clamp(absolute_x, 4.0, std::max(4.0, static_cast<double>(width_) - child_w - 4.0));
            absolute_y = std::clamp(absolute_y, 4.0, std::max(4.0, static_cast<double>(height_) - child_h - 4.0));
          }
        }
        layout_node(child->id, absolute_x, absolute_y, positioning_w, positioning_h, child_w, child_h);
        continue;
      }
      if (child->style.flex_grow > 0.0 && total_grow > 0.0) {
        const double share = extra * child->style.flex_grow / total_grow;
        if (n->style.row) child_w += share;
        else child_h += share;
      }
      if (n->style.row) {
        if (!child->style.height.set && n->style.align_items == Style::Align::kStretch) {
          child_h = std::max(0.0, inner_h - child->style.margin_top - child->style.margin_bottom);
        }
        double cross_y = inner_y + child->style.margin_top;
        if (n->style.align_items == Style::Align::kCenter) cross_y = inner_y + (inner_h - child_h) * 0.5;
        else if (n->style.align_items == Style::Align::kEnd) cross_y = inner_y + inner_h - child_h - child->style.margin_bottom;
        cursor += child->style.margin_left;
        layout_node(child->id, cursor, cross_y, inner_w, inner_h, child_w, child_h);
        cursor += child_w + child->style.margin_right + effective_gap;
      } else {
        if (!child->style.width.set && n->style.align_items == Style::Align::kStretch) {
          child_w = std::max(0.0, inner_w - child->style.margin_left - child->style.margin_right);
        }
        double cross_x = inner_x + child->style.margin_left;
        if (n->style.align_items == Style::Align::kCenter) cross_x = inner_x + (inner_w - child_w) * 0.5;
        else if (n->style.align_items == Style::Align::kEnd) cross_x = inner_x + inner_w - child_w - child->style.margin_right;
        cursor += child->style.margin_top;
        layout_node(child->id, cross_x, cursor, inner_w, inner_h, child_w, child_h);
        cursor += child_h + child->style.margin_bottom + effective_gap;
      }
    }
  }

  static bool intersects(const Box& box, const BLRectI& rect) {
    return box.x < rect.x + rect.w && rect.x < box.x + box.w &&
           box.y < rect.y + rect.h && rect.y < box.y + box.h;
  }

  void draw_text(BLContext& context, const std::string& text, double x, double baseline,
                 double size, uint32_t color) const {
    if (text.empty()) return;
    BLFont font = make_font(size);
    context.fill_utf8_text(BLPoint(x, baseline), font, text.data(), text.size(), BLRgba32(color));
  }

  void paint_multiline(BLContext& context, const std::string& content, const Node& n,
                       uint32_t color, double size, bool markdown, bool diff) const {
    const double line_height = n.style.line_height > 0.0 ? n.style.line_height : size * 1.45;
    double y = n.box.y + n.style.padding_top;
    std::istringstream stream(content);
    std::string line;
    size_t line_number = 1;
    while (std::getline(stream, line)) {
      if (y + line_height > n.box.y + n.box.h + 1.0) break;
      uint32_t line_color = color;
      double line_size = size;
      double x = n.box.x + n.style.padding_left;
      if (diff) {
        if (!line.empty() && line.front() == '+' && line.rfind("+++", 0) != 0) {
          context.fill_rect(BLRect(n.box.x, y, n.box.w, line_height), BLRgba32(0x3322C55Eu));
          line_color = 0xFF86EFACu;
        } else if (!line.empty() && line.front() == '-' && line.rfind("---", 0) != 0) {
          context.fill_rect(BLRect(n.box.x, y, n.box.w, line_height), BLRgba32(0x33EF4444u));
          line_color = 0xFFFCA5A5u;
        } else if (line.rfind("@@", 0) == 0) {
          line_color = 0xFF93C5FDu;
        }
      } else if (markdown) {
        if (line.rfind("### ", 0) == 0) { line.erase(0, 4); line_size = size * 1.12; line_color = 0xFFFFFFFFu; }
        else if (line.rfind("## ", 0) == 0) { line.erase(0, 3); line_size = size * 1.25; line_color = 0xFFFFFFFFu; }
        else if (line.rfind("# ", 0) == 0) { line.erase(0, 2); line_size = size * 1.45; line_color = 0xFFFFFFFFu; }
        else if (line.rfind("- ", 0) == 0 || line.rfind("* ", 0) == 0) { line.replace(0, 1, "•"); }
        else if (line.rfind("> ", 0) == 0) { line_color = 0xFF94A3B8u; x += 10.0; }
      } else if (bool_prop(n, "showLineNumbers")) {
        const std::string number_text = std::to_string(line_number);
        draw_text(context, number_text, x, y + line_height * 0.76, size * 0.85, 0xFF64748Bu);
        x += 42.0;
      }
      draw_text(context, line, x, y + line_height * 0.76, line_size, line_color);
      y += line_height;
      ++line_number;
    }
  }

  void paint_special(BLContext& context, const Node& n, uint32_t color,
                     double font_size) const {
    const auto kind = n.handler->kind;
    if (kind == ElementHandler::Kind::kImage) {
      const BLImage* image = image_for(string_prop(n, "src"));
      if (!image) {
        context.fill_rect(BLRect(n.box.x, n.box.y, n.box.w, n.box.h), BLRgba32(0xFF252A35u));
        context.stroke_line(n.box.x, n.box.y, n.box.x + n.box.w, n.box.y + n.box.h,
                            BLRgba32(0xFF64748Bu));
        context.stroke_line(n.box.x + n.box.w, n.box.y, n.box.x, n.box.y + n.box.h,
                            BLRgba32(0xFF64748Bu));
        return;
      }
      const double iw = image->width();
      const double ih = image->height();
      const std::string fit = string_prop(n, "objectFit", "fill");
      BLRect destination(n.box.x, n.box.y, n.box.w, n.box.h);
      if (fit == "contain" || fit == "scaleDown" || fit == "none") {
        double scale = fit == "none" ? 1.0 : std::min(n.box.w / iw, n.box.h / ih);
        if (fit == "scaleDown") scale = std::min(1.0, scale);
        destination.w = iw * scale;
        destination.h = ih * scale;
        destination.x += (n.box.w - destination.w) * 0.5;
        destination.y += (n.box.h - destination.h) * 0.5;
        context.blit_image(destination, *image);
      } else if (fit == "cover") {
        const double target_ratio = n.box.w / std::max(1.0, n.box.h);
        const double source_ratio = iw / std::max(1.0, ih);
        BLRectI source(0, 0, image->width(), image->height());
        if (source_ratio > target_ratio) {
          source.w = std::max(1, static_cast<int>(ih * target_ratio));
          source.x = (image->width() - source.w) / 2;
        } else {
          source.h = std::max(1, static_cast<int>(iw / target_ratio));
          source.y = (image->height() - source.h) / 2;
        }
        context.blit_image(destination, *image, source);
      } else {
        context.blit_image(destination, *image);
      }
      return;
    }
    if (kind == ElementHandler::Kind::kCanvas) {
      const auto* commands = prop_as<std::vector<CanvasCommand>>(n, "commands");
      if (!commands) return;
      for (const CanvasCommand& command : *commands) {
        const double x = n.box.x + command.x;
        const double y = n.box.y + command.y;
        if (command.kind == "fillRect") {
          if (command.radius > 0.0) context.fill_round_rect(BLRect(x, y, command.width, command.height), command.radius, command.radius, BLRgba32(command.color));
          else context.fill_rect(BLRect(x, y, command.width, command.height), BLRgba32(command.color));
        } else if (command.kind == "strokeRect") {
          context.set_stroke_width(command.stroke_width);
          if (command.radius > 0.0) context.stroke_round_rect(BLRect(x, y, command.width, command.height), command.radius, command.radius, BLRgba32(command.color));
          else context.stroke_rect(BLRect(x, y, command.width, command.height), BLRgba32(command.color));
        } else if (command.kind == "line") {
          context.set_stroke_width(command.stroke_width);
          context.stroke_line(x, y, n.box.x + command.x2, n.box.y + command.y2, BLRgba32(command.color));
        } else if (command.kind == "circle") {
          if (command.fill) context.fill_circle(x, y, command.radius, BLRgba32(command.color));
          else { context.set_stroke_width(command.stroke_width); context.stroke_circle(x, y, command.radius, BLRgba32(command.color)); }
        } else if (command.kind == "text") {
          draw_text(context, command.text, x, y, command.font_size, command.color);
        }
      }
      return;
    }
    if (kind == ElementHandler::Kind::kSeparator) {
      const uint32_t separator_color = n.style.color.value_or(0xFF343A46u);
      context.fill_rect(BLRect(n.box.x, n.box.y, n.box.w, std::max(1.0, n.box.h)), BLRgba32(separator_color));
      return;
    }
    if (kind == ElementHandler::Kind::kProgress) {
      const double maximum = std::max(0.0001, number_prop(n, "max", 100.0));
      const double value = std::clamp(number_prop(n, "value", 0.0), 0.0, maximum);
      const double radius = std::min(n.box.h * 0.5, n.style.border_radius > 0.0 ? n.style.border_radius : n.box.h * 0.5);
      context.fill_round_rect(BLRect(n.box.x, n.box.y, n.box.w, n.box.h), radius, radius, BLRgba32(n.style.background.value_or(0xFF273142u)));
      if (value > 0.0) context.fill_round_rect(BLRect(n.box.x, n.box.y, n.box.w * value / maximum, n.box.h), radius, radius, BLRgba32(n.style.color.value_or(0xFF38BDF8u)));
      return;
    }
    if (kind == ElementHandler::Kind::kMarkdown) {
      paint_multiline(context, string_prop(n, "source"), n, color, font_size, true, false);
      return;
    }
    if (kind == ElementHandler::Kind::kCode) {
      double offset = 0.0;
      if (bool_prop(n, "showHeader")) {
        const std::string header = string_prop(n, "language", "code");
        context.fill_rect(BLRect(n.box.x, n.box.y, n.box.w, font_size * 2.0), BLRgba32(0xFF202632u));
        draw_text(context, header, n.box.x + 12.0, n.box.y + font_size * 1.35, font_size * 0.85, 0xFF94A3B8u);
        offset = font_size * 2.0;
      }
      Node shifted = n;
      shifted.box.y += offset;
      shifted.box.h = std::max(0.0, shifted.box.h - offset);
      paint_multiline(context, string_prop(n, "code"), shifted, color, font_size, false, false);
      return;
    }
    if (kind == ElementHandler::Kind::kDiff) {
      paint_multiline(context, string_prop(n, "patch"), n, color, font_size, false, true);
      return;
    }
    if (kind == ElementHandler::Kind::kInput) {
      const std::string value = string_prop(n, "value");
      const std::string display = value.empty() ? string_prop(n, "placeholder") : value;
      const uint32_t display_color = value.empty() ? 0xFF64748Bu : color;
      const double line = n.style.line_height > 0.0 ? n.style.line_height : font_size * 1.35;
      std::istringstream stream(display);
      std::string text;
      const bool multiline = n.type == "textarea";
      const double content_height = std::max(0.0, n.box.h - n.style.padding_top - n.style.padding_bottom);
      double y = n.box.y + n.style.padding_top;
      if (!multiline) y += std::max(0.0, (content_height - line) * 0.5);
      double caret_y = y;
      while (std::getline(stream, text)) {
        caret_y = y;
        draw_text(context, text, n.box.x + n.style.padding_left, y + line * 0.78, font_size, display_color);
        y += line;
        if (y >= n.box.y + n.box.h) break;
      }
      if (n.id == focused_id_ && !bool_prop(n, "readOnly")) {
        const Size measured = measure_text(value.substr(value.find_last_of('\n') == std::string::npos ? 0 : value.find_last_of('\n') + 1), font_size);
        const double caret_x = std::min(n.box.x + n.box.w - 3.0, n.box.x + n.style.padding_left + measured.w + 1.0);
        context.fill_rect(BLRect(caret_x, caret_y + 2.0, 1.5, line - 4.0), BLRgba32(color));
      }
      return;
    }
    if (kind == ElementHandler::Kind::kSvg) {
      paint_svg(context, n, color);
    }
  }

  void paint_node(BLContext& context, uint64_t id, uint32_t inherited_color,
                  double inherited_font_size, const BLRectI* damage) {
    Node* n = node(id);
    if (!n || !n->style.visible) return;
    const bool node_intersects = !damage || intersects(n->box, *damage);
    const bool clips_children = n->handler->kind == ElementHandler::Kind::kVirtualList ||
                                n->style.overflow != Style::Overflow::kVisible;
    if (!node_intersects && (clips_children || n->children.empty())) return;
    ++painted_nodes_;
    const uint32_t color = n->style.color.value_or(inherited_color);
    const double font_size = n->style.font_size > 0.0 ? n->style.font_size : inherited_font_size;
    context.save();
    if (n->style.opacity < 1.0) {
      context.set_global_alpha(context.global_alpha() * std::clamp(n->style.opacity, 0.0, 1.0));
    }
    if (node_intersects && n->style.background) {
      BLRect rect(n->box.x, n->box.y, n->box.w, n->box.h);
      if (n->style.border_radius > 0.0) {
        context.fill_round_rect(rect, n->style.border_radius, n->style.border_radius,
                                BLRgba32(*n->style.background));
      } else {
        context.fill_rect(rect, BLRgba32(*n->style.background));
      }
    }
    if (node_intersects && n->style.border_width > 0.0 && n->style.border_color) {
      context.set_stroke_width(n->style.border_width);
      const double inset = n->style.border_width * 0.5;
      BLRect rect(n->box.x + inset, n->box.y + inset,
                  std::max(0.0, n->box.w - n->style.border_width),
                  std::max(0.0, n->box.h - n->style.border_width));
      if (n->style.border_radius > 0.0) {
        context.stroke_round_rect(rect, n->style.border_radius, n->style.border_radius,
                                  BLRgba32(*n->style.border_color));
      } else {
        context.stroke_rect(rect, BLRgba32(*n->style.border_color));
      }
    }
    if (node_intersects && n->handler->kind == ElementHandler::Kind::kText && !n->text.empty()) {
      BLFont font = make_font(font_size);
      const double baseline = n->box.y + font.metrics().ascent;
      context.fill_utf8_text(BLPoint(n->box.x, baseline), font, n->text.data(),
                             n->text.size(), BLRgba32(color));
    }
    if (node_intersects) paint_special(context, *n, color, font_size);
    if (clips_children) {
      context.save();
      context.clip_to_rect(BLRect(n->box.x, n->box.y, n->box.w, n->box.h));
    }
    if (n->handler->kind == ElementHandler::Kind::kVirtualList) {
      for (size_t i = n->visible_start; i < n->visible_end; ++i) {
        paint_node(context, n->children[i], color, font_size, damage);
      }
    } else {
      for (uint64_t child : n->children) {
        paint_node(context, child, color, font_size, damage);
      }
    }
    if (clips_children) context.restore();
    context.restore();
  }

  uint64_t hit_test(uint64_t id, double x, double y, const std::string& event) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end() || !it->second.style.visible || !it->second.box.contains(x, y)) return 0;
    if (it->second.handler->kind == ElementHandler::Kind::kVirtualList) {
      for (size_t i = it->second.visible_end; i > it->second.visible_start; --i) {
        const uint64_t hit = hit_test(it->second.children[i - 1], x, y, event);
        if (hit) return hit;
      }
    } else {
      for (auto child = it->second.children.rbegin(); child != it->second.children.rend(); ++child) {
        const uint64_t hit = hit_test(*child, x, y, event);
        if (hit) return hit;
      }
    }
    return it->second.events.count(event) ? id : 0;
  }

  uint64_t find_scroll_target(uint64_t id, double x, double y) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end() || !it->second.box.contains(x, y)) return 0;
    if (it->second.handler->kind == ElementHandler::Kind::kVirtualList) return id;
    for (auto child = it->second.children.rbegin(); child != it->second.children.rend(); ++child) {
      const uint64_t hit = find_scroll_target(*child, x, y);
      if (hit) return hit;
    }
    return it->second.style.overflow == Style::Overflow::kScroll ? id : 0;
  }

  uint64_t hit_test_input(uint64_t id, double x, double y) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end() || !it->second.style.visible || !it->second.box.contains(x, y)) return 0;
    for (auto child = it->second.children.rbegin(); child != it->second.children.rend(); ++child) {
      const uint64_t hit = hit_test_input(*child, x, y);
      if (hit) return hit;
    }
    return it->second.handler->kind == ElementHandler::Kind::kInput ? id : 0;
  }

  uint64_t hit_test_focusable(uint64_t id, double x, double y) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end() || !it->second.style.visible || !it->second.box.contains(x, y)) return 0;
    for (auto child = it->second.children.rbegin(); child != it->second.children.rend(); ++child) {
      const uint64_t hit = hit_test_focusable(*child, x, y);
      if (hit) return hit;
    }
    const double default_tab = it->second.type == "button" ||
                               it->second.handler->kind == ElementHandler::Kind::kInput ? 0.0 : -1.0;
    return !bool_prop(it->second, "disabled") &&
                   number_prop(it->second, "tabIndex", default_tab) >= 0.0 ? id : 0;
  }

  uint64_t hit_test_hoverable(uint64_t id, double x, double y) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end() || !it->second.style.visible || !it->second.box.contains(x, y)) return 0;
    for (auto child = it->second.children.rbegin(); child != it->second.children.rend(); ++child) {
      const uint64_t hit = hit_test_hoverable(*child, x, y);
      if (hit) return hit;
    }
    return it->second.events.count("mouseEnter") || it->second.events.count("mouseLeave") ? id : 0;
  }

  void update_hover(napi_env env, napi_ref callback_ref, double x, double y) {
    const uint64_t next = hit_test_hoverable(root_id_, x, y);
    if (next == hovered_id_) return;
    const uint64_t previous = hovered_id_;
    hovered_id_ = next;
    if (previous) emit_to(env, callback_ref, previous, "mouseLeave");
    if (hovered_id_) emit_to(env, callback_ref, hovered_id_, "mouseEnter");
  }

  void emit_outside(napi_env env, napi_ref callback_ref, double x, double y) {
    std::vector<uint64_t> outside;
    outside.reserve(nodes_.size());
    for (const auto& [id, candidate] : nodes_) {
      if (candidate.events.count("mouseDownOutside") && !candidate.box.contains(x, y)) {
        outside.push_back(id);
      }
    }
    for (uint64_t id : outside) emit_to(env, callback_ref, id, "mouseDownOutside");
  }

  void collect_focusable(uint64_t id, std::vector<uint64_t>& result) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end() || !it->second.style.visible) return;
    const double default_tab = it->second.type == "button" ||
                               it->second.handler->kind == ElementHandler::Kind::kInput ? 0.0 : -1.0;
    if (!bool_prop(it->second, "disabled") &&
        number_prop(it->second, "tabIndex", default_tab) >= 0.0) {
      result.push_back(id);
    }
    for (uint64_t child : it->second.children) collect_focusable(child, result);
  }

  void focus_next(napi_env env, napi_ref callback_ref, int direction) {
    std::vector<uint64_t> focusable;
    collect_focusable(root_id_, focusable);
    if (focusable.empty()) return;
    auto current = std::find(focusable.begin(), focusable.end(), focused_id_);
    ptrdiff_t index = current == focusable.end() ? (direction > 0 ? -1 : 0)
                                                 : std::distance(focusable.begin(), current);
    index = (index + direction + static_cast<ptrdiff_t>(focusable.size())) %
            static_cast<ptrdiff_t>(focusable.size());
    focus_element(env, callback_ref, focusable[static_cast<size_t>(index)]);
  }

  void emit_to(napi_env env, napi_ref callback_ref, uint64_t id, const std::string& event,
               const std::string& value = {}, const std::string& key = {}) {
    if (!callback_ref || !id) return;
    auto it = nodes_.find(id);
    if (it == nodes_.end() || !it->second.events.count(event)) return;
    napi_value callback;
    napi_value global;
    napi_value payload;
    napi_get_reference_value(env, callback_ref, &callback);
    napi_get_global(env, &global);
    napi_create_object(env, &payload);
    set_number(env, payload, "elementId", static_cast<double>(id));
    set_string(env, payload, "eventType", event);
    set_number(env, payload, "x", 0.0);
    set_number(env, payload, "y", 0.0);
    set_number(env, payload, "button", 0.0);
    set_number(env, payload, "deltaY", 0.0);
    set_string(env, payload, "value", value);
    if (!key.empty()) set_string(env, payload, "key", key);
    napi_value result;
    napi_call_function(env, global, callback, 1, &payload, &result);
  }

  void emit_pointer(napi_env env, napi_ref callback_ref, const std::string& event,
                    double x, double y, int button, double delta_y = 0.0) {
    if (!callback_ref || !root_id_) return;
    const uint64_t hit = hit_test(root_id_, x, y, event);
    if (!hit) return;
    napi_value callback;
    napi_value global;
    napi_value payload;
    napi_get_reference_value(env, callback_ref, &callback);
    napi_get_global(env, &global);
    napi_create_object(env, &payload);
    set_number(env, payload, "elementId", static_cast<double>(hit));
    set_string(env, payload, "eventType", event);
    set_number(env, payload, "x", x);
    set_number(env, payload, "y", y);
    set_number(env, payload, "button", button);
    set_number(env, payload, "deltaY", delta_y);
    napi_value result;
    napi_call_function(env, global, callback, 1, &payload, &result);
  }

  void present(napi_env env, const std::vector<BLRectI>& regions) {
    SDL_Surface* surface = SDL_GetWindowSurface(window_);
    if (!surface) {
      napi_throw_error(env, nullptr, SDL_GetError());
      return;
    }
    BLImageData data{};
    framebuffer_.get_data(&data);
    if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);
    const auto* source = static_cast<const uint8_t*>(data.pixel_data);
    auto* destination = static_cast<uint8_t*>(surface->pixels);
    std::vector<SDL_Rect> updates;
    updates.reserve(regions.size());
    for (const BLRectI& region : regions) {
      SDL_ConvertPixels(region.w, region.h, SDL_PIXELFORMAT_ARGB8888,
                        source + region.y * data.stride + region.x * 4,
                        static_cast<int>(data.stride), surface->format->format,
                        destination + region.y * surface->pitch + region.x * surface->format->BytesPerPixel,
                        surface->pitch);
      updates.push_back(SDL_Rect{region.x, region.y, region.w, region.h});
    }
    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
    if (!updates.empty()) {
      SDL_UpdateWindowSurfaceRects(window_, updates.data(), static_cast<int>(updates.size()));
    }
  }

  static void set_number(napi_env env, napi_value object, const char* name, double value) {
    napi_value js_value;
    napi_create_double(env, value, &js_value);
    napi_set_named_property(env, object, name, js_value);
  }

  static void set_string(napi_env env, napi_value object, const char* name,
                         const std::string& value) {
    napi_value js_value;
    napi_create_string_utf8(env, value.data(), value.size(), &js_value);
    napi_set_named_property(env, object, name, js_value);
  }

  SDL_Window* window_ = nullptr;
  bool sdl_initialized_ = false;
  bool headless_ = false;
  bool running_ = false;
  bool dirty_ = false;
  int width_ = 800;
  int height_ = 600;
  uint32_t threads_ = 0;
  BLImage framebuffer_;
  BLFontFace font_face_;
  mutable std::unordered_map<std::string, BLImage> image_cache_;
  mutable std::unordered_map<std::string, std::string> svg_cache_;
  std::unordered_map<uint64_t, Node> nodes_;
  ElementRegistry element_registry_;
  uint64_t root_id_ = 0;
  uint64_t frame_count_ = 0;
  double render_time_ms_ = 0.0;
  double layout_time_ms_ = 0.0;
  double paint_time_ms_ = 0.0;
  double present_time_ms_ = 0.0;
  uint64_t painted_pixels_ = 0;
  uint64_t painted_nodes_ = 0;
  size_t last_dirty_rect_count_ = 0;
  size_t mutations_last_commit_ = 0;
  bool force_full_repaint_ = true;
  std::vector<BLRectI> dirty_regions_;
  std::unordered_set<uint64_t> dirty_nodes_;
  std::unordered_set<uint64_t> scrolling_nodes_;
  std::vector<double> frame_samples_;
  uint64_t focused_id_ = 0;
  uint64_t hovered_id_ = 0;
  std::chrono::steady_clock::time_point last_poll_at_ = std::chrono::steady_clock::now();
};

Renderer renderer;
napi_ref event_callback = nullptr;

void check(napi_env env, napi_status status, const char* message) {
  if (status != napi_ok) napi_throw_error(env, nullptr, message);
}

std::vector<napi_value> args(napi_env env, napi_callback_info info, size_t count) {
  std::vector<napi_value> values(count);
  size_t actual = count;
  check(env, napi_get_cb_info(env, info, &actual, values.data(), nullptr, nullptr),
        "Could not read arguments");
  values.resize(actual);
  return values;
}

double number(napi_env env, napi_value value, double fallback = 0.0) {
  napi_valuetype type;
  if (napi_typeof(env, value, &type) != napi_ok || type != napi_number) return fallback;
  double result = fallback;
  napi_get_value_double(env, value, &result);
  return result;
}

bool boolean(napi_env env, napi_value value, bool fallback = false) {
  napi_valuetype type;
  if (napi_typeof(env, value, &type) != napi_ok || type != napi_boolean) return fallback;
  bool result = fallback;
  napi_get_value_bool(env, value, &result);
  return result;
}

std::string string(napi_env env, napi_value value, const std::string& fallback = {}) {
  napi_valuetype type;
  if (napi_typeof(env, value, &type) != napi_ok || type != napi_string) return fallback;
  size_t length = 0;
  napi_get_value_string_utf8(env, value, nullptr, 0, &length);
  std::string result(length + 1, '\0');
  napi_get_value_string_utf8(env, value, result.data(), result.size(), &length);
  result.resize(length);
  return result;
}

napi_value property(napi_env env, napi_value object, const char* name) {
  bool has = false;
  napi_has_named_property(env, object, name, &has);
  if (!has) return nullptr;
  napi_value value;
  napi_get_named_property(env, object, name, &value);
  return value;
}

uint64_t id_arg(napi_env env, napi_value value) {
  return static_cast<uint64_t>(std::max(0.0, number(env, value)));
}

Dimension dimension(napi_env env, napi_value value) {
  Dimension result;
  if (!value) return result;
  napi_valuetype type;
  napi_typeof(env, value, &type);
  if (type == napi_number) {
    result.set = true;
    result.value = number(env, value);
  } else if (type == napi_string) {
    std::string text = string(env, value);
    if (!text.empty() && text.back() == '%') {
      try {
        result.set = true;
        result.percent = true;
        result.value = std::stod(text.substr(0, text.size() - 1));
      } catch (...) {
      }
    }
  }
  return result;
}

std::optional<uint32_t> color(napi_env env, napi_value value) {
  if (!value) return std::nullopt;
  std::string text = string(env, value);
  if (text.empty() || text[0] != '#') return std::nullopt;
  text.erase(text.begin());
  if (text.size() == 3) {
    std::string expanded;
    for (char c : text) { expanded.push_back(c); expanded.push_back(c); }
    text = expanded;
  }
  if (text.size() != 6 && text.size() != 8) return std::nullopt;
  try {
    const uint32_t rgb = static_cast<uint32_t>(std::stoul(text.substr(0, 6), nullptr, 16));
    const uint32_t alpha = text.size() == 8
                               ? static_cast<uint32_t>(std::stoul(text.substr(6, 2), nullptr, 16))
                               : 0xFFu;
    return (alpha << 24) | rgb;
  } catch (...) {
    return std::nullopt;
  }
}

PropValue prop_value_from_js(napi_env env, napi_value value) {
  if (!value) return std::monostate{};
  napi_valuetype type = napi_undefined;
  if (napi_typeof(env, value, &type) != napi_ok ||
      type == napi_undefined || type == napi_null) {
    return std::monostate{};
  }
  if (type == napi_number) return number(env, value);
  if (type == napi_boolean) return boolean(env, value);
  if (type == napi_string) return string(env, value);
  if (type != napi_object) return std::monostate{};

  bool is_array = false;
  napi_is_array(env, value, &is_array);
  if (is_array) {
    uint32_t length = 0;
    napi_get_array_length(env, value, &length);
    std::vector<CanvasCommand> commands;
    commands.reserve(length);
    for (uint32_t i = 0; i < length; ++i) {
      napi_value raw;
      napi_get_element(env, value, i, &raw);
      napi_valuetype raw_type = napi_undefined;
      napi_typeof(env, raw, &raw_type);
      if (raw_type != napi_object) continue;
      CanvasCommand command;
      if (auto item = property(env, raw, "kind")) command.kind = string(env, item);
      if (auto item = property(env, raw, "x")) command.x = number(env, item);
      if (auto item = property(env, raw, "y")) command.y = number(env, item);
      if (auto item = property(env, raw, "x1")) command.x = number(env, item);
      if (auto item = property(env, raw, "y1")) command.y = number(env, item);
      if (auto item = property(env, raw, "x2")) command.x2 = number(env, item);
      if (auto item = property(env, raw, "y2")) command.y2 = number(env, item);
      if (auto item = property(env, raw, "width")) command.width = number(env, item);
      if (auto item = property(env, raw, "height")) command.height = number(env, item);
      if (auto item = property(env, raw, "radius")) command.radius = number(env, item);
      if (auto item = property(env, raw, "widthPx")) command.stroke_width = number(env, item, 1.0);
      if (auto item = property(env, raw, "fontSize")) command.font_size = number(env, item, 14.0);
      if (auto item = property(env, raw, "color")) command.color = color(env, item).value_or(command.color);
      if (auto item = property(env, raw, "fill")) command.fill = boolean(env, item, true);
      if (auto item = property(env, raw, "text")) command.text = string(env, item);
      if (!command.kind.empty()) commands.push_back(std::move(command));
    }
    return commands;
  }

  napi_value x = property(env, value, "x");
  napi_value y = property(env, value, "y");
  if (x && y) return BLPoint(number(env, x), number(env, y));
  return std::monostate{};
}

Style style_from_js(napi_env env, napi_value object) {
  Style style;
  napi_valuetype type;
  if (!object || napi_typeof(env, object, &type) != napi_ok || type != napi_object) return style;
  style.width = dimension(env, property(env, object, "width"));
  style.height = dimension(env, property(env, object, "height"));
  if (auto value = property(env, object, "minWidth")) style.min_width = number(env, value);
  if (auto value = property(env, object, "minHeight")) style.min_height = number(env, value);
  if (auto value = property(env, object, "flexDirection")) style.row = string(env, value) == "row";
  if (auto value = property(env, object, "flexGrow")) style.flex_grow = number(env, value);
  if (auto value = property(env, object, "flexShrink")) style.flex_shrink = number(env, value, 1.0);
  if (auto value = property(env, object, "gap")) style.gap = number(env, value);
  if (auto value = property(env, object, "padding")) {
    style.padding_left = style.padding_right = style.padding_top =
        style.padding_bottom = number(env, value);
  }
  if (auto value = property(env, object, "paddingHorizontal")) {
    style.padding_left = style.padding_right = number(env, value);
  }
  if (auto value = property(env, object, "paddingVertical")) {
    style.padding_top = style.padding_bottom = number(env, value);
  }
  if (auto value = property(env, object, "paddingLeft")) style.padding_left = number(env, value);
  if (auto value = property(env, object, "paddingRight")) style.padding_right = number(env, value);
  if (auto value = property(env, object, "paddingTop")) style.padding_top = number(env, value);
  if (auto value = property(env, object, "paddingBottom")) style.padding_bottom = number(env, value);
  if (auto value = property(env, object, "marginLeft")) style.margin_left = number(env, value);
  if (auto value = property(env, object, "marginRight")) style.margin_right = number(env, value);
  if (auto value = property(env, object, "marginTop")) style.margin_top = number(env, value);
  if (auto value = property(env, object, "marginBottom")) style.margin_bottom = number(env, value);
  style.background = color(env, property(env, object, "backgroundColor"));
  style.color = color(env, property(env, object, "color"));
  style.border_color = color(env, property(env, object, "borderColor"));
  if (auto value = property(env, object, "fontSize")) style.font_size = number(env, value, 16.0);
  if (auto value = property(env, object, "lineHeight")) style.line_height = number(env, value);
  if (auto value = property(env, object, "borderRadius")) style.border_radius = number(env, value);
  if (auto value = property(env, object, "borderWidth")) style.border_width = number(env, value);
  if (auto value = property(env, object, "opacity")) style.opacity = number(env, value, 1.0);
  if (auto value = property(env, object, "visibility")) style.visible = string(env, value) != "hidden";
  if (auto value = property(env, object, "overflow")) {
    const std::string overflow = string(env, value);
    if (overflow == "scroll") style.overflow = Style::Overflow::kScroll;
    else if (overflow == "hidden") style.overflow = Style::Overflow::kHidden;
  }
  if (auto value = property(env, object, "overflowY")) {
    const std::string overflow = string(env, value);
    if (overflow == "scroll") style.overflow = Style::Overflow::kScroll;
    else if (overflow == "hidden") style.overflow = Style::Overflow::kHidden;
  }
  if (auto value = property(env, object, "position")) {
    const std::string position = string(env, value);
    if (position == "absolute") style.position = Style::Position::kAbsolute;
    else if (position == "fixed") style.position = Style::Position::kFixed;
  }
  if (auto value = property(env, object, "left")) style.left = number(env, value);
  if (auto value = property(env, object, "right")) style.right = number(env, value);
  if (auto value = property(env, object, "top")) style.top = number(env, value);
  if (auto value = property(env, object, "bottom")) style.bottom = number(env, value);
  style.max_width = dimension(env, property(env, object, "maxWidth"));
  style.max_height = dimension(env, property(env, object, "maxHeight"));
  if (auto value = property(env, object, "alignItems")) {
    const std::string align = string(env, value);
    if (align == "center") style.align_items = Style::Align::kCenter;
    else if (align == "end" || align == "flex-end") style.align_items = Style::Align::kEnd;
    else if (align == "start" || align == "flex-start") style.align_items = Style::Align::kStart;
  }
  if (auto value = property(env, object, "justifyContent")) {
    const std::string justify = string(env, value);
    if (justify == "center") style.justify = Style::Justify::kCenter;
    else if (justify == "end" || justify == "flex-end") style.justify = Style::Justify::kEnd;
    else if (justify == "spaceBetween" || justify == "space-between") style.justify = Style::Justify::kSpaceBetween;
  }
  return style;
}

napi_value undefined(napi_env env) {
  napi_value result;
  napi_get_undefined(env, &result);
  return result;
}

napi_value init(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 1);
  napi_value options = values.empty() ? nullptr : values[0];
  std::string title = "BlendX";
  int width = 900;
  int height = 620;
  uint32_t threads = 0;
  std::string font_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
  bool headless = false;
  if (options) {
    if (auto value = property(env, options, "title")) title = string(env, value, title);
    if (auto value = property(env, options, "width")) width = static_cast<int>(number(env, value, width));
    if (auto value = property(env, options, "height")) height = static_cast<int>(number(env, value, height));
    if (auto value = property(env, options, "threads")) threads = static_cast<uint32_t>(number(env, value));
    if (auto value = property(env, options, "fontPath")) font_path = string(env, value, font_path);
    if (auto value = property(env, options, "headless")) headless = boolean(env, value);
  }
  std::string error;
  if (!renderer.init(title, width, height, threads, font_path, headless, error)) {
    napi_throw_error(env, nullptr, error.c_str());
  }
  return undefined(env);
}

napi_value shutdown(napi_env env, napi_callback_info) {
  renderer.shutdown();
  return undefined(env);
}

napi_value create_element(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 2);
  if (values.size() == 2) renderer.create_node(id_arg(env, values[0]), string(env, values[1]));
  return undefined(env);
}

napi_value destroy_element(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 1);
  std::vector<uint64_t> destroyed;
  if (!values.empty()) renderer.destroy_node(id_arg(env, values[0]), destroyed);
  napi_value result;
  napi_create_array_with_length(env, destroyed.size(), &result);
  for (size_t i = 0; i < destroyed.size(); ++i) {
    napi_value value;
    napi_create_double(env, static_cast<double>(destroyed[i]), &value);
    napi_set_element(env, result, static_cast<uint32_t>(i), value);
  }
  return result;
}

napi_value append_child(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 2);
  if (values.size() == 2) renderer.append_child(id_arg(env, values[0]), id_arg(env, values[1]));
  return undefined(env);
}

napi_value remove_child(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 2);
  if (values.size() == 2) renderer.remove_child(id_arg(env, values[0]), id_arg(env, values[1]));
  return undefined(env);
}

napi_value insert_before(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 3);
  if (values.size() == 3) renderer.insert_before(id_arg(env, values[0]), id_arg(env, values[1]), id_arg(env, values[2]));
  return undefined(env);
}

napi_value set_style(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 2);
  if (values.size() == 2) {
    renderer.set_style(id_arg(env, values[0]), style_from_js(env, values[1]));
  }
  return undefined(env);
}

napi_value set_text(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 2);
  if (values.size() == 2) {
    renderer.set_text(id_arg(env, values[0]), string(env, values[1]));
  }
  return undefined(env);
}

napi_value set_custom_prop(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 3);
  if (values.size() == 3) {
    renderer.set_custom_prop(id_arg(env, values[0]), string(env, values[1]),
                             prop_value_from_js(env, values[2]));
  }
  return undefined(env);
}

napi_value set_event_listener(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 3);
  if (values.size() == 3) {
    renderer.set_event(id_arg(env, values[0]), string(env, values[1]), boolean(env, values[2]));
  }
  return undefined(env);
}

napi_value set_root(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 1);
  if (!values.empty()) renderer.set_root(id_arg(env, values[0]));
  return undefined(env);
}

napi_value commit_mutations(napi_env env, napi_callback_info) {
  renderer.mark_dirty();
  return undefined(env);
}

napi_value apply_batch(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 1);
  if (values.empty()) return undefined(env);
  bool is_array = false;
  napi_is_array(env, values[0], &is_array);
  if (!is_array) {
    napi_throw_type_error(env, nullptr, "applyBatch expects an array");
    return undefined(env);
  }
  uint32_t count = 0;
  napi_get_array_length(env, values[0], &count);
  for (uint32_t i = 0; i < count; ++i) {
    napi_value operation;
    napi_get_element(env, values[0], i, &operation);
    bool operation_is_array = false;
    napi_is_array(env, operation, &operation_is_array);
    if (!operation_is_array) continue;
    uint32_t length = 0;
    napi_get_array_length(env, operation, &length);
    if (length == 0) continue;
    auto at = [&](uint32_t index) {
      napi_value value;
      napi_get_element(env, operation, index, &value);
      return value;
    };
    const std::string kind = string(env, at(0));
    if (kind == "create" && length >= 3) {
      renderer.create_node(id_arg(env, at(1)), string(env, at(2)));
    } else if (kind == "append" && length >= 3) {
      renderer.append_child(id_arg(env, at(1)), id_arg(env, at(2)));
    } else if (kind == "remove" && length >= 3) {
      renderer.remove_child(id_arg(env, at(1)), id_arg(env, at(2)));
    } else if (kind == "insert" && length >= 4) {
      renderer.insert_before(id_arg(env, at(1)), id_arg(env, at(2)), id_arg(env, at(3)));
    } else if (kind == "style" && length >= 3) {
      renderer.set_style(id_arg(env, at(1)), style_from_js(env, at(2)));
    } else if (kind == "text" && length >= 3) {
      renderer.set_text(id_arg(env, at(1)), string(env, at(2)));
    } else if (kind == "event" && length >= 4) {
      renderer.set_event(id_arg(env, at(1)), string(env, at(2)), boolean(env, at(3)));
    } else if (kind == "prop" && length >= 4) {
      renderer.set_custom_prop(id_arg(env, at(1)), string(env, at(2)),
                               prop_value_from_js(env, at(3)));
    } else if (kind == "root" && length >= 2) {
      renderer.set_root(id_arg(env, at(1)));
    }
  }
  renderer.record_mutations(count);
  return undefined(env);
}

napi_value set_event_callback(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 1);
  if (event_callback) {
    napi_delete_reference(env, event_callback);
    event_callback = nullptr;
  }
  if (!values.empty()) napi_create_reference(env, values[0], 1, &event_callback);
  return undefined(env);
}

napi_value poll(napi_env env, napi_callback_info) {
  napi_value result;
  napi_get_boolean(env, renderer.poll(env, event_callback), &result);
  return result;
}

napi_value render_frame(napi_env env, napi_callback_info) {
  renderer.render_frame(env);
  return undefined(env);
}

napi_value focus_element(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 1);
  if (!values.empty()) renderer.focus_element(env, event_callback, id_arg(env, values[0]));
  return undefined(env);
}

napi_value dispatch_pointer(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 4);
  if (values.size() >= 3) {
    renderer.dispatch_pointer(env, event_callback, string(env, values[0]),
                              number(env, values[1]), number(env, values[2]),
                              values.size() >= 4 ? static_cast<int>(number(env, values[3], 1.0)) : 1);
  }
  return undefined(env);
}

napi_value dispatch_key(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 1);
  if (!values.empty()) renderer.dispatch_key(env, event_callback, string(env, values[0]));
  return undefined(env);
}

napi_value scroll_to_item(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 2);
  if (values.size() == 2) {
    renderer.scroll_to_item(id_arg(env, values[0]),
                            static_cast<size_t>(std::max(0.0, number(env, values[1]))));
  }
  return undefined(env);
}

napi_value get_element_box(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 1);
  const Box box = values.empty() ? Box{} : renderer.element_box(id_arg(env, values[0]));
  napi_value result;
  napi_create_object(env, &result);
  auto set = [&](const char* name, double value) {
    napi_value js_value;
    napi_create_double(env, value, &js_value);
    napi_set_named_property(env, result, name, js_value);
  };
  set("x", box.x);
  set("y", box.y);
  set("width", box.w);
  set("height", box.h);
  return result;
}

napi_value capture_screenshot(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 1);
  if (values.empty() || renderer.capture_screenshot(string(env, values[0])) != BL_SUCCESS) {
    napi_throw_error(env, nullptr, "Could not write BlendX framebuffer screenshot");
  }
  return undefined(env);
}

napi_value get_stats(napi_env env, napi_callback_info) {
  napi_value result;
  napi_create_object(env, &result);
  auto set = [&](const char* name, double value) {
    napi_value js_value;
    napi_create_double(env, value, &js_value);
    napi_set_named_property(env, result, name, js_value);
  };
  set("width", renderer.width());
  set("height", renderer.height());
  set("nodeCount", renderer.node_count());
  set("frameCount", static_cast<double>(renderer.frame_count()));
  set("renderTimeMs", renderer.render_time_ms());
  set("layoutTimeMs", renderer.layout_time_ms());
  set("paintTimeMs", renderer.paint_time_ms());
  set("presentTimeMs", renderer.present_time_ms());
  set("paintedPixels", static_cast<double>(renderer.painted_pixels()));
  set("paintedNodes", static_cast<double>(renderer.painted_nodes()));
  set("dirtyRectCount", renderer.dirty_rect_count());
  set("mutationsLastCommit", renderer.mutations_last_commit());
  set("frameP50Ms", renderer.frame_percentile(0.50));
  set("frameP95Ms", renderer.frame_percentile(0.95));
  set("frameMaxMs", renderer.frame_percentile(1.0));
  set("threads", renderer.threads());
  return result;
}

napi_value module_init(napi_env env, napi_value exports) {
  const napi_property_descriptor properties[] = {
      {"init", nullptr, init, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"shutdown", nullptr, shutdown, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createElement", nullptr, create_element, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"destroyElement", nullptr, destroy_element, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"appendChild", nullptr, append_child, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"removeChild", nullptr, remove_child, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"insertBefore", nullptr, insert_before, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setStyle", nullptr, set_style, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setText", nullptr, set_text, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setCustomProp", nullptr, set_custom_prop, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setEventListener", nullptr, set_event_listener, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setRoot", nullptr, set_root, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"commitMutations", nullptr, commit_mutations, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"applyBatch", nullptr, apply_batch, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setEventCallback", nullptr, set_event_callback, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"poll", nullptr, poll, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"renderFrame", nullptr, render_frame, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"focusElement", nullptr, focus_element, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"dispatchPointer", nullptr, dispatch_pointer, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"dispatchKey", nullptr, dispatch_key, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"scrollToItem", nullptr, scroll_to_item, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getElementBox", nullptr, get_element_box, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"captureScreenshot", nullptr, capture_screenshot, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getStats", nullptr, get_stats, nullptr, nullptr, nullptr, napi_default, nullptr},
  };
  napi_define_properties(env, exports, sizeof(properties) / sizeof(properties[0]), properties);
  return exports;
}

}  // namespace

extern "C" napi_value blendx_module_init(napi_env env, napi_value exports) {
  return module_init(env, exports);
}

#if !defined(BLENDX_USE_HERMES_NAPI)
NAPI_MODULE(blendx_native, module_init)
#endif
