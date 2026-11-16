#pragma once

// Builds the launcher's box art from the player's own ROM: on first launch
// (no assets/img/boxart.tga beside the exe), TempleFrog runs itself headless
// to the title screen and converts that frame to a TGA. Nothing derived from
// the game ships with TempleFrog; the art exists only on the player's machine.
void templefrog_ensure_boxart();
