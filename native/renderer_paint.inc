  void draw_text(BLContext& context, const std::string& text, double x, double baseline,
                 double size, uint32_t color) const {
    if (text.empty()) return;
    BLFont font = make_font(size);
    context.fill_utf8_text(BLPoint(x, baseline), font, text.data(), text.size(), BLRgba32(color));
  }

  double draw_markdown_inline(BLContext& context, const std::string& source, double x,
                              double baseline, double size, uint32_t color) const {
    size_t at = 0;
    while (at < source.size()) {
      if (source.compare(at, 2, "**") == 0) {
        const size_t end = source.find("**", at + 2);
        if (end != std::string::npos) {
          const std::string run = source.substr(at + 2, end - at - 2);
          draw_text(context, run, x, baseline, size * 1.02, 0xFFFFFFFFu);
          x += measure_text(run, size * 1.02).w;
          at = end + 2;
          continue;
        }
      }
      if (source[at] == '`') {
        const size_t end = source.find('`', at + 1);
        if (end != std::string::npos) {
          const std::string run = source.substr(at + 1, end - at - 1);
          const double width = measure_text(run, size * 0.92).w + 6.0;
          context.fill_round_rect(BLRect(x, baseline - size, width, size * 1.3), 3.0, 3.0,
                                  BLRgba32(0xFF273142u));
          draw_text(context, run, x + 3.0, baseline, size * 0.92, 0xFFF0ABFCu);
          x += width;
          at = end + 1;
          continue;
        }
      }
      if (source[at] == '[') {
        const size_t label_end = source.find(']', at + 1);
        const size_t target_end = label_end == std::string::npos ? std::string::npos
                                  : source.find(')', label_end + 2);
        if (label_end != std::string::npos && label_end + 1 < source.size() &&
            source[label_end + 1] == '(' && target_end != std::string::npos) {
          const std::string run = source.substr(at + 1, label_end - at - 1);
          draw_text(context, run, x, baseline, size, 0xFF60A5FAu);
          x += measure_text(run, size).w;
          at = target_end + 1;
          continue;
        }
      }
      size_t end = at + 1;
      while (end < source.size() && source.compare(end, 2, "**") != 0 &&
             source[end] != '`' && source[end] != '[') ++end;
      const std::string run = source.substr(at, end - at);
      draw_text(context, run, x, baseline, size, color);
      x += measure_text(run, size).w;
      at = end;
    }
    return x;
  }

  void draw_highlighted_code(BLContext& context, const std::string& source, double x,
                             double baseline, double size, uint32_t color) const {
    static const std::unordered_set<std::string> keywords = {
        "as", "async", "await", "break", "case", "catch", "class", "const", "continue",
        "default", "do", "else", "export", "extends", "false", "finally", "for", "from",
        "function", "if", "import", "in", "interface", "let", "new", "null", "return",
        "switch", "throw", "true", "try", "type", "typeof", "undefined", "while"};
    size_t at = 0;
    while (at < source.size()) {
      size_t end = at + 1;
      uint32_t run_color = color;
      if (source.compare(at, 2, "//") == 0) {
        end = source.size();
        run_color = 0xFF718096u;
      } else if (source[at] == '\'' || source[at] == '"' || source[at] == '`') {
        const char quote = source[at];
        while (end < source.size()) {
          if (source[end] == quote && source[end - 1] != '\\') { ++end; break; }
          ++end;
        }
        run_color = 0xFF86EFACu;
      } else if (std::isdigit(static_cast<unsigned char>(source[at]))) {
        while (end < source.size() && (std::isdigit(static_cast<unsigned char>(source[end])) || source[end] == '.')) ++end;
        run_color = 0xFFFBBF24u;
      } else if (std::isalpha(static_cast<unsigned char>(source[at])) || source[at] == '_') {
        while (end < source.size() && (std::isalnum(static_cast<unsigned char>(source[end])) || source[end] == '_')) ++end;
        if (keywords.count(source.substr(at, end - at))) run_color = 0xFFC4B5FDu;
      } else {
        while (end < source.size() && std::isspace(static_cast<unsigned char>(source[end]))) ++end;
      }
      const std::string run = source.substr(at, end - at);
      draw_text(context, run, x, baseline, size, run_color);
      x += measure_text(run, size).w;
      at = end;
    }
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
      if (markdown) draw_markdown_inline(context, line, x, y + line_height * 0.76, line_size, line_color);
      else if (!diff) draw_highlighted_code(context, line, x, y + line_height * 0.76, line_size, line_color);
      else draw_text(context, line, x, y + line_height * 0.76, line_size, line_color);
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
      std::string display_value = value;
      if (bool_prop(n, "password")) {
        display_value.clear();
        for (size_t at = 0; at < value.size(); at = next_utf8(value, at)) display_value.push_back('*');
      }
      const std::string display = value.empty() ? string_prop(n, "placeholder") : display_value;
      const uint32_t display_color = value.empty() ? 0xFF64748Bu : color;
      const double line = n.style.line_height > 0.0 ? n.style.line_height : font_size * 1.35;
      std::istringstream stream(display);
      std::string text;
      const bool multiline = n.type == "textarea";
      const double content_height = std::max(0.0, n.box.h - n.style.padding_top - n.style.padding_bottom);
      double y = n.box.y + n.style.padding_top;
      if (!multiline) y += std::max(0.0, (content_height - line) * 0.5);
      double caret_y = y;
      size_t line_start = 0;
      while (std::getline(stream, text)) {
        caret_y = y;
        if (!value.empty() && n.selection_start != n.selection_end) {
          const size_t selection_start = std::min(n.selection_start, n.selection_end);
          const size_t selection_end = std::max(n.selection_start, n.selection_end);
          const size_t line_end = line_start + text.size();
          const size_t selected_start = std::max(selection_start, line_start);
          const size_t selected_end = std::min(selection_end, line_end);
          if (selected_end > selected_start) {
            const double before = measure_text(text.substr(0, selected_start - line_start), font_size).w;
            const double selected = measure_text(text.substr(selected_start - line_start,
                                                              selected_end - selected_start), font_size).w;
            context.fill_rect(BLRect(n.box.x + n.style.padding_left + before, y,
                                     std::max(1.0, selected), line), BLRgba32(0x8060A5FAu));
          }
        }
        draw_text(context, text, n.box.x + n.style.padding_left, y + line * 0.78, font_size, display_color);
        y += line;
        line_start += text.size() + 1;
        if (y >= n.box.y + n.box.h) break;
      }
      if (n.id == focused_id_ && !bool_prop(n, "readOnly")) {
        const size_t caret = std::min(n.selection_end, value.size());
        const size_t marker = caret == 0 ? std::string::npos : value.rfind('\n', caret - 1);
        const size_t caret_line_start = marker == std::string::npos ? 0 : marker + 1;
        const size_t caret_line = static_cast<size_t>(std::count(value.begin(), value.begin() + static_cast<std::ptrdiff_t>(caret), '\n'));
        caret_y = n.box.y + n.style.padding_top + caret_line * line;
        if (!multiline) caret_y += std::max(0.0, (content_height - line) * 0.5);
        const Size measured = measure_text(value.substr(caret_line_start, caret - caret_line_start), font_size);
        const double caret_x = std::min(n.box.x + n.box.w - 3.0, n.box.x + n.style.padding_left + measured.w + 1.0);
        context.fill_rect(BLRect(caret_x, caret_y + 2.0, 1.5, line - 4.0), BLRgba32(color));
        if (!n.composition.empty()) {
          draw_text(context, n.composition, caret_x + 2.0, caret_y + line * 0.78, font_size, color);
          const double composition_width = measure_text(n.composition, font_size).w;
          context.fill_rect(BLRect(caret_x + 2.0, caret_y + line - 2.0,
                                   composition_width, 1.0), BLRgba32(color));
        }
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
    if (damage && !intersects(clips_children ? n->box : n->paint_bounds, *damage)) return;
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
      const double line_height = n->style.line_height > 0.0
                                     ? n->style.line_height
                                     : font.metrics().ascent + font.metrics().descent + font.metrics().line_gap;
      double baseline = n->box.y + font.metrics().ascent;
      size_t search_from = 0;
      for (const std::string& line : text_lines(*n, std::max(1.0, n->box.w))) {
        const size_t found = n->text.find(line, search_from);
        const size_t source_start = found == std::string::npos ? search_from : found;
        const size_t source_end = source_start + line.size();
        if (n->id == selected_text_id_ && text_selection_anchor_ != text_selection_focus_) {
          const size_t selection_start = std::min(text_selection_anchor_, text_selection_focus_);
          const size_t selection_end = std::max(text_selection_anchor_, text_selection_focus_);
          const size_t selected_start = std::max(selection_start, source_start);
          const size_t selected_end = std::min(selection_end, source_end);
          if (selected_end > selected_start) {
            const double before = measure_text(line.substr(0, selected_start - source_start), font_size).w;
            const double selected = measure_text(line.substr(selected_start - source_start,
                                                               selected_end - selected_start), font_size).w;
            context.fill_rect(BLRect(n->box.x + before, baseline - font.metrics().ascent,
                                     std::max(1.0, selected), line_height), BLRgba32(0x8060A5FAu));
          }
        }
        context.fill_utf8_text(BLPoint(n->box.x, baseline), font, line.data(),
                               line.size(), BLRgba32(color));
        search_from = source_end;
        baseline += line_height;
        if (baseline > n->box.y + n->box.h + line_height) break;
      }
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
      const bool layered = std::any_of(n->children.begin(), n->children.end(), [this](uint64_t child) {
        return nodes_.at(child).style.z_index != 0.0;
      });
      if (layered) {
        std::vector<uint64_t> paint_order = n->children;
        std::stable_sort(paint_order.begin(), paint_order.end(), [this](uint64_t a, uint64_t b) {
          return nodes_.at(a).style.z_index < nodes_.at(b).style.z_index;
        });
        for (uint64_t child : paint_order) paint_node(context, child, color, font_size, damage);
      } else {
        for (uint64_t child : n->children) paint_node(context, child, color, font_size, damage);
      }
    }
    if (n->style.overflow == Style::Overflow::kScroll && n->content_height > n->box.h + 0.5) {
      const double track_height = std::max(0.0, n->box.h - 8.0);
      const double thumb_height = std::max(24.0, track_height * n->box.h / n->content_height);
      const double max_scroll = std::max(1.0, n->content_height - n->box.h);
      const double travel = std::max(0.0, track_height - thumb_height);
      const double thumb_y = n->box.y + 4.0 + travel * n->scroll_y / max_scroll;
      context.fill_round_rect(BLRect(n->box.x + n->box.w - 6.0, thumb_y, 3.0, thumb_height),
                              1.5, 1.5, BLRgba32(0x805E6A7Eu));
    }
    if (clips_children) context.restore();
    context.restore();
  }
