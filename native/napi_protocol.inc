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
  if (auto value = property(env, object, "whiteSpace")) {
    const std::string white_space = string(env, value);
    if (white_space == "normal") style.white_space = Style::WhiteSpace::kNormal;
    else if (white_space == "pre") style.white_space = Style::WhiteSpace::kPre;
    else if (white_space == "preWrap") style.white_space = Style::WhiteSpace::kPreWrap;
  }
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
  if (auto value = property(env, object, "zIndex")) style.z_index = number(env, value);
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
  if (auto value = property(env, object, "layoutContain")) {
    style.layout_contain = boolean(env, value);
  }
  return style;
}

void apply_style_property(napi_env env, Style& style, uint32_t property_id, napi_value value) {
  switch (property_id) {
    case 0: style.height = dimension(env, value); break;
    case 1:
      style.padding_left = style.padding_right = style.padding_top =
          style.padding_bottom = number(env, value);
      break;
    case 2: style.gap = number(env, value); break;
    case 3: style.background = color(env, value); break;
    case 4: style.border_color = color(env, value); break;
    case 5: style.color = color(env, value); break;
    case 6: style.opacity = number(env, value, 1.0); break;
    case 7: style.border_radius = number(env, value); break;
    case 8: style.width = dimension(env, value); break;
    case 9: style.border_width = number(env, value); break;
    case 10: style.font_size = number(env, value, 16.0); break;
    case 11: style.line_height = number(env, value); break;
    case 12: style.flex_grow = number(env, value); break;
    case 13: style.flex_shrink = number(env, value, 1.0); break;
    case 14: style.layout_contain = boolean(env, value); break;
    default: break;
  }
}

void apply_style_patch(napi_env env, uint64_t id, napi_value operation,
                       uint32_t start, uint32_t length) {
  Style style;
  if (!renderer.copy_style(id, style)) return;
  for (uint32_t index = start; index + 1 < length; index += 2) {
    napi_value property_id;
    napi_value value;
    napi_get_element(env, operation, index, &property_id);
    napi_get_element(env, operation, index + 1, &value);
    apply_style_property(env, style, static_cast<uint32_t>(number(env, property_id)), value);
  }
  renderer.set_style(id, std::move(style));
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
  std::string font_path;
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

napi_value set_style_patch(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 2);
  if (values.size() != 2) return undefined(env);
  bool is_array = false;
  napi_is_array(env, values[1], &is_array);
  if (!is_array) return undefined(env);
  uint32_t length = 0;
  napi_get_array_length(env, values[1], &length);
  apply_style_patch(env, id_arg(env, values[0]), values[1], 0, length);
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
  const auto batch_started = std::chrono::steady_clock::now();
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
    } else if (kind == "stylePatch" && length >= 4) {
      apply_style_patch(env, id_arg(env, at(1)), operation, 2, length);
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
  renderer.record_batch_time(std::chrono::duration<double, std::milli>(
                                 std::chrono::steady_clock::now() - batch_started)
                                 .count());
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
  if (!values.empty()) renderer.focus_element(env, event_callback, id_arg(env, values[0]), true);
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

napi_value scroll_to_offset(napi_env env, napi_callback_info info) {
  auto values = args(env, info, 2);
  if (values.size() == 2) {
    renderer.scroll_to_offset(id_arg(env, values[0]), number(env, values[1]));
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

napi_value get_selected_text(napi_env env, napi_callback_info) {
  const std::string value = renderer.selected_text();
  napi_value result;
  napi_create_string_utf8(env, value.data(), value.size(), &result);
  return result;
}

napi_value get_accessibility_tree(napi_env env, napi_callback_info) {
  const std::vector<AccessibilityNode> nodes = renderer.accessibility_nodes();
  napi_value result;
  napi_create_array_with_length(env, nodes.size(), &result);
  for (size_t index = 0; index < nodes.size(); ++index) {
    const AccessibilityNode& node = nodes[index];
    napi_value item;
    napi_create_object(env, &item);
    auto set_text = [&](const char* name, const std::string& text) {
      napi_value value;
      napi_create_string_utf8(env, text.data(), text.size(), &value);
      napi_set_named_property(env, item, name, value);
    };
    auto set_value = [&](const char* name, double number) {
      napi_value value;
      napi_create_double(env, number, &value);
      napi_set_named_property(env, item, name, value);
    };
    auto set_boolean = [&](const char* name, bool state) {
      napi_value value;
      napi_get_boolean(env, state, &value);
      napi_set_named_property(env, item, name, value);
    };
    set_value("id", static_cast<double>(node.id));
    set_text("role", node.role);
    set_text("label", node.label);
    set_text("description", node.description);
    set_text("value", node.value);
    set_text("checked", node.checked);
    set_boolean("disabled", node.disabled);
    set_boolean("selected", node.selected);
    set_value("x", node.box.x);
    set_value("y", node.box.y);
    set_value("width", node.box.w);
    set_value("height", node.box.h);
    napi_set_element(env, result, static_cast<uint32_t>(index), item);
  }
  return result;
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
  set("batchTimeMs", renderer.batch_time_ms());
  set("bridgeTimeMs", 0.0);
  set("reactCommitTimeMs", 0.0);
  set("yogaTimeMs", renderer.yoga_time_ms());
  set("boxSyncTimeMs", renderer.box_sync_time_ms());
  set("specialLayoutTimeMs", renderer.special_layout_time_ms());
  set("paintTimeMs", renderer.paint_time_ms());
  set("presentTimeMs", renderer.present_time_ms());
  set("paintedPixels", static_cast<double>(renderer.painted_pixels()));
  set("paintedNodes", static_cast<double>(renderer.painted_nodes()));
  set("dirtyRectCount", renderer.dirty_rect_count());
  set("mutationsLastCommit", renderer.mutations_last_commit());
  set("frameP50Ms", renderer.frame_percentile(0.50));
  set("frameP95Ms", renderer.frame_percentile(0.95));
  set("frameP99Ms", renderer.frame_percentile(0.99));
  set("frameMaxMs", renderer.frame_percentile(1.0));
  set("framesOverBudget", static_cast<double>(renderer.frames_over_budget(1000.0 / 60.0)));
  set("framesOver120Budget", static_cast<double>(renderer.frames_over_budget(1000.0 / 120.0)));
  set("activeAnimations", static_cast<double>(renderer.active_animation_count()));
  set("threads", renderer.threads());
  return result;
}

napi_value next_frame_delay(napi_env env, napi_callback_info) {
  napi_value result;
  napi_create_double(env, renderer.next_frame_delay(), &result);
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
      {"setStylePatch", nullptr, set_style_patch, nullptr, nullptr, nullptr, napi_default, nullptr},
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
      {"scrollToOffset", nullptr, scroll_to_offset, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getElementBox", nullptr, get_element_box, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"captureScreenshot", nullptr, capture_screenshot, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSelectedText", nullptr, get_selected_text, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getAccessibilityTree", nullptr, get_accessibility_tree, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getStats", nullptr, get_stats, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"nextFrameDelay", nullptr, next_frame_delay, nullptr, nullptr, nullptr, napi_default, nullptr},
  };
  napi_define_properties(env, exports, sizeof(properties) / sizeof(properties[0]), properties);
  return exports;
}
