  Size natural_size(uint64_t id, double available_width) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end()) return {};
    const Node& n = it->second;
    if (n.handler->kind == ElementHandler::Kind::kText) {
      double wrap_width = n.style.width.resolve(available_width, available_width);
      if (n.style.max_width.set) wrap_width = std::min(wrap_width, n.style.max_width.resolve(available_width, wrap_width));
      const std::vector<std::string> lines = text_lines(n, std::max(1.0, wrap_width));
      Size measured{};
      for (const std::string& line : lines) measured.w = std::max(measured.w, measure_text(line, n.style.font_size).w);
      const double line_height = n.style.line_height > 0.0
                                     ? n.style.line_height
                                     : measure_text("Mg", n.style.font_size).h;
      measured.h = line_height * lines.size();
      if (n.style.white_space == Style::WhiteSpace::kNormal ||
          n.style.white_space == Style::WhiteSpace::kPreWrap) measured.w = std::min(measured.w, wrap_width);
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

