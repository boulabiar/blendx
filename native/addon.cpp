#include <hermes/napi/node_api.h>

#include <SDL.h>
#include <blend2d/blend2d.h>

#include "blendx_embedded_font.h"
#include "renderer_model.h"

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

using namespace blendx;

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

    BLResult font_result = BL_ERROR_INVALID_DATA;
    if (!font_path.empty()) {
      font_result = font_face_.create_from_file(font_path.c_str());
    } else {
      BLFontData font_data;
      if (font_data.create_from_data(kBlendxEmbeddedFont, kBlendxEmbeddedFontSize) == BL_SUCCESS) {
        font_result = font_face_.create_from_data(font_data, 0);
      }
    }
    if (font_result != BL_SUCCESS) {
      error = font_path.empty() ? "Could not load the embedded default font"
                                : "Could not load font: " + font_path;
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
    pointer_capture_id_ = 0;
    pressed_click_id_ = 0;
    scrollbar_drag_id_ = 0;
    modal_root_id_ = 0;
    selected_text_id_ = 0;
    selecting_text_ = false;
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
    if (pointer_capture_id_ == id) pointer_capture_id_ = 0;
    if (pressed_click_id_ == id) pressed_click_id_ = 0;
    if (scrollbar_drag_id_ == id) scrollbar_drag_id_ = 0;
    if (modal_root_id_ == id) modal_root_id_ = 0;
    if (selected_text_id_ == id) selected_text_id_ = 0;
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
    } else if (name == "value") {
      if (const std::string* text = std::get_if<std::string>(&value)) {
        const std::string previous = string_prop(*target, "value");
        if (previous.empty() && target->selection_start == 0 && target->selection_end == 0) {
          target->selection_start = target->selection_end = text->size();
        } else {
          target->selection_start = std::min(target->selection_start, text->size());
          target->selection_end = std::min(target->selection_end, text->size());
        }
      }
    } else if (name == "autoFocus" && std::get_if<bool>(&value) && std::get<bool>(value)) {
      focused_id_ = id;
      if (!headless_) SDL_StartTextInput();
    } else if (name == "modal") {
      if (const bool* enabled = std::get_if<bool>(&value); enabled && *enabled) modal_root_id_ = id;
      else if (modal_root_id_ == id) modal_root_id_ = 0;
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
  void focus_element(napi_env env, napi_ref callback_ref, uint64_t id,
                     bool programmatic = false) {
    Node* target = node(id);
    if (id && (!target || bool_prop(*target, "disabled") ||
               (!programmatic && number_prop(*target, "tabIndex", target->type == "button" ||
                                                   target->handler->kind == ElementHandler::Kind::kInput ? 0.0 : -1.0) < 0.0))) {
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
      if (scrollbar_drag_id_) {
        drag_scrollbar(env, callback_ref, y);
        return;
      }
      if (selecting_text_) {
        if (Node* target = node(selected_text_id_)) {
          text_selection_focus_ = text_offset_at(*target, x, y);
          invalidate_paint(selected_text_id_);
        }
        return;
      }
      update_hover(env, callback_ref, x, y);
      emit_pointer(env, callback_ref, "mouseMove", x, y, button, 0.0, pointer_capture_id_);
      return;
    }
    if (kind == "click") {
      emit_pointer(env, callback_ref, "click", x, y, button);
      return;
    }
    if (kind == "mouseDown") {
      if (button == 1 && begin_scrollbar_drag(x, y)) return;
      if (button == 1) {
        const uint64_t selectable = hit_test_selectable_text(root_id_, x, y);
        if (Node* target = node(selectable)) {
          selected_text_id_ = selectable;
          text_selection_anchor_ = text_selection_focus_ = text_offset_at(*target, x, y);
          selecting_text_ = true;
          invalidate_paint(selectable);
          return;
        }
      }
      pressed_click_id_ = hit_test(root_id_, x, y, "click");
      pointer_capture_id_ = hit_test(root_id_, x, y, "mouseDown");
      emit_pointer(env, callback_ref, kind, x, y, button, 0.0, pointer_capture_id_);
      emit_outside(env, callback_ref, x, y);
      const uint64_t focus_id = hit_test_focusable(root_id_, x, y);
      focus_element(env, callback_ref, focus_id);
      position_input_caret(focus_id, x, y);
    } else if (kind == "mouseUp") {
      if (selecting_text_) {
        if (Node* target = node(selected_text_id_)) text_selection_focus_ = text_offset_at(*target, x, y);
        selecting_text_ = false;
        invalidate_paint(selected_text_id_);
        return;
      }
      if (scrollbar_drag_id_) {
        drag_scrollbar(env, callback_ref, y);
        scrollbar_drag_id_ = 0;
        return;
      }
      emit_pointer(env, callback_ref, kind, x, y, button, 0.0, pointer_capture_id_);
      const uint64_t released_click_id = hit_test(root_id_, x, y, "click");
      if (pressed_click_id_ && released_click_id == pressed_click_id_) {
        emit_pointer(env, callback_ref, "click", x, y, button, 0.0, pressed_click_id_);
      }
      pointer_capture_id_ = 0;
      pressed_click_id_ = 0;
    } else {
      emit_pointer(env, callback_ref, kind, x, y, button);
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
  void scroll_to_offset(uint64_t id, double offset) {
    Node* target = node(id);
    if (!target) return;
    const double max_scroll = std::max(0.0, target->content_height - target->box.h);
    target->scroll_y = std::clamp(offset, 0.0, max_scroll);
    target->scroll_target_y = target->scroll_y;
    scrolling_nodes_.erase(id);
    invalidate_layout(id);
  }
  Box element_box(uint64_t id) const {
    auto it = nodes_.find(id);
    return it == nodes_.end() ? Box{} : it->second.box;
  }
  std::string selected_text() const {
    auto found = nodes_.find(selected_text_id_);
    if (found == nodes_.end()) return {};
    const size_t start = std::min(text_selection_anchor_, text_selection_focus_);
    const size_t end = std::min(found->second.text.size(),
                                std::max(text_selection_anchor_, text_selection_focus_));
    return end > start ? found->second.text.substr(start, end - start) : std::string{};
  }
  std::vector<AccessibilityNode> accessibility_nodes() const {
    std::vector<AccessibilityNode> result;
    collect_accessibility(root_id_, result);
    return result;
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
  size_t frames_over_budget(double budget_ms) const {
    return static_cast<size_t>(std::count_if(
        frame_samples_.begin(), frame_samples_.end(),
        [budget_ms](double sample) { return sample > budget_ms; }));
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
          if (event.type == SDL_MOUSEBUTTONDOWN) {
            if (event.button.button == SDL_BUTTON_LEFT &&
                begin_scrollbar_drag(event.button.x, event.button.y)) {
              continue;
            }
            if (event.button.button == SDL_BUTTON_LEFT) {
              const uint64_t selectable = hit_test_selectable_text(root_id_, event.button.x, event.button.y);
              if (Node* target = node(selectable)) {
                selected_text_id_ = selectable;
                text_selection_anchor_ = text_selection_focus_ =
                    text_offset_at(*target, event.button.x, event.button.y);
                selecting_text_ = true;
                invalidate_paint(selectable);
                continue;
              }
            }
            pressed_click_id_ = hit_test(root_id_, event.button.x, event.button.y, "click");
            pointer_capture_id_ = hit_test(root_id_, event.button.x, event.button.y, kind);
            emit_pointer(env, event_callback, kind, event.button.x, event.button.y,
                         event.button.button, 0.0, pointer_capture_id_);
            emit_outside(env, event_callback, event.button.x, event.button.y);
            const uint64_t focus_id = hit_test_focusable(root_id_, event.button.x, event.button.y);
            focus_element(env, event_callback, focus_id);
            position_input_caret(focus_id, event.button.x, event.button.y);
          } else {
            if (selecting_text_) {
              if (Node* target = node(selected_text_id_)) {
                text_selection_focus_ = text_offset_at(*target, event.button.x, event.button.y);
              }
              selecting_text_ = false;
              invalidate_paint(selected_text_id_);
              continue;
            }
            if (scrollbar_drag_id_) {
              drag_scrollbar(env, event_callback, event.button.y);
              scrollbar_drag_id_ = 0;
              continue;
            }
            emit_pointer(env, event_callback, kind, event.button.x, event.button.y,
                         event.button.button, 0.0, pointer_capture_id_);
            const uint64_t released_click_id = hit_test(
                root_id_, event.button.x, event.button.y, "click");
            if (pressed_click_id_ && released_click_id == pressed_click_id_) {
              emit_pointer(env, event_callback, "click", event.button.x, event.button.y,
                           event.button.button, 0.0, pressed_click_id_);
            }
            pointer_capture_id_ = 0;
            pressed_click_id_ = 0;
          }
        } else if (event.type == SDL_MOUSEMOTION) {
          if (scrollbar_drag_id_) {
            drag_scrollbar(env, event_callback, event.motion.y);
            continue;
          }
          if (selecting_text_) {
            if (Node* target = node(selected_text_id_)) {
              text_selection_focus_ = text_offset_at(*target, event.motion.x, event.motion.y);
              invalidate_paint(selected_text_id_);
            }
            continue;
          }
          update_hover(env, event_callback, event.motion.x, event.motion.y);
          emit_pointer(env, event_callback, "mouseMove", event.motion.x, event.motion.y, 0,
                       0.0, pointer_capture_id_);
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
            emit_scroll(env, event_callback, target_id, delta_y);
          }
        } else if (event.type == SDL_TEXTINPUT && focused_id_) {
          if (Node* target = node(focused_id_); target && !bool_prop(*target, "readOnly")) {
            const std::string value = replace_input_selection(*target, event.text.text);
            target->composition.clear();
            invalidate_paint(focused_id_);
            emit_to(env, event_callback, focused_id_, "change", value);
          }
        } else if (event.type == SDL_TEXTEDITING && focused_id_) {
          if (Node* target = node(focused_id_); target && target->handler->kind == ElementHandler::Kind::kInput) {
            target->composition = event.edit.text;
            invalidate_paint(focused_id_);
          }
        } else if (event.type == SDL_KEYDOWN && selected_text_id_ &&
                   (event.key.keysym.mod & (KMOD_CTRL | KMOD_GUI)) != 0 &&
                   event.key.keysym.sym == SDLK_c) {
          if (Node* target = node(selected_text_id_)) {
            const size_t start = std::min(text_selection_anchor_, text_selection_focus_);
            const size_t end = std::max(text_selection_anchor_, text_selection_focus_);
            if (end > start) SDL_SetClipboardText(target->text.substr(start, end - start).c_str());
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
          else if (code == SDLK_LEFT) key = "ArrowLeft";
          else if (code == SDLK_RIGHT) key = "ArrowRight";
          else if (code == SDLK_HOME) key = "Home";
          else if (code == SDLK_END) key = "End";
          else if (code == SDLK_DELETE) key = "Delete";
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
          if (target->handler->kind != ElementHandler::Kind::kInput || bool_prop(*target, "readOnly")) continue;
          std::string value = string_prop(*target, "value");
          const bool control = (event.key.keysym.mod & (KMOD_CTRL | KMOD_GUI)) != 0;
          const bool shift = (event.key.keysym.mod & KMOD_SHIFT) != 0;
          auto changed = [&]() {
            invalidate_paint(focused_id_);
            emit_to(env, event_callback, focused_id_, "change", string_prop(*target, "value"), key);
          };
          auto move_selection = [&](size_t next) {
            next = std::min(next, value.size());
            if (shift) target->selection_end = next;
            else target->selection_start = target->selection_end = next;
            invalidate_paint(focused_id_);
          };
          const size_t selection_begin = std::min(target->selection_start, target->selection_end);
          const size_t selection_end = std::max(target->selection_start, target->selection_end);
          if (control && code == SDLK_a) {
            target->selection_start = 0;
            target->selection_end = value.size();
            invalidate_paint(focused_id_);
          } else if (control && (code == SDLK_c || code == SDLK_x)) {
            if (selection_end > selection_begin) {
              SDL_SetClipboardText(value.substr(selection_begin, selection_end - selection_begin).c_str());
              if (code == SDLK_x) { replace_input_selection(*target, ""); changed(); }
            }
          } else if (control && code == SDLK_v) {
            char* clipboard = SDL_GetClipboardText();
            replace_input_selection(*target, clipboard ? clipboard : "");
            if (clipboard) SDL_free(clipboard);
            changed();
          } else if (control && code == SDLK_z && !shift && !target->undo_stack.empty()) {
            target->redo_stack.push_back(value);
            value = target->undo_stack.back();
            target->undo_stack.pop_back();
            target->props.insert_or_assign("value", value);
            target->selection_start = target->selection_end = value.size();
            changed();
          } else if (control && (code == SDLK_y || (shift && code == SDLK_z)) && !target->redo_stack.empty()) {
            target->undo_stack.push_back(value);
            value = target->redo_stack.back();
            target->redo_stack.pop_back();
            target->props.insert_or_assign("value", value);
            target->selection_start = target->selection_end = value.size();
            changed();
          } else if (code == SDLK_LEFT) {
            move_selection(!shift && selection_end > selection_begin
                               ? selection_begin : previous_utf8(value, target->selection_end));
          } else if (code == SDLK_RIGHT) {
            move_selection(!shift && selection_end > selection_begin
                               ? selection_end : next_utf8(value, target->selection_end));
          } else if (code == SDLK_UP || code == SDLK_DOWN) {
            const size_t current = target->selection_end;
            const size_t marker = current == 0 ? std::string::npos : value.rfind('\n', current - 1);
            const size_t line_start = marker == std::string::npos ? 0 : marker + 1;
            const size_t column = current - line_start;
            if (code == SDLK_UP) {
              if (line_start == 0) move_selection(0);
              else {
                const size_t previous_end = line_start - 1;
                const size_t previous_marker = previous_end == 0 ? std::string::npos : value.rfind('\n', previous_end - 1);
                const size_t previous_start = previous_marker == std::string::npos ? 0 : previous_marker + 1;
                move_selection(previous_start + std::min(column, previous_end - previous_start));
              }
            } else {
              const size_t line_end = value.find('\n', current);
              if (line_end == std::string::npos) move_selection(value.size());
              else {
                const size_t next_start = line_end + 1;
                const size_t next_marker = value.find('\n', next_start);
                const size_t next_end = next_marker == std::string::npos ? value.size() : next_marker;
                move_selection(next_start + std::min(column, next_end - next_start));
              }
            }
          } else if (code == SDLK_HOME) {
            const size_t newline = value.rfind('\n', target->selection_end == 0 ? 0 : target->selection_end - 1);
            move_selection(newline == std::string::npos ? 0 : newline + 1);
          } else if (code == SDLK_END) {
            const size_t newline = value.find('\n', target->selection_end);
            move_selection(newline == std::string::npos ? value.size() : newline);
          } else if (code == SDLK_BACKSPACE && (selection_end > selection_begin || !value.empty())) {
            if (selection_end == selection_begin) target->selection_start = previous_utf8(value, selection_begin);
            replace_input_selection(*target, "");
            changed();
          } else if (code == SDLK_DELETE && (selection_end > selection_begin || selection_begin < value.size())) {
            if (selection_end == selection_begin) target->selection_end = next_utf8(value, selection_end);
            replace_input_selection(*target, "");
            changed();
          } else if (code == SDLK_RETURN) {
            const bool multiline = target->type == "textarea";
            const bool submit = !multiline || (event.key.keysym.mod & KMOD_CTRL) != 0;
            if (submit) {
              emit_to(env, event_callback, focused_id_, "submit", value, key);
            } else {
              replace_input_selection(*target, "\n");
              changed();
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
      emit_scroll(env, event_callback, id, 0.0);
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

  static size_t previous_utf8(const std::string& value, size_t at) {
    if (at == 0) return 0;
    --at;
    while (at > 0 && (static_cast<unsigned char>(value[at]) & 0xC0u) == 0x80u) --at;
    return at;
  }

  static size_t next_utf8(const std::string& value, size_t at) {
    if (at >= value.size()) return value.size();
    ++at;
    while (at < value.size() && (static_cast<unsigned char>(value[at]) & 0xC0u) == 0x80u) ++at;
    return at;
  }

  void remember_input(Node& node, const std::string& value) {
    if (node.undo_stack.empty() || node.undo_stack.back() != value) {
      node.undo_stack.push_back(value);
      if (node.undo_stack.size() > 100) node.undo_stack.erase(node.undo_stack.begin());
    }
    node.redo_stack.clear();
  }

  std::string replace_input_selection(Node& node, const std::string& replacement) {
    std::string value = string_prop(node, "value");
    const size_t start = std::min(node.selection_start, node.selection_end);
    const size_t end = std::max(node.selection_start, node.selection_end);
    remember_input(node, value);
    value.replace(start, end - start, replacement);
    node.selection_start = node.selection_end = start + replacement.size();
    node.props.insert_or_assign("value", value);
    return value;
  }

  void position_input_caret(uint64_t id, double x, double y) {
    Node* target = node(id);
    if (!target || target->handler->kind != ElementHandler::Kind::kInput) return;
    const std::string value = string_prop(*target, "value");
    const double line_height = target->style.line_height > 0.0
                                   ? target->style.line_height
                                   : target->style.font_size * 1.35;
    const size_t wanted_line = target->type == "textarea"
                                   ? static_cast<size_t>(std::max(0.0, std::floor((y - target->box.y - target->style.padding_top) / line_height)))
                                   : 0;
    size_t line_start = 0;
    for (size_t line = 0; line < wanted_line; ++line) {
      const size_t newline = value.find('\n', line_start);
      if (newline == std::string::npos) { line_start = value.size(); break; }
      line_start = newline + 1;
    }
    const size_t line_end = value.find('\n', line_start) == std::string::npos
                                ? value.size() : value.find('\n', line_start);
    const double local_x = std::max(0.0, x - target->box.x - target->style.padding_left);
    size_t caret = line_start;
    while (caret < line_end) {
      const size_t next = next_utf8(value, caret);
      if (measure_text(value.substr(line_start, next - line_start), target->style.font_size).w > local_x) break;
      caret = next;
    }
    target->selection_start = target->selection_end = caret;
    invalidate_paint(id);
  }

  std::vector<std::string> text_lines(const Node& node, double width) const {
    std::vector<std::string> lines;
    const bool wrap = node.style.white_space == Style::WhiteSpace::kNormal ||
                      node.style.white_space == Style::WhiteSpace::kPreWrap;
    std::istringstream paragraphs(node.text);
    std::string paragraph;
    while (std::getline(paragraphs, paragraph)) {
      if (!wrap) {
        lines.push_back(paragraph);
        continue;
      }
      std::istringstream words(paragraph);
      std::string word;
      std::string line;
      while (words >> word) {
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && measure_text(candidate, node.style.font_size).w > width) {
          lines.push_back(line);
          line = word;
        } else {
          line = candidate;
        }
      }
      lines.push_back(line);
    }
    if (lines.empty()) lines.emplace_back();
    return lines;
  }

  size_t text_offset_at(const Node& node, double x, double y) const {
    const std::vector<std::string> lines = text_lines(node, std::max(1.0, node.box.w));
    const double line_height = node.style.line_height > 0.0
                                   ? node.style.line_height
                                   : measure_text("Mg", node.style.font_size).h;
    const size_t line_index = std::min(lines.size() - 1,
        static_cast<size_t>(std::max(0.0, std::floor((y - node.box.y) / std::max(1.0, line_height)))));
    size_t search_from = 0;
    size_t source_start = 0;
    for (size_t index = 0; index <= line_index; ++index) {
      const size_t found = node.text.find(lines[index], search_from);
      source_start = found == std::string::npos ? search_from : found;
      search_from = source_start + lines[index].size();
    }
    const std::string& line = lines[line_index];
    const double local_x = std::max(0.0, x - node.box.x);
    size_t offset = 0;
    while (offset < line.size()) {
      const size_t next = next_utf8(line, offset);
      if (measure_text(line.substr(0, next), node.style.font_size).w > local_x) break;
      offset = next;
    }
    return std::min(node.text.size(), source_start + offset);
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

#include "renderer_layout.inc"

#include "renderer_paint.inc"

  template <typename Visitor>
  uint64_t visit_children_front_to_back(const Node& parent, const Visitor& visitor) const {
    const bool layered = std::any_of(parent.children.begin(), parent.children.end(), [this](uint64_t child) {
      return nodes_.at(child).style.z_index != 0.0;
    });
    if (layered) {
      std::vector<uint64_t> order = parent.children;
      std::stable_sort(order.begin(), order.end(), [this](uint64_t a, uint64_t b) {
        return nodes_.at(a).style.z_index < nodes_.at(b).style.z_index;
      });
      for (auto child = order.rbegin(); child != order.rend(); ++child) {
        if (const uint64_t hit = visitor(*child)) return hit;
      }
    } else {
      for (auto child = parent.children.rbegin(); child != parent.children.rend(); ++child) {
        if (const uint64_t hit = visitor(*child)) return hit;
      }
    }
    return 0;
  }

  uint64_t hit_test(uint64_t id, double x, double y, const std::string& event) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end() || !it->second.style.visible) return 0;
    const bool inside = it->second.box.contains(x, y);
    if (!inside && it->second.style.overflow != Style::Overflow::kVisible) return 0;
    if (it->second.handler->kind == ElementHandler::Kind::kVirtualList) {
      for (size_t i = it->second.visible_end; i > it->second.visible_start; --i) {
        const uint64_t hit = hit_test(it->second.children[i - 1], x, y, event);
        if (hit) return hit;
      }
    } else {
      if (const uint64_t hit = visit_children_front_to_back(it->second, [this, x, y, &event](uint64_t child) {
            return hit_test(child, x, y, event);
          })) return hit;
    }
    return inside && it->second.events.count(event) ? id : 0;
  }

  uint64_t find_scroll_target(uint64_t id, double x, double y) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end() || !it->second.style.visible) return 0;
    const bool inside = it->second.box.contains(x, y);
    if (!inside && it->second.style.overflow != Style::Overflow::kVisible) return 0;
    if (inside && it->second.handler->kind == ElementHandler::Kind::kVirtualList) return id;
    if (const uint64_t hit = visit_children_front_to_back(it->second, [this, x, y](uint64_t child) {
          return find_scroll_target(child, x, y);
        })) return hit;
    return inside && it->second.style.overflow == Style::Overflow::kScroll ? id : 0;
  }

  Box scrollbar_thumb(const Node& node) const {
    if (node.style.overflow != Style::Overflow::kScroll ||
        node.content_height <= node.box.h + 0.5) return {};
    const double track_height = std::max(0.0, node.box.h - 8.0);
    const double thumb_height = std::max(24.0, track_height * node.box.h / node.content_height);
    const double max_scroll = std::max(1.0, node.content_height - node.box.h);
    const double travel = std::max(0.0, track_height - thumb_height);
    return {node.box.x + node.box.w - 9.0, node.box.y + 4.0 + travel * node.scroll_y / max_scroll,
            8.0, thumb_height};
  }

  uint64_t hit_test_scrollbar(uint64_t id, double x, double y) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end() || !it->second.style.visible) return 0;
    const bool inside = it->second.box.contains(x, y);
    if (!inside && it->second.style.overflow != Style::Overflow::kVisible) return 0;
    if (inside && scrollbar_thumb(it->second).contains(x, y)) return id;
    return visit_children_front_to_back(it->second, [this, x, y](uint64_t child) {
      return hit_test_scrollbar(child, x, y);
    });
  }

  bool begin_scrollbar_drag(double x, double y) {
    const uint64_t id = hit_test_scrollbar(root_id_, x, y);
    Node* target = node(id);
    if (!target) return false;
    const Box thumb = scrollbar_thumb(*target);
    scrollbar_drag_id_ = id;
    scrollbar_drag_offset_ = y - thumb.y;
    pressed_click_id_ = 0;
    pointer_capture_id_ = 0;
    return true;
  }

  void drag_scrollbar(napi_env env, napi_ref callback_ref, double y) {
    Node* target = node(scrollbar_drag_id_);
    if (!target) return;
    const Box thumb = scrollbar_thumb(*target);
    const double track_height = std::max(0.0, target->box.h - 8.0);
    const double travel = std::max(1.0, track_height - thumb.h);
    const double ratio = std::clamp((y - target->box.y - 4.0 - scrollbar_drag_offset_) / travel,
                                    0.0, 1.0);
    target->scroll_y = ratio * std::max(0.0, target->content_height - target->box.h);
    target->scroll_target_y = target->scroll_y;
    scrolling_nodes_.erase(scrollbar_drag_id_);
    invalidate_layout(scrollbar_drag_id_);
    emit_scroll(env, callback_ref, scrollbar_drag_id_, 0.0);
  }

  uint64_t hit_test_input(uint64_t id, double x, double y) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end() || !it->second.style.visible) return 0;
    const bool inside = it->second.box.contains(x, y);
    if (!inside && it->second.style.overflow != Style::Overflow::kVisible) return 0;
    if (const uint64_t hit = visit_children_front_to_back(it->second, [this, x, y](uint64_t child) {
          return hit_test_input(child, x, y);
        })) return hit;
    return inside && it->second.handler->kind == ElementHandler::Kind::kInput ? id : 0;
  }

  uint64_t hit_test_focusable(uint64_t id, double x, double y) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end() || !it->second.style.visible) return 0;
    const bool inside = it->second.box.contains(x, y);
    if (!inside && it->second.style.overflow != Style::Overflow::kVisible) return 0;
    if (const uint64_t hit = visit_children_front_to_back(it->second, [this, x, y](uint64_t child) {
          return hit_test_focusable(child, x, y);
        })) return hit;
    const double default_tab = it->second.type == "button" ||
                               it->second.handler->kind == ElementHandler::Kind::kInput ? 0.0 : -1.0;
    return inside && !bool_prop(it->second, "disabled") &&
                   number_prop(it->second, "tabIndex", default_tab) >= 0.0 ? id : 0;
  }

  uint64_t hit_test_hoverable(uint64_t id, double x, double y) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end() || !it->second.style.visible) return 0;
    const bool inside = it->second.box.contains(x, y);
    if (!inside && it->second.style.overflow != Style::Overflow::kVisible) return 0;
    if (const uint64_t hit = visit_children_front_to_back(it->second, [this, x, y](uint64_t child) {
          return hit_test_hoverable(child, x, y);
        })) return hit;
    return inside && (it->second.events.count("mouseEnter") ||
                      it->second.events.count("mouseLeave")) ? id : 0;
  }

  uint64_t hit_test_selectable_text(uint64_t id, double x, double y) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end() || !it->second.style.visible) return 0;
    const bool inside = it->second.box.contains(x, y);
    if (!inside && it->second.style.overflow != Style::Overflow::kVisible) return 0;
    if (const uint64_t hit = visit_children_front_to_back(it->second, [this, x, y](uint64_t child) {
          return hit_test_selectable_text(child, x, y);
        })) return hit;
    return inside && it->second.handler->kind == ElementHandler::Kind::kText &&
                   bool_prop(it->second, "selectable") ? id : 0;
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

  std::string descendant_text(uint64_t id) const {
    auto found = nodes_.find(id);
    if (found == nodes_.end()) return {};
    if (!found->second.text.empty()) return found->second.text;
    std::string result;
    for (uint64_t child : found->second.children) {
      const std::string text = descendant_text(child);
      if (text.empty()) continue;
      if (!result.empty()) result.push_back(' ');
      result += text;
    }
    return result;
  }

  void collect_accessibility(uint64_t id, std::vector<AccessibilityNode>& result) const {
    auto found = nodes_.find(id);
    if (found == nodes_.end() || !found->second.style.visible) return;
    const Node& node = found->second;
    std::string role = string_prop(node, "accessibilityRole");
    if (role.empty()) {
      if (node.type == "button") role = "button";
      else if (node.handler->kind == ElementHandler::Kind::kInput) role = "textbox";
      else if (node.handler->kind == ElementHandler::Kind::kImage) role = "image";
    }
    if (!role.empty()) {
      std::string label = string_prop(node, "accessibilityLabel");
      if (label.empty()) label = string_prop(node, "alt");
      if (label.empty()) label = descendant_text(id);
      if (label.empty()) label = string_prop(node, "placeholder");
      std::string checked = string_prop(node, "accessibilityChecked");
      if (checked.empty() && node.props.find("accessibilityChecked") != node.props.end()) {
        checked = bool_prop(node, "accessibilityChecked") ? "true" : "false";
      }
      result.push_back({id, role, label, string_prop(node, "accessibilityDescription"),
                        string_prop(node, "accessibilityValue"), checked,
                        bool_prop(node, "disabled"), bool_prop(node, "accessibilitySelected"), node.box});
    }
    for (uint64_t child : node.children) collect_accessibility(child, result);
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
    collect_focusable(modal_root_id_ ? modal_root_id_ : root_id_, focusable);
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
                    double x, double y, int button, double delta_y = 0.0,
                    uint64_t forced_id = 0) {
    if (!callback_ref || !root_id_) return;
    const uint64_t hit = forced_id ? forced_id : hit_test(root_id_, x, y, event);
    if (!hit) return;
    auto target = nodes_.find(hit);
    if (target == nodes_.end() || !target->second.events.count(event)) return;
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

  void emit_scroll(napi_env env, napi_ref callback_ref, uint64_t id, double delta_y) {
    if (!callback_ref || !id) return;
    Node* target = node(id);
    if (!target || !target->events.count("scroll")) return;
    napi_value callback;
    napi_value global;
    napi_value payload;
    napi_get_reference_value(env, callback_ref, &callback);
    napi_get_global(env, &global);
    napi_create_object(env, &payload);
    set_number(env, payload, "elementId", static_cast<double>(id));
    set_string(env, payload, "eventType", "scroll");
    set_number(env, payload, "x", 0.0);
    set_number(env, payload, "y", 0.0);
    set_number(env, payload, "button", 0.0);
    set_number(env, payload, "deltaY", delta_y);
    set_number(env, payload, "scrollOffset", target->scroll_y);
    set_number(env, payload, "scrollTarget", target->scroll_target_y);
    set_number(env, payload, "viewportSize", target->box.h);
    set_number(env, payload, "contentSize", target->content_height);
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
  uint64_t pointer_capture_id_ = 0;
  uint64_t pressed_click_id_ = 0;
  uint64_t scrollbar_drag_id_ = 0;
  double scrollbar_drag_offset_ = 0.0;
  uint64_t modal_root_id_ = 0;
  uint64_t selected_text_id_ = 0;
  size_t text_selection_anchor_ = 0;
  size_t text_selection_focus_ = 0;
  bool selecting_text_ = false;
  std::chrono::steady_clock::time_point last_poll_at_ = std::chrono::steady_clock::now();
};

#include "napi_protocol.inc"

}  // namespace

extern "C" napi_value blendx_module_init(napi_env env, napi_value exports) {
  return module_init(env, exports);
}

#if !defined(BLENDX_USE_HERMES_NAPI)
NAPI_MODULE(blendx_native, module_init)
#endif
