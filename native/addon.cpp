#include <node_api.h>

#include <SDL.h>
#include <blend2d/blend2d.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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
  double gap = 0.0;
  double padding_x = 0.0;
  double padding_y = 0.0;
  std::optional<uint32_t> background;
  std::optional<uint32_t> color;
  double font_size = 16.0;
  double border_radius = 0.0;
  bool visible = true;
  enum class Overflow { kVisible, kHidden, kScroll } overflow = Overflow::kVisible;

  bool same_layout(const Style& other) const {
    auto same_dimension = [](const Dimension& a, const Dimension& b) {
      return a.set == b.set && a.percent == b.percent && a.value == b.value;
    };
    return same_dimension(width, other.width) && same_dimension(height, other.height) &&
           min_width == other.min_width && min_height == other.min_height && row == other.row &&
           flex_grow == other.flex_grow && gap == other.gap && padding_x == other.padding_x &&
           padding_y == other.padding_y && font_size == other.font_size;
  }


  bool same_visual(const Style& other) const {
    return background == other.background && color == other.color &&
           border_radius == other.border_radius && visible == other.visible &&
           overflow == other.overflow;
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
  enum class Kind { kContainer, kText, kVirtualList } kind;
  const char* name;
};

class ElementRegistry {
 public:
  ElementRegistry() {
    register_handler("div", {ElementHandler::Kind::kContainer, "div"});
    register_handler("text", {ElementHandler::Kind::kText, "text"});
    register_handler("virtual-list", {ElementHandler::Kind::kVirtualList, "virtual-list"});
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
  double scroll_y = 0.0;
  double content_height = 0.0;
  double item_height = 28.0;
  uint32_t overdraw = 2;
  size_t visible_start = 0;
  size_t visible_end = 0;
};

struct Size {
  double w = 0.0;
  double h = 0.0;
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
    return true;
  }

  void shutdown() {
    if (window_) SDL_DestroyWindow(window_);
    window_ = nullptr;
    if (sdl_initialized_) SDL_Quit();
    sdl_initialized_ = false;
    framebuffer_.reset();
    nodes_.clear();
    root_id_ = 0;
    running_ = false;
    dirty_ = false;
    frame_count_ = 0;
    render_time_ms_ = 0.0;
    frame_samples_.clear();
    dirty_regions_.clear();
    dirty_nodes_.clear();
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
  void set_custom_prop(uint64_t id, const std::string& name, std::optional<double> value) {
    Node* target = node(id);
    if (!target) return;
    invalidate_layout(id);
    if (name == "itemHeight") target->item_height = value ? std::max(1.0, *value) : 28.0;
    else if (name == "overdraw") {
      target->overdraw = value ? static_cast<uint32_t>(std::max(0.0, *value)) : 2u;
    }
  }
  void set_event(uint64_t id, const std::string& kind, bool enabled) {
    Node* target = node(id);
    if (!target) return;
    if (enabled) target->events.insert(kind);
    else target->events.erase(kind);
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
          if (event.type == SDL_MOUSEBUTTONUP) {
            emit_pointer(env, event_callback, "click", event.button.x, event.button.y,
                         event.button.button);
          }
        } else if (event.type == SDL_MOUSEWHEEL) {
          int mouse_x = 0;
          int mouse_y = 0;
          SDL_GetMouseState(&mouse_x, &mouse_y);
          const double delta_y = -static_cast<double>(event.wheel.preciseY) * 48.0;
          const uint64_t target_id = find_scroll_target(root_id_, mouse_x, mouse_y);
          if (Node* target = node(target_id)) {
            add_dirty_box(target->box);
            target->scroll_y = std::clamp(target->scroll_y + delta_y, 0.0,
                                           std::max(0.0, target->content_height - target->box.h));
            dirty_nodes_.insert(target_id);
            dirty_ = true;
            emit_pointer(env, event_callback, "scroll", mouse_x, mouse_y, 0, delta_y);
          }
        }
      }
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

