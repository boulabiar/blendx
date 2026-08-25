#include "renderer_model.h"

#include <iostream>

using blendx::Style;

int main() {
  auto expect = [](bool condition, const char* message) {
    if (condition) return true;
    std::cerr << message << '\n';
    return false;
  };
  Style original;

  Style layout = original;
  layout.gap = 4.0;
  if (!expect(!original.same_layout(layout), "gap must invalidate layout") ||
      !expect(original.same_visual(layout), "gap must not invalidate paint")) return 1;

  Style dimension = original;
  dimension.width = {.set = true, .percent = true, .value = 50.0};
  if (!expect(!original.same_layout(dimension), "dimensions must invalidate layout")) return 1;

  Style visual = original;
  visual.opacity = 0.5;
  if (!expect(original.same_layout(visual), "opacity must not invalidate layout") ||
      !expect(!original.same_visual(visual), "opacity must invalidate paint")) return 1;

  Style stacking = original;
  stacking.z_index = 2.0;
  if (!expect(original.same_layout(stacking), "z-index must not invalidate layout") ||
      !expect(!original.same_visual(stacking), "z-index must invalidate paint")) return 1;
}
