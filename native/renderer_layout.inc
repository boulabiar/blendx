  static bool yoga_measures(const Node& node) {
    return node.handler->kind != ElementHandler::Kind::kContainer &&
           node.handler->kind != ElementHandler::Kind::kAnchored;
  }

  static YGAlign yoga_align(Style::Align align) {
    switch (align) {
      case Style::Align::kCenter: return YGAlignCenter;
      case Style::Align::kEnd: return YGAlignFlexEnd;
      case Style::Align::kStretch: return YGAlignStretch;
      default: return YGAlignFlexStart;
    }
  }

  static YGJustify yoga_justify(Style::Justify justify) {
    switch (justify) {
      case Style::Justify::kCenter: return YGJustifyCenter;
      case Style::Justify::kEnd: return YGJustifyFlexEnd;
      case Style::Justify::kSpaceBetween: return YGJustifySpaceBetween;
      default: return YGJustifyFlexStart;
    }
  }

  static void set_yoga_dimension(YGNodeRef yoga, const Dimension& dimension,
                                 void (*points)(YGNodeRef, float),
                                 void (*percent)(YGNodeRef, float),
                                 void (*automatic)(YGNodeRef)) {
    if (!dimension.set) automatic(yoga);
    else if (dimension.percent) percent(yoga, static_cast<float>(dimension.value));
    else points(yoga, static_cast<float>(dimension.value));
  }

  static void set_yoga_max_dimension(YGNodeRef yoga, const Dimension& dimension,
                                     void (*points)(YGNodeRef, float),
                                     void (*percent)(YGNodeRef, float)) {
    if (!dimension.set) points(yoga, YGUndefined);
    else if (dimension.percent) percent(yoga, static_cast<float>(dimension.value));
    else points(yoga, static_cast<float>(dimension.value));
  }

  void configure_yoga_node(Node& node) {
    if (!node.yoga) return;
    const Style& style = node.style;
    YGNodeStyleSetBoxSizing(node.yoga, YGBoxSizingBorderBox);
    YGNodeStyleSetFlexDirection(node.yoga,
                                style.row ? YGFlexDirectionRow : YGFlexDirectionColumn);
    YGNodeStyleSetFlexGrow(node.yoga, static_cast<float>(style.flex_grow));
    YGNodeStyleSetFlexShrink(node.yoga, static_cast<float>(style.flex_shrink));
    YGNodeStyleSetGap(node.yoga, YGGutterAll, static_cast<float>(style.gap));
    YGNodeStyleSetAlignItems(node.yoga, yoga_align(style.align_items));
    YGNodeStyleSetJustifyContent(node.yoga, yoga_justify(style.justify));
    YGNodeStyleSetOverflow(node.yoga,
                           style.overflow == Style::Overflow::kScroll ? YGOverflowScroll
                           : style.overflow == Style::Overflow::kHidden ? YGOverflowHidden
                                                                       : YGOverflowVisible);

    const bool detached_overlay = style.position == Style::Position::kFixed ||
                                  node.handler->kind == ElementHandler::Kind::kAnchored;
    YGNodeStyleSetPositionType(node.yoga,
        style.position == Style::Position::kAbsolute && !detached_overlay
            ? YGPositionTypeAbsolute : YGPositionTypeRelative);
    YGNodeStyleSetPosition(node.yoga, YGEdgeLeft,
                           style.left ? static_cast<float>(*style.left) : YGUndefined);
    YGNodeStyleSetPosition(node.yoga, YGEdgeRight,
                           style.right ? static_cast<float>(*style.right) : YGUndefined);
    YGNodeStyleSetPosition(node.yoga, YGEdgeTop,
                           style.top ? static_cast<float>(*style.top) : YGUndefined);
    YGNodeStyleSetPosition(node.yoga, YGEdgeBottom,
                           style.bottom ? static_cast<float>(*style.bottom) : YGUndefined);

    set_yoga_dimension(node.yoga, style.width, YGNodeStyleSetWidth,
                       YGNodeStyleSetWidthPercent, YGNodeStyleSetWidthAuto);
    set_yoga_dimension(node.yoga, style.height, YGNodeStyleSetHeight,
                       YGNodeStyleSetHeightPercent, YGNodeStyleSetHeightAuto);
    YGNodeStyleSetMinWidth(node.yoga, static_cast<float>(style.min_width));
    YGNodeStyleSetMinHeight(node.yoga, static_cast<float>(style.min_height));
    set_yoga_max_dimension(node.yoga, style.max_width, YGNodeStyleSetMaxWidth,
                           YGNodeStyleSetMaxWidthPercent);
    set_yoga_max_dimension(node.yoga, style.max_height, YGNodeStyleSetMaxHeight,
                           YGNodeStyleSetMaxHeightPercent);

    YGNodeStyleSetPadding(node.yoga, YGEdgeLeft, static_cast<float>(style.padding_left));
    YGNodeStyleSetPadding(node.yoga, YGEdgeRight, static_cast<float>(style.padding_right));
    YGNodeStyleSetPadding(node.yoga, YGEdgeTop, static_cast<float>(style.padding_top));
    YGNodeStyleSetPadding(node.yoga, YGEdgeBottom, static_cast<float>(style.padding_bottom));
    YGNodeStyleSetMargin(node.yoga, YGEdgeLeft, static_cast<float>(style.margin_left));
    YGNodeStyleSetMargin(node.yoga, YGEdgeRight, static_cast<float>(style.margin_right));
    YGNodeStyleSetMargin(node.yoga, YGEdgeTop, static_cast<float>(style.margin_top));
    YGNodeStyleSetMargin(node.yoga, YGEdgeBottom, static_cast<float>(style.margin_bottom));
    YGNodeSetMeasureFunc(node.yoga, yoga_measures(node) ? &Renderer::yoga_measure : nullptr);
  }

  bool yoga_child_participates(const Node& parent, const Node& child) const {
    return parent.handler->kind != ElementHandler::Kind::kVirtualList &&
           child.handler->kind != ElementHandler::Kind::kAnchored &&
           child.style.position != Style::Position::kFixed;
  }

  void sync_yoga_children(uint64_t parent_id) {
    Node* parent = node(parent_id);
    if (!parent || !parent->yoga) return;
    YGNodeRemoveAllChildren(parent->yoga);
    if (yoga_measures(*parent)) return;
    size_t index = 0;
    for (uint64_t child_id : parent->children) {
      Node* child = node(child_id);
      if (child && child->yoga && yoga_child_participates(*parent, *child)) {
        YGNodeInsertChild(parent->yoga, child->yoga, index++);
      }
    }
  }

  void mark_yoga_measure_dirty(Node& node) {
    if (node.yoga && YGNodeHasMeasureFunc(node.yoga)) YGNodeMarkDirty(node.yoga);
  }

  Size intrinsic_size(const Node& node, double available_width) const {
    const double width = std::max(0.0, available_width);
    if (node.handler->kind == ElementHandler::Kind::kText) {
      const double wrap_width = std::max(1.0, width);
      const std::vector<std::string> lines = text_lines(node, wrap_width);
      Size measured{};
      for (const std::string& line : lines) {
        measured.w = std::max(measured.w, measure_text(line, node.style.font_size).w);
      }
      const double line_height = node.style.line_height > 0.0
                                     ? node.style.line_height
                                     : measure_text("Mg", node.style.font_size).h;
      measured.h = line_height * lines.size();
      if (node.style.white_space == Style::WhiteSpace::kNormal ||
          node.style.white_space == Style::WhiteSpace::kPreWrap) {
        measured.w = std::min(measured.w, wrap_width);
      }
      return measured;
    }
    if (node.handler->kind == ElementHandler::Kind::kVirtualList) {
      return {width, node.children.size() * node.item_height};
    }
    if (node.handler->kind == ElementHandler::Kind::kImage) {
      if (const BLImage* image = image_for(string_prop(node, "src"))) {
        return {static_cast<double>(image->width()), static_cast<double>(image->height())};
      }
      return {100.0, 100.0};
    }
    if (node.handler->kind == ElementHandler::Kind::kSvg) return {24.0, 24.0};
    if (node.handler->kind == ElementHandler::Kind::kCanvas) return {300.0, 150.0};
    if (node.handler->kind == ElementHandler::Kind::kSeparator) return {width, 1.0};
    if (node.handler->kind == ElementHandler::Kind::kProgress) return {120.0, 8.0};
    if (node.handler->kind == ElementHandler::Kind::kInput) {
      const double rows = std::max(1.0, number_prop(node, "minRows", node.type == "textarea" ? 3.0 : 1.0));
      const double line = node.style.line_height > 0.0 ? node.style.line_height : node.style.font_size * 1.35;
      return {std::min(width, 320.0), rows * line};
    }
    if (node.handler->kind == ElementHandler::Kind::kMarkdown ||
        node.handler->kind == ElementHandler::Kind::kCode ||
        node.handler->kind == ElementHandler::Kind::kDiff) {
      const char* prop = node.handler->kind == ElementHandler::Kind::kMarkdown ? "source" :
                         node.handler->kind == ElementHandler::Kind::kCode ? "code" : "patch";
      const double line = node.style.line_height > 0.0 ? node.style.line_height : node.style.font_size * 1.45;
      return {width, line_count(string_prop(node, prop)) * line};
    }
    return {};
  }

  static YGSize yoga_measure(YGNodeConstRef yoga, float width, YGMeasureMode width_mode,
                             float height, YGMeasureMode height_mode) {
    auto* context = static_cast<YogaNodeContext*>(YGNodeGetContext(yoga));
    if (!context || !context->renderer) return {0.0f, 0.0f};
    auto* renderer = static_cast<Renderer*>(context->renderer);
    Node* node = renderer->node(context->id);
    if (!node) return {0.0f, 0.0f};
    const double available_width = width_mode == YGMeasureModeUndefined
                                       ? 1000000.0 : std::max(0.0f, width);
    Size measured = renderer->intrinsic_size(*node, available_width);
    if (width_mode == YGMeasureModeExactly) measured.w = width;
    else if (width_mode == YGMeasureModeAtMost) measured.w = std::min(measured.w, static_cast<double>(width));
    if (height_mode == YGMeasureModeExactly) measured.h = height;
    else if (height_mode == YGMeasureModeAtMost) measured.h = std::min(measured.h, static_cast<double>(height));
    return {static_cast<float>(std::max(0.0, measured.w)),
            static_cast<float>(std::max(0.0, measured.h))};
  }

  void apply_yoga_boxes(uint64_t id, double origin_x, double origin_y, bool detached_root = false) {
    Node* current = node(id);
    if (!current || !current->yoga) return;
    const double left = detached_root ? 0.0 : YGNodeLayoutGetLeft(current->yoga);
    const double top = detached_root ? 0.0 : YGNodeLayoutGetTop(current->yoga);
    current->box = {origin_x + left, origin_y + top,
                    YGNodeLayoutGetWidth(current->yoga), YGNodeLayoutGetHeight(current->yoga)};
    double content_bottom = current->style.padding_top;
    for (uint64_t child_id : current->children) {
      Node* child = node(child_id);
      if (!child || !yoga_child_participates(*current, *child)) continue;
      content_bottom = std::max(content_bottom,
          static_cast<double>(YGNodeLayoutGetTop(child->yoga) + YGNodeLayoutGetHeight(child->yoga)));
    }
    current->content_height = content_bottom + current->style.padding_bottom;
    const double max_scroll = std::max(0.0, current->content_height - current->box.h);
    current->scroll_y = std::clamp(current->scroll_y, 0.0, max_scroll);
    current->scroll_target_y = std::clamp(current->scroll_target_y, 0.0, max_scroll);
    const double child_origin_y = current->box.y - current->scroll_y;
    for (uint64_t child_id : current->children) {
      Node* child = node(child_id);
      if (child && yoga_child_participates(*current, *child)) {
        apply_yoga_boxes(child_id, current->box.x, child_origin_y);
      }
    }
  }

  void calculate_forced_subtree(uint64_t id, double x, double y, double width, double height) {
    Node* child = node(id);
    if (!child || !child->yoga) return;
    YGNodeStyleSetWidth(child->yoga, static_cast<float>(std::max(0.0, width)));
    YGNodeStyleSetHeight(child->yoga, static_cast<float>(std::max(0.0, height)));
    YGNodeCalculateLayout(child->yoga, static_cast<float>(width), static_cast<float>(height), YGDirectionLTR);
    apply_yoga_boxes(id, x, y, true);
    layout_special_descendants(id);
    configure_yoga_node(*child);
  }

  void layout_virtual_children(Node& list) {
    list.content_height = list.children.size() * list.item_height +
                          list.style.padding_top + list.style.padding_bottom;
    const double inner_h = std::max(0.0, list.box.h - list.style.padding_top - list.style.padding_bottom);
    const double inner_w = std::max(0.0, list.box.w - list.style.padding_left - list.style.padding_right);
    const double max_scroll = std::max(0.0, list.content_height - list.box.h);
    if (bool_prop(list, "followTail") && list.children.size() != list.last_child_count) {
      list.scroll_y = max_scroll;
      list.scroll_target_y = max_scroll;
    }
    list.last_child_count = list.children.size();
    list.scroll_y = std::clamp(list.scroll_y, 0.0, max_scroll);
    list.scroll_target_y = std::clamp(list.scroll_target_y, 0.0, max_scroll);
    const size_t first = static_cast<size_t>(std::floor(list.scroll_y / list.item_height));
    const size_t count = static_cast<size_t>(std::ceil(inner_h / list.item_height));
    list.visible_start = first > list.overdraw ? first - list.overdraw : 0;
    list.visible_end = std::min(list.children.size(), first + count + list.overdraw + 1);
    const double bottom_alignment = string_prop(list, "alignment") == "bottom"
        ? std::max(0.0, inner_h - list.children.size() * list.item_height) : 0.0;
    for (size_t i = list.visible_start; i < list.visible_end; ++i) {
      const double child_y = list.box.y + list.style.padding_top + bottom_alignment +
                             i * list.item_height - list.scroll_y;
      calculate_forced_subtree(list.children[i], list.box.x + list.style.padding_left,
                               child_y, inner_w, list.item_height);
    }
  }

  void layout_overlay(Node& child, const Node& parent) {
    const bool fixed = child.style.position == Style::Position::kFixed;
    const double area_x = fixed ? 0.0 : parent.box.x + parent.style.padding_left;
    const double area_y = fixed ? 0.0 : parent.box.y + parent.style.padding_top;
    const double area_w = fixed ? static_cast<double>(width_)
                                : std::max(0.0, parent.box.w - parent.style.padding_left - parent.style.padding_right);
    const double area_h = fixed ? static_cast<double>(height_)
                                : std::max(0.0, parent.box.h - parent.style.padding_top - parent.style.padding_bottom);
    // An unconstrained detached root otherwise stretches to the entire
    // available height, which makes an auto-sized popover viewport-sized.
    const float layout_w = child.style.width.set || child.style.max_width.percent
                               ? static_cast<float>(area_w) : YGUndefined;
    const float layout_h = child.style.height.set || child.style.max_height.percent
                               ? static_cast<float>(area_h) : YGUndefined;
    YGNodeCalculateLayout(child.yoga, layout_w, layout_h, YGDirectionLTR);
    double child_w = YGNodeLayoutGetWidth(child.yoga);
    double child_h = YGNodeLayoutGetHeight(child.yoga);
    if (!child.style.width.set && child.style.left && child.style.right) {
      child_w = std::max(0.0, area_w - *child.style.left - *child.style.right);
    }
    if (!child.style.height.set && child.style.top && child.style.bottom) {
      child_h = std::max(0.0, area_h - *child.style.top - *child.style.bottom);
    }
    double x = area_x + child.style.left.value_or(0.0);
    double y = area_y + child.style.top.value_or(0.0);
    if (!child.style.left && child.style.right) x = area_x + area_w - *child.style.right - child_w;
    if (!child.style.top && child.style.bottom) y = area_y + area_h - *child.style.bottom - child_h;

    if (child.handler->kind == ElementHandler::Kind::kAnchored) {
      BLPoint anchor{};
      bool found = false;
      if (const double* anchor_id = prop_as<double>(child, "anchorId")) {
        if (Node* target = node(static_cast<uint64_t>(*anchor_id))) {
          const std::string side = string_prop(child, "side", "bottom");
          const std::string align = string_prop(child, "align", "start");
          anchor.x = align == "center" ? target->box.x + target->box.w * 0.5
                     : align == "end" ? target->box.x + target->box.w : target->box.x;
          anchor.y = side == "top" ? target->box.y
                     : side == "bottom" ? target->box.y + target->box.h
                     : align == "center" ? target->box.y + target->box.h * 0.5
                     : align == "end" ? target->box.y + target->box.h : target->box.y;
          if (side == "left") anchor.x = target->box.x;
          else if (side == "right") anchor.x = target->box.x + target->box.w;
          found = true;
        }
      }
      if (!found) {
        if (const BLPoint* position = prop_as<BLPoint>(child, "position")) {
          anchor = *position;
          found = true;
        }
      }
      if (found) {
        x = anchor.x;
        y = anchor.y;
        const std::string side = string_prop(child, "side", "bottom");
        const std::string align = string_prop(child, "align", "start");
        const double gap = number_prop(child, "anchorGap", 4.0);
        if (side == "top" || side == "bottom") {
          if (align == "center") x -= child_w * 0.5;
          else if (align == "end") x -= child_w;
          y += side == "top" ? -(child_h + gap) : gap;
        } else {
          if (align == "center") y -= child_h * 0.5;
          else if (align == "end") y -= child_h;
          x += side == "left" ? -(child_w + gap) : gap;
        }
        if (const BLPoint* offset = prop_as<BLPoint>(child, "offset")) {
          x += offset->x;
          y += offset->y;
        }
        x = std::clamp(x, 4.0, std::max(4.0, static_cast<double>(width_) - child_w - 4.0));
        y = std::clamp(y, 4.0, std::max(4.0, static_cast<double>(height_) - child_h - 4.0));
      }
    }
    calculate_forced_subtree(child.id, x, y, child_w, child_h);
  }

  void layout_special_descendants(uint64_t id) {
    Node* parent = node(id);
    if (!parent) return;
    if (parent->handler->kind == ElementHandler::Kind::kVirtualList) {
      layout_virtual_children(*parent);
      return;
    }
    const std::vector<uint64_t> children = parent->children;
    for (uint64_t child_id : children) {
      Node* child = node(child_id);
      if (!child) continue;
      if (!yoga_child_participates(*parent, *child)) layout_overlay(*child, *parent);
      else layout_special_descendants(child_id);
    }
  }

  void layout_tree() {
    Node* root = node(root_id_);
    if (!root || !root->yoga) return;
    YGNodeStyleSetWidth(root->yoga, static_cast<float>(width_));
    YGNodeStyleSetHeight(root->yoga, static_cast<float>(height_));
    YGNodeCalculateLayout(root->yoga, static_cast<float>(width_), static_cast<float>(height_), YGDirectionLTR);
    apply_yoga_boxes(root_id_, 0.0, 0.0, true);
    layout_special_descendants(root_id_);
  }

  static bool intersects(const Box& box, const BLRectI& rect) {
    return box.x < rect.x + rect.w && rect.x < box.x + box.w &&
           box.y < rect.y + rect.h && rect.y < box.y + box.h;
  }
