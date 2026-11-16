# Menu design research: the house style, and a TempleFrog theme

Read-only research for TempleFrog's pre-boot launcher and in-game overlay. Three of the user's projects were studied:
SuperWonders, Savage Wind and Slate Companion. Colours are converted to hex from the source floats and `IM_COL32`
values. Paths are relative to the folder that holds the projects named below.

Screenshots viewed:

- SuperWonders: `SMW\ui-options\A-nunito-wonder-night-video.png` (overlay, Video tab) and
  `SMW\runs-linux\launcher_tour_devbuild.jpg` (all launcher pages plus the setup card).
- Slate Companion: `SlateCompanion\docs\images\launch.png`, `home.png` and `welcome.png`.
- Savage Wind: only `SavageWindRecomp\docs\media\banner.png` is local. Its README screenshots are on imgur, and no
  local capture of its menus exists, so its section is built from code. The Savage Wind settings view in Slate's
  `launch.png` and the SuperWonders overlay (a port of Savage Wind's layout) show what that layout looks like.
- TempleFrog: `captures\boot\noinput_900.png` and `fresh_masher_3600.png`.

---

## 1. Per-project findings

### 1.1 SuperWonders (Super Mario Bros. Wonder recomp)

**Toolkit and files.** Dear ImGui, drawn on the game's own Vulkan presenter.

- Theme: `SMW\SuperWonders-ui\runtime\src\overlay\overlay_ui.h` and `overlay_ui.cpp`. `struct Theme`, the
  `kTheme` "Wonder Night" palette, `apply_style(height)`, `load_fonts()`, and the animation helpers `approach`,
  `eased` and `smoothstep`. Also `badge` and `hints` for the prompt bar, and `notify` for toasts.
- Overlay menu: `overlay_menu.cpp` (`draw_menu`) and `option_rows.cpp` (ported from Savage Wind).
- Overlay backdrop and reveal: `overlay.cpp`, around lines 385-397.
- Compile screen: `overlay_progress.cpp`.
- Launcher: `SMW\SuperWonders-launcher\runtime\src\launcher\launcher.cpp`. It has `backdrop()`, `progress_card()`,
  and the screen loop around lines 693-880. It reuses the overlay's theme and rows.
- Commit that defines the look: `592463d`, "Overlay overhaul: its own look, Savage Wind's layout and motion, a compile
  screen". The user chose this palette and font from three options; `SMW\ui-options\` holds the
  A/B/C option shots.

**Palette ("Wonder Night", `overlay_ui.cpp` kTheme)**

| Role | Hex |
|---|---|
| Backdrop top / bottom (launcher, compile screen) | `#0A081E` |
| Backdrop middle | `#2C1858` |
| Panel (menu window) | `#161236` at 95% alpha (242) |
| Card (popups, notices, compile card) | `#1C1642` at 98% |
| Accent: focus border, title, tab underline, bars, top rule | `#FF5EA8` hot pink |
| Accent 2: section titles, second highlight, backdrop glow | `#56D6FF` sky cyan |
| Text | `#FAF8FF` |
| Text dim (values, unselected nav) | `#D0CAE6` |
| Text off (disabled, notes) | `#746C8E` |
| Good / warn / bad (metrics, status dot) | `#6EE88C` / `#FFCE48` / `#FF6070` |
| Frame and button fills | panel mixed toward accent2 by 12-14% |
| Hover / active | panel mixed toward accent2 by 22% / 34% |
| Separator / scrollbar | white at 10% / 14% |

**Fonts.**

- Nunito SemiBold for the body and Nunito ExtraBold for titles, both SIL OFL. They live in
  `third_party/fonts/Nunito`, are subset to Latin-1 (about 29 KB each), and are embedded with `#embed`.
- Baked at 40 px (body) and 72 px (title), then drawn smaller through `FontGlobalScale`. The body is 29 px at 1080p,
  which is Savage Wind's size.
- The menu title is drawn at 0.74 of the title font, in the accent colour, in title case ("Quick Settings").
- The launcher wordmark "SUPERWONDERS" is the title font at 1.25x, all capitals, in the accent colour.
- Row labels and section titles are all capitals. Values are written normally.
- System fonts (Segoe UI, Noto Sans, DejaVu) are the fallback.

**Layout: overlay.**

- One full-height window, `min(92% of width, 1500 px at 1080p)` wide, with a 22 px margin top and bottom.
- A 3 px accent rule runs along the top edge, inset 14 px.
- Top row: the title on the left, a close X on the right.
- Tabs: Video, Audio, Controls, Overlay, System. They are transparent; the selected tab gets white at 7%, and an
  accent underline glides under it.
- Each tab has two panes.
  - Left, about 48% wide: the option rows. Each row has an uppercase label on the left and `◀ value ▶` on the right.
    A value changed from its default is drawn brighter. Rows that belong to the row above are indented.
  - Right: the selected or hovered row's description in large dim text. Notices such as "applies after restart"
    appear below it in the accent colour.
- A selected row gets a 2 px accent border and a slightly lighter fill (see the FULLSCREEN row in the screenshot).
- Section headers (DISPLAY, IMAGE, FRAME PACING) are small capitals in accent2.

**Layout: launcher.**

- The full screen is used with no window chrome. The wordmark sits top left, with a dim subtitle under it ("Super Mario
  Bros. Wonder - Configuration").
- A left column, `max(300 px, 24% of width)` wide, lists the pages in capitals: PLAY, GAME FILES, SETTINGS, MODS,
  UPDATES, QUIT.
  - The selected page gets an accent fill at 16% plus a 5 px accent bar on its left edge.
  - Controller focus adds an accent outline at 90%, 2.5 px wide, with radius 10.
- The page on the right uses the same two-pane option rows: rows plus a description column.
- Bottom left: a status dot and a line of text. The dot is good, warn or bad; the text reads, for example, "Game files
  ready".
- Settings opens the overlay menu on top of the launcher, so there is a single settings UI.

**Motion.**

- Overlay open: `reveal` uses `approach` at rate 18 when opening and 26 when closing (about 0.15 s), shaped with
  `smoothstep`. The window rises 26 px as it fades in. It fades out without taking input.
- Page change: the new page slides 36 px in from the side of the tab that was asked for. Its alpha goes from 0.15 to 1
  at rate 14.
- The tab underline glides with `approach` at rate 18. It gets brighter while the cursor is on the tab bar.
- Row highlights ease at rate 22 (`eased`, which is stored per ImGui ID). The description cross-fades.
- Launcher: it fades in and rises 20 px over 0.35 s. The page highlight eases at rate 14. A backdrop glow drifts
  slowly (`sin(t*0.3)`).
- Compile card: a gliding progress bar with a moving sheen. It fades out over the first game frames.

**Background treatment.**

- Overlay: the game shows through, with a full-screen `#04020A` layer at up to alpha 120 (47%) that fades with
  `reveal`. There is no blur.
- Launcher: a vertical gradient (back_top, then back_mid, then back_top) and a radial accent2 glow at alpha 30, with a
  radius of 0.6 x the screen height, drifting horizontally.

**Controller prompts.**

- The overlay footer has round badges: (A) Select, (B) Back, (L)(R) Tab, (− +) Close, plus F1 / Esc. They are centred,
  under a hairline at white 10%.
- The launcher footer shows A Select, B Back, L R Pages, + Play.
- L and R switch tabs and pages. Down from the tab bar goes to the first row of the page. Left and right step the
  value of a row; A steps it forward.

**Distinctive touches.**

- The metrics card shows values coloured against the target frame rate, plus a frame-time graph with lines at 16.7 and
  33.3 ms.
- The compile screen is a card with a percentage, a count and the time left.
- Toasts appear in the bottom-right corner for 3.2 s.
- Quit is a "select twice" action.

**Desktop integration.**

- The program creates no shortcuts or menu entries of its own.
- Linux: `SMW\SuperWonders-launcher\tools\linux\superwonders.desktop` is packed into the AppImage by
  `tools/linux/package.sh`. It contains `Exec=wonder`, `Icon=superwonders` and `Categories=Game;`, plus a
  `[Desktop Action Configuration]` named "SuperWonders Launcher" that runs `Exec=wonder --config`. The menu entry is
  made by the user's AppImage integrator.
- Windows: a zip.
- `--config` and `--play` choose between the launcher and starting the game directly (`wanted()` in `launcher.cpp`).

### 1.2 Savage Wind (Breath of the Wild recomp)

**Toolkit and files.** Dear ImGui with Objective-C++ sources (`.mm`), built on every platform.

- Style: `SavageWindRecomp\runtime\ui\menu.mm` (`style_init()` at about line 435, `load_fonts()` at about 470,
  `tabs_body()` at about 1729, the prompt drawing at about 1909).
- Option rows: `runtime\ui\option_rows.h` and `option_rows.mm`, with `row()` at about line 60 and `kGold`.
- Start screen and launcher: `runtime\ui\launcher.mm` (`left_shade()`, the menu item with its feathered band at about
  line 1036, the title `text_fx`, the progress card at about 2098). Also `runtime\ui\splash.mm` (the start-screen art).

**Palette (`style_init`, `option_rows.mm`, `launcher.mm`)**

| Role | Hex |
|---|---|
| Menu window | `#0F1714` at 94% (a deep green-black) |
| Child / page card | `#17211C` at 85% |
| Popup | `#121A17` at 98% |
| Frame / button | `#213029` / `#24382E` |
| Hover / header | `#265C40` (a forest green) |
| Active / selected tab | `#36704F` |
| Accent (focus, checkmarks, tab bar, row borders) | `#ECC254` gold (`kGold`); `#9E7D33` dim gold for slider grabs |
| Text | `#F5F0E0`; row labels `#F0ECE0` |
| Muted (TextDisabled) | `#9EA394`; section titles `#8C9485` |
| Value / changed value / disabled | `#CDCDC3` / `#FFFFFF` / `#6E6E66` |
| Row fill idle / hot | black at 16% / white at 9% |
| Row border idle / hot | white at 15%, 1 px / gold, 2 px |
| Separator | `#4D5C4D` |
| Start screen shade | navy `#060A22`, from 88% at the left edge to 0 at 74% of the width |
| Start menu item idle / hot | `#FAFAFF` / `#FFD640`, shading to `#FFA032` |
| Hot item glow band | `#FFBE50` at 13% over a navy band at 59%, feathered |
| Title "Savage Wind" | gradient `#FFE878` to `#FF8A30`, shadow `#04081E` at 43% |
| Subtitle "CONFIGURATION" | ice blue `#CDE8FA` to `#96C8EB` |
| Progress card | `#0F1714` fill, gold border at 55%, gold bar, track white at 10%, radius 14 |
| Prompt pills | cream `#F5F6E8` with dark glyph `#191C18` |

**Fonts.**

- Fira Sans (SIL OFL, shipped in `third_party/fonts/FiraSans`): SemiBold for titles, Medium for rows.
- In-game menu: rows 29 px and titles 53.5 px at 1080p.
- Start screen: title 92 x 1.6, menu 52 x 1.3 and small 30 x 1.3, baked at about 2x and scaled with `s = height/720`.
- System fonts (Avenir Next, Segoe UI, DejaVu) are the fallback and are used for the FPS counter and the keyboard.
- Row labels are all capitals (`tr_upper`), as are section titles and the start-screen items (PLAY, SETTINGS, QUIT).

**Layout.**

- In-game menu: a rounded window (radius 14, padding 28 x 24, item spacing 14 x 12).
- Tab order: Prelaunch (start screen only), Controls, Video, Audio, Language, amiibo, Saves (start screen only),
  Cheats (in game only), Quit (in game only). Tabs that cannot be used are greyed out and skipped.
- Each tab page is a card (`##page` child) that reaches the edges of the line under the tabs.
- The option-row pattern (`option_rows.h`):
  - The rows pane is 48% of the width. Each row is 1.9 x the font height, with radius 9 and 16 px padding: an
    uppercase label on the left and the value on the right.
  - A value that can be stepped shows drawn triangles (`◀ value ▶`).
  - The right pane holds the description of the focused row, or the hovered row if none is focused, with an optional
    picture (save screenshots in Saves).
  - Rows can be greyed out but stay selectable, so their help still shows. Some rows sit indented under the one above.
  - Long lists open as a popup with a gold border.
- Start screen: the full-screen game art (`splash_draw`), or a picture the user puts in place. A navy shade comes in
  from the left. The gold-to-orange title sits at the top left. The menu of large words runs down the left; a
  version corner sits bottom right on its own feathered shade.

**Motion.**

- The in-game menu is mostly static in code. The focus bar under a tab is drawn at once.
- Start screen: a four-point sparkle grows and fades on PLAY, and a warm glow band appears behind the hot item. The
  boot picture fades into the game's title screen.
- SuperWonders' commit says it took "Savage Wind's motion", but the eased motion is SuperWonders' own (see 1.1).

**Background treatment.**

- In-game menu: the window is 94% opaque, with no blur. The on-screen keyboard and popups put black at 43-47%
  (alpha 110-120) over the game.
- Start screen: a gradient shade over the art, with no blur.

**Controller prompts and navigation.**

- L/R (Q/E on the keyboard) switch tabs and skip greyed-out ones.
- With the controller, the focused tab lights up and gets a 4 px gold bar along its bottom; ImGui's own outline is
  turned off.
- Down from the tab bar goes to the first row (`opt_focus_first`). Left and right step a row; holding them speeds it
  up after 1 s.
- The in-game hints are cream pills with button icons from the game's own glyph font, so they match the device in use.

**Distinctive touches.**

- Values that differ from their defaults are brighter.
- Greyed-out rows can still be selected, so their help is readable.
- The descriptions take the place of tooltips.
- The colour-grade presets in Video.
- A "Configuration" mode of the same program.

**Desktop integration.**

- Savage Wind creates no shortcuts or menu entries itself.
- Linux: `tools/linux/savagewind.desktop` is packed by `tools/linux/package.sh`. It contains `Exec=savagewind`,
  `Icon=savagewind`, `Categories=Game;` and `Terminal=false`, plus a `[Desktop Action Configuration]` named
  "Savage Wind Configuration" that runs `savagewind --config`. `docs/linux.md` tells users to get the menu entry from
  AppImageLauncher, Gear Lever or their desktop's own AppImage integration.
- Windows: a second executable, "Savage Wind Configuration.exe", opens the configuration screen.
- macOS: holding Option at start does the same (`launcher_config_key_held`).
- The recent commits `e3b8397` and `eb091e5` cover AppImage self-updates. SW Update keeps the AppImage's file name, so
  "shortcuts and Steam entries keep working" (`runtime/update_appimage.h`). It leaves alone copies installed by other
  launchers (Quiver, GitHub Launcher, RecompLauncher), and never takes `$APPIMAGE` unless `$APPDIR` shows it runs from
  inside that image.

### 1.3 Slate Companion (Breath of the Wild companion and Savage Wind launcher)

**Toolkit and files.** Avalonia 12 with `FluentTheme` (Dark variant) and .NET; the UI is built in C# code.

- Theme: `SlateCompanion\src\SlateCompanion\App.axaml`, which holds the brushes and all control styles.
- Helpers: `UI\Ui.cs` (`Text`, `Panel`, progress bars with gradients, `Tag`, `Eye`).
- Shell: `UI\MainWindow.cs` (the rune rail, the top bar and the toast).
- Settings: `UI\Views\SettingsView.cs` (the `LauncherPanel()` section with "APP MENU").
- First-run modal: `UI\SetupOverlay.cs`.
- Savage Wind options: `UI\Views\SwOptionsPanel.cs`.

**Palette ("Sheikah Slate", `App.axaml`)**

| Role | Hex |
|---|---|
| Background | `#071116` |
| Background 2 (rail gradient, top bar) | `#0B1A21` |
| Panel | `#10242D` (cards at 85%, `#D910242D`) |
| Panel high / button fill | `#163240` (buttons at 70%) |
| Line (borders) / line high | `#1F4352` / `#2D6378` |
| Accent ("Glow") | `#39C6FF`; light `#7FE3FF`; dark `#2A9ACB` |
| Glow soft (hover) | `#39C6FF` at 18% |
| Second accent (Play, eyebrows, to-do) | `#FF9A3C` orange; Play-button gradient from `#FF7A1A` |
| Done | `#6FF2C1`; "done in save" blue `#4FD2FF` |
| Text | `#D6EEF6` |
| Muted / faint | `#86A9B6` / `#55798A` |
| Chip "on" text | `#062030` on the accent |
| List selected / hover | accent at 12% / 5% |

**Fonts.**

- Segoe UI Variable Text, falling back to Segoe UI, Noto Sans, Cantarell and Ubuntu. The base size is 14.
- Mono: Cascadia Mono, then Consolas and JetBrains Mono.
- Type scale:
  - `h1`: 22 SemiBold.
  - `h2`: 13 SemiBold, letter-spacing 2, light accent, used in capitals ("SAVAGE WIND SETTINGS").
  - `eyebrow`: 11, letter-spacing 3, orange.
  - `label`: 11, letter-spacing 2, muted, capitals ("COMPLETION").
  - `value`: 28 SemiBold.
- The brand is "SLATE COMPANION" with letter-spacing 3.

**Layout.**

- A 76 px icon rail runs down the left (the "runes"): 54 x 54 buttons with radius 14 and 24 px icons.
  - The active rune has accent icons, a light border, an accent fill at 10% and an inset glow.
  - Settings is pinned at the bottom of the rail.
- The top bar holds the eye logo, the spaced wordmark, a search pill with a `Ctrl K` tag, and a save-sync pill.
- Content is in cards (radius 12, padding 16 x 14, 1 px line border) arranged in grids: stat tiles and progress rows.
- Play screen: a large orange pill button "Play" (radius 30, padding 54 x 14, 22 SemiBold) with a play icon, plus
  small outline buttons under it.
- Savage Wind settings panel:
  - At the top, a help box with an orange left border on an amber tint, reading "Point at a setting to see what it
    does". This is Slate's version of Savage Wind's description pane.
  - Below it, underline tabs: Controls, Video, Audio, Language, Saves, Game Files.
  - Below the tabs, section labels in spaced muted capitals (MOVEMENT, MOUSE), then rows with the label on the left
    and a toggle, slider or value on the right.
- Settings toggles are panels with a title, a muted help line and a switch on the right (`SetupOverlay.cs` "Add to
  applications").
- Notes are blocks with an orange 2 px left border.

**Motion.** Hardly any. Fluent's built-in control transitions (the toggle thumb, hover fades), and a toast at the
bottom centre.

**Background treatment.**

- A faint hex pattern behind the content (visible in `home.png`).
- The rail has a vertical gradient from `#0B1A21` to `#071116` at 95%.
- Modals (`welcome.png`) dim the app strongly and get a glowing accent border with radius about 16.
- No blur.

**Controller.** None: it is a mouse-and-keyboard desktop app (`Ctrl K` opens search).

**Distinctive touches.**

- The eye logo.
- Tags (pill outlines) in accent, orange or done colours.
- Progress bars with gradient fills.
- Everything is worded as plain sentences ("Goes straight into the game. Starting savagewind.exe yourself works as
  always.").

**Desktop integration.** This is the only project with code for it.

- Linux: `src\SlateCompanion\Core\DesktopEntries.cs`.
  - `DataHome` is `$XDG_DATA_HOME` when that is set to an absolute path, else `~/.local/share`. Entries go to
    `DataHome/applications` and icons to `DataHome/icons/hicolor/256x256/apps`.
  - `Sync()` writes `slatecompanion.desktop` (Type, Name, GenericName, Comment, quoted `Exec`, `Icon=slatecompanion`,
    `Terminal=false`, `Categories=Game;` and `StartupWMClass`). It writes one more entry for each Savage Wind AppImage
    it knows, with Play and Configuration actions (`--play` and `--config`).
  - `Exec` points at `$APPIMAGE` when the program runs as an AppImage, else at the binary. Paths are quoted and
    escaped for `\ " ` $`.
  - Writes are atomic (`.tmp` then `Move`), are skipped when nothing changed, and set mode 0755.
  - Icons are copied from the app, or pulled out of an AppImage with `--appimage-extract` (the file's PNG signature is
    checked).
  - Only files named `slatecompanion*.desktop` are ever written or removed; stale ones are removed. `RemoveAll()`
    removes every entry and icon the app made. Nothing system-wide is touched and no root is needed.
  - There is a CLI form: `--app-menu on|off` (`Program.cs`).
- How the UI presents it:
  - In first-run setup, a panel titled **"Add to applications"** with the help line "Slate Companion, and the copies
    of Savage Wind it knows, in your app menu so you can start and pin them." and a switch.
  - In Settings, an "APP MENU" section with a checkbox: "Add to applications: Slate Companion and your Savage Wind
    AppImages in the app menu (off removes them)".
  - Both are shown only on Linux (`DesktopEntries.Supported`).
- Windows: no in-app code. `build-release.ps1` packs a Velopack installer (`vpk pack --packTitle "Slate Companion"
  --icon icon.ico`). Velopack's Setup makes the Start menu and desktop shortcuts itself (its default; no
  `--shortcuts` override is passed) and removes them on uninstall.
- Updates keep the same file name "so desktop entries and shortcuts keep working" (`SwUpdate.cs`).

---

## 2. House-style synthesis

### What all three share

1. **Dark, tinted grounds, never neutral grey.** Each project tints its dark ground toward its own colour: green-black
   (Savage Wind), indigo (SuperWonders), teal-black (Slate). Panels are a step lighter and often slightly translucent
   (85-95%).
2. **One loud accent for focus and brand, and a second for headings or "go".** Gold and ice blue in Savage Wind; pink
   and cyan in SuperWonders; cyan and orange in Slate. The focused or selected item is shown with the accent as a
   border, an underline or a left bar, and never with ImGui's default nav rectangle (each project hides `NavCursor`
   and draws its own).
3. **Small capitals with tracking for structure.** Section titles and row labels are in capitals, often letter-spaced.
   The values and descriptions are in sentence case.
4. **The help text lives in its own pane or box, not in tooltips.** Savage Wind and SuperWonders use a right-hand
   description column. Slate uses "Point at a setting to see what it does".
5. **Rounded cards** with radius 9-14 and hairline borders at about 15% white or a "line" colour. Comfortable rows:
   about 1.9x the font height, 44-64 px.
6. **Plain-sentence copy.** Labels say what happens ("Goes straight into the game", "off removes them").
7. **A humanist sans with two weights**, one for rows and a heavier one for titles: Fira Sans, Nunito, or Segoe UI
   Variable. The two recomps ship the font under the OFL.

### What the two recomps share (the template for TempleFrog)

- Full-screen launcher with no window chrome: the wordmark top left, a vertical list of pages or words on the left,
  the content on the right, a status line bottom left, button hints along the bottom.
- The overlay: one large rounded window over the dimmed game, with tabs. L/R change tabs. Each tab has the two-pane
  rows on the left and the description on the right. Changed values are brighter, and the arrows show which values
  can be stepped.
- The game behind the overlay is dimmed with black at about 45% (alpha 110-120). There is no blur.
- A `--config` path, and a `[Desktop Action Configuration]` in the `.desktop` file, open the launcher. A plain start
  goes straight into the game.

### Where they differ

| | Savage Wind | SuperWonders | Slate Companion |
|---|---|---|---|
| Toolkit | ImGui | ImGui | Avalonia/Fluent |
| Launcher navigation | big words over the game art | page list with eased highlight and a left bar | icon rail plus top bar |
| Motion | nearly none (sparkle on PLAY) | eased everything: rise and fade, slide, glide | none |
| Backdrop | the game's art with a navy shade | indigo gradient with a drifting glow | flat with a hex texture |
| Title treatment | gold-to-orange gradient with a shadow | flat accent, ExtraBold | spaced capitals, small |
| Controller | full (game glyph font) | full (letter badges) | none |
| Desktop entries | AppImage-embedded only | AppImage-embedded only | writes XDG entries; Velopack shortcuts |

The house style takes Savage Wind's layout, SuperWonders' motion and badges, and Slate's plain help boxes and
toggle-with-help-line rows. Its desktop integration follows Slate's.

---

## 3. Desktop-integration findings and recommendation

**Today.**

- Only Slate Companion writes menu entries itself (Linux XDG). On Windows it relies on its Velopack installer.
- Savage Wind and SuperWonders only ship a `.desktop` file inside the AppImage and rely on AppImageLauncher, Gear Lever
  or the desktop's own integration.
- gbarecomp's Flatpak packaging (`gbarecomp\packaging\README.md`,
  `flatpak/build_flatpak.sh --generate-only`) generates a `.desktop` file and metainfo, which Flatpak exports itself.
- None of the projects creates a Windows `.lnk` from inside the program, and none uses `IShellLink` or PowerShell.

**Recommended for TempleFrog.** This is new code. It should be modelled on `DesktopEntries.cs`.

- **Windows, "Add desktop shortcut" and "Add to Start menu".**
  - Use `IShellLinkW` with `IPersistFile::Save`, through `CoCreateInstance(CLSID_ShellLink)` after `CoInitializeEx`.
    Set the target to `GetModuleFileNameW`, the working directory to the exe's folder, the icon to the exe (index 0),
    the description to "Frogger's Adventures: Temple of the Frog (TempleFrog)", and optionally the arguments.
  - Folders: `SHGetKnownFolderPath(FOLDERID_Desktop)` for the desktop, `FOLDERID_Programs` for the per-user Start menu
    (`%APPDATA%\Microsoft\Windows\Start Menu\Programs`). Neither needs admin rights.
  - File name: `TempleFrog.lnk`. Add an optional second `TempleFrog Launcher.lnk` with `--config`, which mirrors the
    Configuration action.
  - Removal deletes only those named files. The switch state is read back from whether the file exists and its target
    matches this exe, so moving the folder shows the switch as off.
  - Avoid PowerShell/`WScript.Shell`: it is slower, antivirus programs flag it, and it breaks under constrained
    language mode.
- **Linux, "Add to applications menu".**
  - Write `$XDG_DATA_HOME/applications/templefrog.desktop` (default `~/.local/share/applications`). Its fields:
    `Type=Application`, `Name=TempleFrog`, `GenericName=Frogger's Adventures: Temple of the Frog`, a quoted `Exec`
    (`$APPIMAGE` if set and if `$APPDIR` shows it runs from inside the image, as in Savage Wind's `e3b8397` rule;
    else `/proc/self/exe`), `Icon=templefrog`, `Terminal=false`, `Categories=Game;`, `StartupWMClass` matching the
    SDL window class, and `Actions=Configuration;` with `Exec=... --config`.
  - Copy the icon to `$XDG_DATA_HOME/icons/hicolor/256x256/apps/templefrog.png`. Optionally add a 512x512 copy.
  - Write atomically (`.tmp` then rename) and skip the write when nothing changed. Optionally run
    `update-desktop-database ~/.local/share/applications` if it is on PATH.
  - Removal deletes only `templefrog*.desktop` and its icon.
  - Optional "Add desktop shortcut" on Linux: put the same file in `$(xdg-user-dir DESKTOP)`, with mode 0755. GNOME
    also needs `gio set <file> metadata::trusted true`.
  - Hide all of these under Flatpak (`/.flatpak-info` exists), where the entry is exported already.
- **Presentation (house style).**
  - The launcher's **Install** tab has rows that work like Slate's switches: label, value On/Off, and a description in
    the help pane. "ADD DESKTOP SHORTCUT", "ADD TO START MENU" on Windows, "ADD TO APPLICATIONS MENU" on Linux.
  - The descriptions are plain sentences that say what the switch does and that turning it off removes the entry.
  - The first-run flow asks once (as `SetupOverlay.cs` does). After that the Install tab owns the setting.
  - Add a CLI form (`--app-menu on|off`, `--shortcut on|off`) for testing.

---

## 4. recomp-ui fit

`recomp-ui`. Theme tokens are in `src/common/launcher_theme.h`; `apply_scale()` maps
them onto `ImGuiStyle` in `src/common/backends/imgui/launcher_imgui.cpp`, and `push_runtime_style()` does the same in
`runtime_ui_imgui.cpp`.

**Expressible with today's tokens.**

| House-style element | Token |
|---|---|
| Tinted dark ground and vignette centre | `background`, `background2` (`draw_crt_background` gradient) |
| Card fill, hover, outline | `panel`, `panel_hovered`, `border` (ChildBg, PopupBg, Border) |
| Control fills | `control`, `control_hovered` |
| Main accent (CTA, active, checkmark) | `accent`, `accent_dim` (`neon_cta` gradient), `accent_text` |
| Second accent for section titles and links | `accent2` (`eyebrow_tracked`, TextLink, runtime value and toast) |
| Text and muted | `text`, `text_muted` |
| Status colours | `good`, `warn` |
| Focus border colour and width | `focus_ring`, `focus_ring_width` (NavCursor and `neon_cta`) |
| Flat LCD look | `scanlines = 0` |
| Radius, spacing, row height, type sizes | `radius_sm/lg`, `spacing_*`, `row_height`, `font_body/title/small` |

**Needs code.** In order of value:

1. **Per-game themes.**
   - Today `RecompLauncherConfig.theme` (`recomp_launcher.h:1788`) and `RecompRuntimeUiConfig.theme`
     (`recomp_runtime_ui.h:145`) are resolved with `launcher_theme_by_name()`. A game cannot pass its own colours.
   - Add a `const LauncherTheme* theme_tokens` (or a struct copy) to both configs. Use it when it is set, and fall back
     to the name. Alternatively add a `"templefrog"` built-in, but the override pointer is the reusable fix.
   - A `bad` token is also missing. Error text currently has to borrow `warn` or a hard-coded red.
2. **Savage Wind-style option rows and the description pane.**
   - Launcher settings are two columns of cards with ImGui widgets (`draw_settings`, `row_label` and the
     `draw_*_controls` functions).
   - The runtime overlay draws its rows in `draw_items()`, with the description under the label and the value in
     `accent2` when selected.
   - The house style needs these changes:
     - A shared row painter: uppercase label, `◀ value ▶` arrows, brighter changed values, an accent border on focus,
       and greyed rows that stay selectable.
     - A right-hand description column fed by the focused or hovered row.
     - L/R stepping the value.
   - SuperWonders' `option_rows.cpp` (MIT, from Savage Wind) is the direct model.
   - `draw_items` already has the label, value and description data, so the runtime side is mostly layout.
3. **Navigation chrome: tabs, bumpers and the prompt bar.**
   - The launcher's top-level nav is right-aligned buttons in the marquee header (`draw_ui`, "right-aligned top-level
     navigation").
   - The runtime overlay has a sections sidebar (`##sections`) plus a text hint line ("%s Select %s Back ...").
   - Needed:
     - A tab bar with an accent underline that glides, and L1/R1 (Q/E on the keyboard) to change tabs. The bumper
       names already exist in `settings_pad_button_label`, but nothing binds them to the tabs.
     - Down from the tabs to the first row.
     - A footer of button badges (port `ui::badge` and `ui::hints`) that use the profile's button names.
     - In the launcher, a left page list (Play, Settings, Mods, Saves, Quit) in place of the header buttons.
4. **Motion.** There are no animation helpers. Add `approach`, `eased` and `smoothstep`, with the same rates as
   SuperWonders: overlay reveal 18/26 with a 26 px rise, page slide 36 px at rate 14, underline at rate 18, row
   highlight at rate 22. The runtime already has `ui->dim` (`recomp_runtime_ui_set_backdrop`); fade it with the
   reveal.
5. **Backdrop.**
   - `draw_crt_background()` draws a gradient plus an accent bloom along the top. The house launcher wants the game's
     own art with a feathered shade from the left, as in Savage Wind's `left_shade()`.
   - TempleFrog's title-screen capture or box art could serve as the art. `launcher_artwork.h` and `hero_boxart_centered`
     exist, but only as a centred card.
   - Add a `backdrop_image` and a `shade` colour/strength, or a hook.
6. **Fonts.**
   - The launcher loads `assets/fonts/LatoLatin-Regular.ttf` only, one weight (`launcher_imgui.cpp` at about line
     15310), and sizes the title by scale.
   - A second, heavier font file for titles, with a config path for both, would allow the two-weight house type.
     Fredoka or Baloo 2 suit Frogger's rounded logo; SuperWonders tested both. Nunito ExtraBold is the
     safe pick.
7. **Desktop integration.**
   - Nothing exists in recomp-ui. `launcher_files.c` only reads `XDG_SESSION_DESKTOP` for file pickers.
   - Add a small `launcher_desktop_entry.{c,h}` (Win32 `IShellLinkW`; XDG `.desktop`) and an Install page that uses
     it (section 3).

---

## 5. TempleFrog proposal

### 5.1 Concept: "Temple at dusk"

The title screen is dark jungle wood, timber, and stained glass behind a lime-green logo with cream lettering. The
first level is bright leaf greens on sandstone and parchment.

- The **grounds and panels** come from the title's wood darks.
- The **main accent** is the leaf/logo lime (focus, CTA, tab underline), as Savage Wind uses gold and SuperWonders
  pink.
- The **second accent** is the stained-glass blue, for section titles.
- **Text** is the logo's cream.
- **Focus borders** are sandstone yellow, the Savage Wind gold role, so the focus never disappears against a lime
  button.
- Flat, with no scanlines (GBA LCD).

### 5.2 LauncherTheme token set

| Token | Hex (alpha) | Source in the game |
|---|---|---|
| `background` | `#201010` | title wood dark |
| `background2` | `#302010` | title wood dark (vignette centre) |
| `panel` | `#302010` (0.94 in the overlay) | wood dark |
| `panel_hovered` | `#3A2410` | between `#302010` and `#402010` |
| `control` | `#402010` | wood dark |
| `control_hovered` | `#503018` | `#402010` toward timber `#704010` |
| `border` | `#704010` | timber (decorative hairline) |
| `accent` | `#A0E010` | leaf green (level 1) |
| `accent_dim` | `#60B000` | leaf green (CTA gradient bottom, pressed) |
| `accent_text` | `#201010` | wood dark (text on lime buttons) |
| `accent2` | `#60A0F0` | stained-glass blue (section titles) |
| `text` | `#F0F0C0` | logo cream |
| `text_muted` | `#C8B088` | between bark `#B09070` and sandstone `#C0A040`, lifted for contrast |
| `good` | `#90F0D0` | mint (title) |
| `warn` | `#E0A080` | peach (title) |
| `focus_ring` | `#E0E080` | sandstone (level 1) |
| `scanline` | unused | `scanlines = 0` |
| (new) `bad` | `#E06040` | timber `#B06020` lifted (needs the new token) |
| title wordmark | gradient `#A0F090` to `#60B000`, shadow `#201010` at 60% | logo lime |
| selected-row tint | accent at 16% over the panel, about `#423F10` | |
| overlay dim | black at 0.45 | house value |

Dimensions:

- Keep `radius_sm 6`, `radius_lg 14`, `spacing 4/8/16/24` and `focus_ring_width 2.5`.
- Raise `row_height` to 52 so rows are about 1.9 x a 26-28 px body.
- Fonts: `font_body 20`, `font_title 40`, `font_small 14` (logical px).

### 5.3 WCAG contrast (computed; AA is 4.5:1 for body text, 3:1 for large text and UI parts)

| Foreground | on `background` `#201010` | on `panel` `#302010` | on `panel_hovered` `#3A2410` | on `control` `#402010` | on `control_hovered` `#503018` |
|---|---|---|---|---|---|
| text `#F0F0C0` | 15.69 | 13.38 | 12.45 | 12.52 | 10.09 |
| text_muted `#C8B088` | 8.77 | 7.48 | 6.96 | 7.00 | 5.64 |
| accent `#A0E010` | 11.52 | 9.83 | 9.14 | 9.19 | 7.41 |
| accent2 `#60A0F0` | 6.81 | 5.81 | 5.41 | 5.44 | 4.38 (large only) |
| good `#90F0D0` | 13.61 | 11.61 | 10.81 | 10.87 | 8.76 |
| warn `#E0A080` | 8.32 | 7.10 | 6.60 | 6.64 | 5.35 |
| focus_ring `#E0E080` (UI part) | 13.25 | 11.30 | 10.52 | 10.57 | 8.52 |
| border `#704010` (decorative) | 2.13 | 1.81 | 1.69 | 1.70 | 1.37 |

More measurements:

- `accent_text` `#201010` on `accent` `#A0E010` is 11.52. On `accent_dim` `#60B000` it is 6.74.
- On the selected-row tint `#423F10`: text 9.19, muted 5.14, focus border 7.77.
- Overlay panel at 94% over the brightest level art (parchment): text is still 11.45 and muted 6.40.
- Notes:
  - Bark `#B09070` alone as `text_muted` gives 3.98 on `control_hovered`. That is why the muted colour is lifted to
    `#C8B088`.
  - Cream text on `#60B000` is only 2.33. Lime buttons must use the dark `accent_text`.
  - The border is decorative. Focus and selection always use `focus_ring` or `accent`, which are at 7.4:1 or more.

### 5.4 Launcher home

The full screen has no window chrome (Savage Wind and SuperWonders pattern).

- **Backdrop:**
  - The title-screen art (a clean capture or the box art from `launcher_artwork`), scaled to fill with nearest-neighbour
    integer scaling, then softened by a `#201010` shade from the left: 88% at x=0, falling to 0 at 74%, using Savage
    Wind's `left_shade` stops.
  - A slow drifting `#60A0F0` glow at about 12% (stained-glass light) behind the logo.
  - Fallback without art: the `background` to `background2` gradient.
- **Top left:** the wordmark "TEMPLEFROG" in the title font with the lime gradient and a wood-dark shadow. Under it,
  "Frogger's Adventures: Temple of the Frog", in `text_muted`. In `--config` mode, add " · Configuration".
- **Left column**, `max(300, 24% of width)` wide, with 64 px rows in capitals:
  - PLAY, SETTINGS, MODS, SAVES, INSTALL, QUIT.
  - The selected row gets an accent fill at 16%, eased, plus a 5 px lime bar on its left edge.
  - Controller focus adds a sandstone `focus_ring` outline (2.5 px, radius 10).
- **Right pane:**
  - The page content in rows (section 5.5), and the description column on the far right.
  - PLAY page: a big lime CTA "▶ PLAY" (`neon_cta` with `accent` and `accent_dim`, text `#201010`). Under it, the ROM
    check line, e.g. "Game ready · AFRE · CRC OK", with a mint dot.
- **Bottom left:** the status dot (good, warn or bad) and one sentence ("Your copy of the game is ready.").
- **Bottom centre:** the prompt badges (A) Select, (B) Back, (L)(R) Pages, (Start) Play. The badges are cream
  `#F0F0C0` with the `#201010` letter, following the Savage Wind pill colours.
- **Motion:** fade and rise of 20 px over 0.35 s on open. The page highlight eases at rate 14.

### 5.5 Settings tabs (launcher Settings page and the in-game overlay share these)

Every tab uses the two-pane row pattern:

- Rows pane: 48% wide. Sections in small capitals in `accent2`. Rows 52 px, radius 9: an uppercase label in `text`, a
  `◀ value ▶` in `text_muted`, which turns `text` (brighter) when changed from the default.
- On focus, the row gets a 2 px `focus_ring` border and the 16% lime tint.
- Description pane: the focused row's help, in large `text_muted`. Notices such as "Applies after restarting" appear
  in `warn`.
- A preview picture where useful.
- Tabs use L/R. Down from the tabs goes to the first row.

**Video / Display**

- DISPLAY: Window size (◀ 3x · 720×480 ▶), Fullscreen, Integer scaling, Aspect (Original 3:2 / Stretch).
- IMAGE: Filter (Nearest / HQ2x / LCD shader), LCD colour correction, Frame blending (the GBA ghosting), Screen border.
  The description pane shows a live crop preview of the game image with the filter applied.
- PACING: VSync, Frame cap, Fast-forward speed.

**Audio**

- Master volume, Music, Effects, Audio latency (buffer), Mute when unfocused.

**Controls**

- CONTROLLER: one row per GBA button (A, B, L, R, Start, Select, D-pad). Left/right picks the pad button; the value is
  shown as a glyph badge.
- KEYBOARD: the same rows; A then a key press captures it.
- Rumble, Stick as D-pad, Reset to defaults.
- The description pane shows a GBA silhouette with the button highlighted.

**Saves**

- Save slots as rows: "SLOT 1 · Level 4 · saved 12 min ago". The description pane shows the save-state thumbnail
  (`opt_desc_image` in Savage Wind).
- Actions: Open saves folder, Back up now, Import .sav, Restore backup. Restore asks for confirmation.

**Mods**

- Patches and mods as toggle rows (the existing `draw_mod_packages` / `draw_quick_mods` data), with the description
  pane holding the author, a short summary and any conflict, in `warn`.
- Actions: Open mods folder, Refresh.

**Install**

- APP:
  - Windows: "ADD DESKTOP SHORTCUT" On/Off and "ADD TO START MENU" On/Off.
  - Linux: "ADD TO APPLICATIONS MENU" On/Off, and "ADD DESKTOP SHORTCUT" On/Off. These are hidden under Flatpak.
- Description pane copy, in the plain style: "Puts TempleFrog in your Start menu so you can search for it and pin it.
  Turning this off removes the entry."
- GAME FILES: ROM path (◀ Choose… ▶) with its check result, and Open data folder.
- UPDATES: This version, Check for updates.
- ABOUT: version, credits and licences.

### 5.6 In-game overlay

The overlay opens with Select+Start, or F1 or Esc.

- **Backdrop:** the game image dimmed with black at 0.45, faded in with the reveal. No blur, because the game image is
  small and blur costs a pass; the dim is cheap and is the house look.
- **Window:**
  - Centred, `min(92% of width, 1500 px at 1080p)` wide, full height minus a 22 px margin, radius 14.
  - Fill `panel` `#302010` at 94%, with a 3 px lime rule inset 14 px along the top edge (SuperWonders).
- **Header:** the title "Paused" in the title font, in `accent`. On the right, the save-sync state in `text_muted`, and
  a close X.
- **Tabs:** RESUME · VIDEO · AUDIO · CONTROLS · SAVES · SYSTEM. A gliding lime underline (rate 18) that brightens while
  the cursor is on the tab bar.
- **Resume tab:** large rows Resume, Save state, Load state, Reset game, and Quit to launcher, which needs pressing
  twice and is drawn in `bad`. The description pane shows the last save-state thumbnail.
- **Other tabs:** the same rows as section 5.5. Settings that need a restart show a `warn` notice and a toast.
- **Footer:** a hairline at white 10%, then the badges (A) Select (B) Back (L)(R) Tab (Start) Resume, centred.
- **Motion:**
  - Open: rise 26 px and fade at rate 18. Close: fade at rate 26, without taking input.
  - Page slide 36 px at rate 14. The row highlight eases at rate 22. The description cross-fades.
- **Metrics card** (optional, from SuperWonders):
  - A rounded card at the top left showing fps, frame ms and the 1% low, coloured against 59.73 Hz.
  - A frame-time graph with lines at 16.7 and 33.3 ms. This fits the user's frame-time suite rules.