  Size natural_size(uint64_t id, double available_width) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end()) return {};
    const Node& n = it->second;
    if (n.handler->kind == ElementHandler::Kind::kText) {
      Size measured = measure_text(n.text, n.style.font_size);
      measured.w = std::max(measured.w, n.style.min_width);
      measured.h = std::max(measured.h, n.style.min_height);
      return measured;
    }

    if (n.handler->kind == ElementHandler::Kind::kVirtualList) {
      const double natural_h = n.children.size() * n.item_height + n.style.padding_y * 2.0;
      return {
          std::max(n.style.min_width, n.style.width.resolve(available_width, available_width)),
          std::max(n.style.min_height, n.style.height.resolve(natural_h, natural_h)),
      };
    }

    const double provisional_w = n.style.width.resolve(available_width, available_width);
    const double inner_w = std::max(0.0, provisional_w - n.style.padding_x * 2.0);
    double main = 0.0;
    double cross = 0.0;
    for (size_t i = 0; i < n.children.size(); ++i) {
      const Size child = natural_size(n.children[i], inner_w);
      if (n.style.row) {
        main += child.w;
        cross = std::max(cross, child.h);
      } else {
        main += child.h;
        cross = std::max(cross, child.w);
      }
    }
    if (n.children.size() > 1) main += n.style.gap * (n.children.size() - 1);
    Size result = n.style.row
                      ? Size{main + n.style.padding_x * 2.0,
                             cross + n.style.padding_y * 2.0}
                      : Size{cross + n.style.padding_x * 2.0,
                             main + n.style.padding_y * 2.0};
    result.w = n.style.width.resolve(available_width, result.w);
    result.h = n.style.height.resolve(0.0, result.h);
    result.w = std::max(result.w, n.style.min_width);
    result.h = std::max(result.h, n.style.min_height);
    return result;
  }

  void layout_node(uint64_t id, double x, double y, double available_w,
                   double available_h, double forced_w = -1.0,
                   double forced_h = -1.0) {
    Node* n = node(id);
    if (!n) return;
    const Size natural = natural_size(id, available_w);
    const double w = std::max(n->style.min_width,
                              n->style.width.resolve(available_w,
                                  forced_w >= 0.0 ? forced_w : natural.w));
    const double h = std::max(n->style.min_height,
                              n->style.height.resolve(available_h,
                                  forced_h >= 0.0 ? forced_h : natural.h));
    n->box = {x, y, std::max(0.0, w), std::max(0.0, h)};
    if (n->handler->kind == ElementHandler::Kind::kText || n->children.empty()) return;

    const double inner_x = x + n->style.padding_x;
    const double inner_y = y + n->style.padding_y;
    const double inner_w = std::max(0.0, w - n->style.padding_x * 2.0);
    const double inner_h = std::max(0.0, h - n->style.padding_y * 2.0);
    if (n->handler->kind == ElementHandler::Kind::kVirtualList) {
      n->content_height = n->children.size() * n->item_height + n->style.padding_y * 2.0;
      n->scroll_y = std::clamp(n->scroll_y, 0.0, std::max(0.0, n->content_height - h));
      const size_t first = static_cast<size_t>(std::floor(n->scroll_y / n->item_height));
      const size_t count = static_cast<size_t>(std::ceil(inner_h / n->item_height));
      n->visible_start = first > n->overdraw ? first - n->overdraw : 0;
      n->visible_end = std::min(n->children.size(), first + count + n->overdraw + 1);
      for (size_t i = n->visible_start; i < n->visible_end; ++i) {
        const double child_y = inner_y + i * n->item_height - n->scroll_y;
        layout_node(n->children[i], inner_x, child_y, inner_w, n->item_height,
                    inner_w, n->item_height);
      }
      return;
    }
    const double main_available = n->style.row ? inner_w : inner_h;
    double occupied = n->style.gap * std::max<size_t>(n->children.size(), 1) -
                      (n->children.empty() ? 0.0 : n->style.gap);
    double total_grow = 0.0;
    std::vector<Size> sizes;
    sizes.reserve(n->children.size());
    for (uint64_t child_id : n->children) {
      Node* child = node(child_id);
      Size size = natural_size(child_id, inner_w);
      if (child) {
        size.w = child->style.width.resolve(inner_w, size.w);
        size.h = child->style.height.resolve(inner_h, size.h);
        total_grow += child->style.flex_grow;
      }
      occupied += n->style.row ? size.w : size.h;
      sizes.push_back(size);
    }
    const double extra = std::max(0.0, main_available - occupied);
    n->content_height = n->style.row ? inner_h : occupied + n->style.padding_y * 2.0;
    n->scroll_y = std::clamp(n->scroll_y, 0.0, std::max(0.0, n->content_height - h));
    double cursor = n->style.row ? inner_x : inner_y - n->scroll_y;
    for (size_t i = 0; i < n->children.size(); ++i) {
      Node* child = node(n->children[i]);
      if (!child) continue;
      double child_w = sizes[i].w;
      double child_h = sizes[i].h;
      if (child->style.flex_grow > 0.0 && total_grow > 0.0) {
        const double share = extra * child->style.flex_grow / total_grow;
        if (n->style.row) child_w += share;
        else child_h += share;
      }
      if (n->style.row) {
        if (!child->style.height.set) child_h = inner_h;
        layout_node(child->id, cursor, inner_y, inner_w, inner_h, child_w, child_h);
        cursor += child_w + n->style.gap;
      } else {
        if (!child->style.width.set) child_w = inner_w;
        layout_node(child->id, inner_x, cursor, inner_w, inner_h, child_w, child_h);
        cursor += child_h + n->style.gap;
      }
    }
  }

  static bool intersects(const Box& box, const BLRectI& rect) {
    return box.x < rect.x + rect.w && rect.x < box.x + box.w &&
           box.y < rect.y + rect.h && rect.y < box.y + box.h;
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
    if (node_intersects && n->style.background) {
      BLRect rect(n->box.x, n->box.y, n->box.w, n->box.h);
      if (n->style.border_radius > 0.0) {
        context.fill_round_rect(rect, n->style.border_radius, n->style.border_radius,
                                BLRgba32(*n->style.background));
      } else {
        context.fill_rect(rect, BLRgba32(*n->style.background));
      }
    }
    if (node_intersects && n->handler->kind == ElementHandler::Kind::kText && !n->text.empty()) {
      BLFont font = make_font(font_size);
      const double baseline = n->box.y + font.metrics().ascent;
      context.fill_utf8_text(BLPoint(n->box.x, baseline), font, n->text.data(),
                             n->text.size(), BLRgba32(color));
    }
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
  std::vector<double> frame_samples_;
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
  if (auto value = property(env, object, "gap")) style.gap = number(env, value);
  if (auto value = property(env, object, "padding")) {
    style.padding_x = style.padding_y = number(env, value);
  }
  if (auto value = property(env, object, "paddingHorizontal")) style.padding_x = number(env, value);
  if (auto value = property(env, object, "paddingVertical")) style.padding_y = number(env, value);
  style.background = color(env, property(env, object, "backgroundColor"));
  style.color = color(env, property(env, object, "color"));
  if (auto value = property(env, object, "fontSize")) style.font_size = number(env, value, 16.0);
  if (auto value = property(env, object, "borderRadius")) style.border_radius = number(env, value);
  if (auto value = property(env, object, "visibility")) style.visible = string(env, value) != "hidden";
  if (auto value = property(env, object, "overflow")) {
    const std::string overflow = string(env, value);
    if (overflow == "scroll") style.overflow = Style::Overflow::kScroll;
    else if (overflow == "hidden") style.overflow = Style::Overflow::kHidden;
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
    napi_valuetype type;
    napi_typeof(env, values[2], &type);
    const std::optional<double> value = type == napi_number
                                            ? std::optional<double>(number(env, values[2]))
                                            : std::nullopt;
    renderer.set_custom_prop(id_arg(env, values[0]), string(env, values[1]), value);
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
      napi_value raw = at(3);
      napi_valuetype type;
      napi_typeof(env, raw, &type);
      renderer.set_custom_prop(id_arg(env, at(1)), string(env, at(2)),
                               type == napi_number
                                   ? std::optional<double>(number(env, raw))
                                   : std::nullopt);
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
      {"getStats", nullptr, get_stats, nullptr, nullptr, nullptr, napi_default, nullptr},
  };
  napi_define_properties(env, exports, sizeof(properties) / sizeof(properties[0]), properties);
  return exports;
}

}  // namespace

NAPI_MODULE(NODE_GYP_MODULE_NAME, module_init)
