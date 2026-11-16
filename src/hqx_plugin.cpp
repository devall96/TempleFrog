// TempleFrog's trusted HQx presentation plugin. The "Pixel smoothing" feature
// (mods/preloaded/packages/templefrog.presentation.hqx) offers HQ2x, HQ3x and
// HQ4x through gbarecomp's replaceable HQx library. Presentation only: the
// guest raster, saves and save states are unchanged.
#include <cstring>

#include "hq2x_filter.h"
#include "mod_runtime.h"

namespace {

constexpr const char* kPackage = "templefrog.presentation.hqx";
constexpr const char* kFeature = "hqx";

void reset_hqx() { gbarecomp::set_hqx_scale(0); }

void activate_hqx() {
    const char* v = gba_mod_option_value(kPackage, kFeature, "scale");
    unsigned scale = 2;
    if (v && std::strcmp(v, "hq3x") == 0) scale = 3;
    else if (v && std::strcmp(v, "hq4x") == 0) scale = 4;
    gbarecomp::set_hqx_scale(scale);
}

}  // namespace

GBA_MOD_CONSTRUCTOR(register_templefrog_hqx) {
    (void)gba_mod_register_reset_callback(reset_hqx);
    (void)gba_mod_register_activation_plugin("templefrog.presentation.hqx", activate_hqx);
}
