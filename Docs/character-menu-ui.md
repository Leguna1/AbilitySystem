# Character Menu: setup and hierarchy

The character menu is one full-screen widget, **WBP_CharacterMenu**. It has a tab bar, a widget switcher holding each tab's content, and a close button. A component on the player controller opens and closes it. The design is in [character-menu.md](character-menu.md).

**Names matter.** Any widget written as **`Name`** below must be named exactly that, or C++ won't find it. Every named part is optional: a missing part just does nothing, and the menu still opens and closes.

---

## 0. Code

New files in `Source/AbilitySystem`:
- `CharacterMenuComponent.h/.cpp`: on the player controller. Handles opening and closing, pause, cursor, input mode, and hiding the player bar.
- `CharacterMenuWidget.h/.cpp`: the menu shell. Handles the tabs and the keys while the menu is open.
- `CharacterMenuTabButton.h/.cpp`: one button in the tab bar.

Phase 2 (3D preview):
- `CharacterPreviewStudio.h/.cpp`: the studio actor (mirrors, lights, scene capture).
- `CharacterPreviewWidget.h/.cpp`: shows the studio's render target and rotates on drag.
- `BaseCharacter.h`: new **Character Info → Preview Animation** property.
- `CharacterMenuWidget`: each tab has a **Preview Subject**, and the menu spawns and owns the studio.

Your stats, inventory and skill tree widgets are reused unchanged.

---

## 1. Input

1. Create three Input Actions in `Content/Input/Actions`, each with **Value Type: Digital (bool)** and no triggers:
   - `IA_Menu_Character`
   - `IA_Menu_Skills`
   - `IA_Menu_Companion`
2. Create **`IMC_Menu`** in `Content/Input` and map:
   - `IA_Menu_Character` to **I**
   - `IA_Menu_Skills` to **K**
   - `IA_Menu_Companion` to **P**

   Don't add these to IMC_Default. The controller adds IMC_Menu by itself, so the keys work while the pawn is dead or respawning.

**Why the menu reads keys itself:** while the menu is open, input is UI-only and the game is paused, so Enhanced Input actions don't fire. The menu widget handles keys in `NativeOnKeyDown` and asks Enhanced Input which keys are mapped to each `IA_Menu_*` action. If you remap I to Tab in IMC_Menu (or in player key settings), pressing Tab opens and closes the menu, and the tab button shows "Tab".

UI-only was chosen over "Game and UI + Trigger When Paused" because it blocks every gameplay action at once. With Game and UI, Q/E would also fire your Q/E abilities behind the menu unless each one were blocked by hand.

---

## 2. WBP_CharacterMenuTab (one tab button)

**Parent class:** Character Menu Tab Button.

```
SizeBox            Width 180, Height 48
└ Button  TabButton                 Style: transparent normal, faint hover (the label colour shows the selected tab)
  └ HorizontalBox                   H-Center, V-Center
    ├ Text   LabelText              the kit font, size 20
    └ Text   KeyText                size 14, Padding left 8, V-Center, colour grey   (shows "I", "K", "P")
```

**Class Defaults:** **Selected Label Color** (gold by default) and **Normal Label Color**. For an underline or background swap, use the **On Selection Changed** event (Is Now Selected).

---

## 3. WBP_CharacterMenu

**Parent class:** Character Menu Widget.

The root is a **Canvas Panel** that fills the screen. Draw order: the first child is at the back.

```
Canvas Panel (root)
├ Image  Backdrop                     Anchors: fill (0,0 → 1,1), Offsets 0   Colour black, alpha 0.85
│                                     Visibility: Visible  (catches clicks so they don't fall through)
│
├ Overlay  Header                     Anchors: top stretch (0,0 → 1,0), Offsets L 0, T 0, R 0, Size Y 72
│ ├ Image  T_UI_Panel                 Box brush, fill
│ ├ HorizontalBox  TabBar             H-Center, V-Center   (tab buttons are created in here)
│ └ Button  CloseButton               H-Right, V-Center, Padding right 24, size 40 × 40
│   └ Image                           an "X" icon
│
└ WidgetSwitcher  TabSwitcher         Anchors: fill, Offsets L 40, T 96, R 40, B 40
  │
  ├ HorizontalBox  CharacterTab       (child 0 = tab 0)
  │ ├ WB_CharacterSheet               Size: Auto, V-Fill
  │ ├ WBP_CharacterPreview            Size: Fill, H-Center, V-Fill, Padding 24, 0   (section 7)
  │ └ WB_Inventory                    Size: Auto, V-Fill
  │
  ├ Overlay  SkillsTab                (child 1 = tab 1)
  │ └ ScrollBox                       Orientation: Horizontal, H-Fill, V-Fill
  │   └ WB_SkillTree                  (its ContentSizeBox makes the scroll box scroll)
  │
  └ WBP_CompanionMenu  CompanionTab   (child 2 = tab 2; section 8)
```

The switcher's children must be in the same order as the **Tabs** list.

**Class Defaults → Character Menu | Tabs:**

| # | Display Name | Open Action | Preview Subject |
|---|---|---|---|
| 0 | Character | `IA_Menu_Character` | Player |
| 1 | Skills | `IA_Menu_Skills` | None |
| 2 | Companion | `IA_Menu_Companion` | Companion |

**Class Defaults → Character Menu | Preview:** set **Preview Studio Class** to `BP_CharacterPreviewStudio` (section 7c). If it's empty, the plain C++ studio is used. **Preview Studio Location** is 0, 0, −50000 by default; move it if your level reaches that deep.

- **Tab Button Class:** `WBP_CharacterMenuTab`.
- **Wrap Tabs:** on.

**Class Defaults → Character Menu | Keys** (the defaults are already right):
- **Previous Tab Keys:** Q, Gamepad Left Shoulder. **Next Tab Keys:** E, Gamepad Right Shoulder. **Close Keys:** Escape, Gamepad Special Right.
- **Previous Tab Action / Next Tab Action:** optional. Set them to `IA_QKey` / `IA_EKey` if you want a remapped Q/E to cycle tabs too. This only works while the pawn's mapping context is applied, which is why the fixed keys stay as a fallback.

**What the menu fills in for you:** every time it opens for a new pawn (the first time, or after a respawn), it finds these widgets anywhere inside itself, including inside WB_CharacterSheet:
- **Attribute Panel:** Set Character (pawn), unless it's showing someone else.
- **Inventory Widget:** Initialize Inventory with the pawn's Inventory Component.
- **Skill Tree Widget:** Initialize Tree with the pawn's Skill Tree Component.

For anything else, use the **On Content Pawn Changed** event. Other events: **On Menu Opened** (play an intro animation), **On Menu Closed**, and **On Tab Changed** (New, Previous; Previous is −1 on open).

---

## 4. BP_DefaultController

Add a **Character Menu** component and set:

| Setting | Value |
|---|---|
| **Menu Widget Class** | `WBP_CharacterMenu` |
| **Menu Mapping Context** | `IMC_Menu` |
| **Mapping Priority** | 1 |
| **Game Input Mode** | Game Only (what the game goes back to on close) |
| **Pause Game** | on |
| **Menu Z Order** | 50 (above the HUD) |
| **Widgets Hidden While Open** | already has *Player Bar Widget*. Add others (WB_ActiveEffectsContainer, a minimap…) if you want them hidden too. |

From Blueprint you can also call **Open Menu (Tab Index)**, **Close Menu** and **Toggle Tab**, and bind **On Menu Opened** / **On Menu Closed**.

---

## 5. Editor cleanup (the old menus)

1. **BP_DefaultCharacter:** delete the raw key events for **I**, **K**, **Tab** and **T** that call Open/Close Inventory, Skills and Stats. Leave the NumPad debug keys.
2. **WB_HUD:** remove **WB_CharacterSheet**, **WB_Inventory** and **WB_SkillTree**, along with the Open/Close functions and the Initialize Tree call. The menu owns them now. Keep WB_PlayerBar and WB_ActiveEffectsContainer.
3. **IMC_Default** doesn't change: Q/E stay ability keys. They don't fire while the menu is open.

---

## 6. What happens by itself

- **I / K / P:** opens the menu on that tab. The same key again closes it. A different tab's key switches tabs.
- **Q / E:** previous and next tab, wrapping around. **Esc:** close. Clicking a tab or the close button works too.
- **On open:** cursor shown, input UI-only, game paused, held keys released (you don't keep running after closing), player bar hidden.
- **On close:** everything is put back as it was. If the game was already paused before opening, it stays paused. The cursor goes back to whatever it was.
- **Focus:** clicking empty space can't break the keys. The menu takes keyboard focus back whenever nothing inside it has it.
- **Between openings:** the menu is collapsed, not destroyed, so scroll positions and the selected tab are kept.
- **PIE:** the editor binds **Esc** to "Stop" in Play In Editor and can grab it first. To test Esc, use Standalone Game, or rebind *Editor Preferences → Keyboard Shortcuts → Play World → Stop* (for example to Shift+Esc).

---

## 7. The 3D preview (Phase 2)

A **preview studio** actor is spawned far below the level the first time the menu opens. It holds copies ("mirrors") of the subject's meshes, its own three lights, and a scene capture. The capture renders only the mirrors into a render target, and **WBP_CharacterPreview** shows that render target in an Image.

### 7a. M_UI_CharacterPreview (material)

Create a material in `Content/Widgets/Materials`:
- **Material Domain:** User Interface
- **Blend Mode:** Translucent

```
TextureSampleParameter2D  "PreviewTexture"   (default: any texture; Sampler Type: Linear Color)
 ├ RGB ──────────────────────────────→ Final Color
 └ A → OneMinus ─────────────────────→ Opacity
```

The capture uses **Scene Color HDR**. Its alpha is 1 where nothing was drawn and 0 where the character is, so **1 − A** gives a transparent background. If the preview looks too dark or too bright, add a scalar parameter `Brightness` and multiply RGB by it, or tune the studio's lights (7c).

### 7b. WBP_CharacterPreview

**Parent class:** Character Preview Widget.

```
SizeBox            Width Override 520, Height Override 780   (2 : 3, the render target's aspect)
└ Image  PreviewImage          Brush → Image: M_UI_CharacterPreview   Draw As: Image
                               Visibility: Visible   (it must receive the mouse for dragging)
```

**Class Defaults:**
- **Texture Parameter Name:** `PreviewTexture`, matching the material.
- **Rotation Speed:** 0.5 degrees per pixel.
- **Invert Drag:** off. Turn it on if dragging feels backwards.

You don't have to wire anything. When the menu opens, it finds every Character Preview Widget inside itself, including inside the Companion tab's widget, and hands them the studio.

### 7c. BP_CharacterPreviewStudio (optional, for tuning)

**Parent class:** Character Preview Studio. Create it in `Content/Widgets/Preview` and set it as the menu's **Preview Studio Class**.

```
StudioRoot
├ SubjectPivot          (the subject's feet; dragging turns this; the subject faces +X)
├ CaptureComponent      (moved automatically to frame the subject)
├ KeyLight              Spot, 60 cd, front-left-high
├ FillLight             Spot, 20 cd, front-right-low, no shadows
└ RimLight              Spot, 50 cd, behind
```

**Class Defaults:**

| Setting | What it does |
|---|---|
| **Render Target Size** | 1024 × 1536. Keep the same aspect as the Size Box in 7b. |
| **Framing Padding** | 1.15, which leaves a 15% margin around the subject. |
| **Min Camera Distance** | 150 cm. |
| **Focus Height Offset** | Positive values move the subject down in the frame, so there's more headroom. |
| **Fallback Animations** | Skeleton → animation, used when a subject has no Preview Animation. Add `SK_Mannequin` → a Manny idle, and the wolf skeleton → `ANIM_Wolf_IdleBreathe`. |

**Lights:** select a light and change Intensity, Colour or Location. Leave **Lighting Channels** as they are: channel 2 only. The mirrors are on channel 2 too, so world lights don't touch the preview and studio lights don't touch the world.

**Capture Component:** fog, atmosphere, clouds, sky light, Lumen GI and reflections, decals and particles are already off. Exposure is manual (bias 0). To change brightness, change **Post Process → Exposure Compensation** or the light intensities.

### 7d. Preview animations

| Blueprint | Character Info → Preview Animation |
|---|---|
| **BP_DefaultCharacter** | A Manny idle (for example `MM_Idle`) |
| **BP_Companion_Wolf** | `ANIM_Wolf_IdleBreathe` |

The order is: the character's own **Preview Animation**, then the studio's **Fallback Animations** for its skeleton, then the reference pose. A Preview Animation made for the wrong skeleton logs a warning and falls back.

### 7e. What happens by itself

- **Mirrored:** every visible skeletal and static mesh on the subject and on every actor attached to it (bow, sword, quiver, an arrow on the string). Each mirror copies the mesh, materials, overlay material, custom primitive data and attach socket. Niagara and other non-mesh components are skipped.
- **Attachments:** a weapon goes on the same socket of the mirrored body (`hand_r`) with the same offset from that socket, so it sits in the idle pose's hand.
- **Leader Pose:** Helmet, Hands, Legs and the other parts follow the mirrored body, just as they follow the real one. A part with the body's skeleton but no leader (one using Copy Pose) is made to follow the body too.
- **Dynamic materials** are the same objects as on the character, not copies. A tier recolour or enhancement glow done through material parameters shows in the preview immediately.
- **Equipment changes:** putting on or taking off an item, or switching weapons, re-mirrors on the next frame. If you change the character's look some other way, such as Set Material with a new material or a mesh swapped in Blueprint, call **Mark Mirror Dirty** on **Get Preview Studio**.
- **Capture:** happens every frame only while a tab with a Preview Subject is open, including while the game is paused. On the Skills tab, and while the menu is closed, nothing is captured and the studio's lights are off.
- **On close:** the mirrors are destroyed and the studio stays, hidden, for the next opening. It's destroyed with the menu.
- **Never visible in game:** mirrors are *visible in scene capture only*, so the game camera can't see them even if it went down there.

---

## 8. The Companion tab (Phase 3)

**WBP_CompanionMenu** shows your companion: name, level, portrait, health, poise, its abilities and its stats, with the 3D preview in the middle. While no companion is out, it shows an empty state instead.

Code: `CompanionMenuWidget.h/.cpp` (new). The menu also refreshes companion widgets every time it opens.

### 8a. Building blocks you already have

- **WBP_ResourceBar** (1a in player-bar-ui.md): for the companion's health.
- **WBP_StaggerBar** (1b): for poise.
- **WBP_AbilityBar** (1e) with **WBP_PetSlot**: for the companion's abilities.
- **WB_AttributePanel**: for the companion's stats.
- **WBP_CharacterPreview** (7b): the 3D preview.

### 8b. WBP_CompanionMenu

**Parent class:** Companion Menu Widget.

```
Overlay (root, fill)
│
├ HorizontalBox  ContentPanel                     H-Fill, V-Fill   (shown while a companion is out)
│ │
│ ├ VerticalBox                                   Size: Auto, Width ≈ 420, V-Fill, Padding right 24
│ │ ├ HorizontalBox                               (header)
│ │ │ ├ Overlay                                   Size 96 × 96
│ │ │ │ ├ Image  Portrait                         Padding 8 (circular mask material or plain)
│ │ │ │ └ Image  T_UI_PetPortrait                 (frame)
│ │ │ └ VerticalBox                               V-Center, Padding left 16
│ │ │   ├ Text  NameText                          size 28
│ │ │   └ Text  LevelText                         size 18, grey      ("Level 12")
│ │ │
│ │ ├ WBP_ResourceBar  HealthBar                  Padding top 16, Height 30
│ │ ├ WBP_StaggerBar   PoiseBar                   Padding top 4, Height 19
│ │ │
│ │ ├ Text  "Abilities"                           Padding top 24, size 18
│ │ ├ WBP_AbilityBar   AbilityBar                 Padding top 8
│ │ │
│ │ └ WB_AttributePanel  CompanionStats           Padding top 24, Size: Fill
│ │
│ └ WBP_CharacterPreview                          Size: Fill, H-Center, V-Fill
│
└ VerticalBox  EmptyStatePanel                    H-Center, V-Center   (shown while no companion is out)
  ├ Image                                         a faded wolf silhouette, 256 × 256
  └ Text   "No companion summoned"                Padding top 16, H-Center, size 22, grey
```

Every named part is optional. Leave out CompanionStats if you don't want a stats list here.

### 8c. Per-instance Details in WBP_CompanionMenu

| Part | Settings |
|---|---|
| **HealthBar** | Displayed Resource = Health. Bar Settings → Material = `MI_UI_Bar_PetHealth`. **Find Resource Component Automatically = off.** If it's left on, the bar shows the player's health. |
| **PoiseBar** | Material = `MI_UI_Bar_PetStagger`. **Find Combatant Automatically = off.** Hide When Full = off. |
| **AbilityBar** | Slot Widget Class = `WBP_PetSlot`. Min Granted Slots = 4, Max Granted Slots = 4. Slot Spacing = 8. |
| **CompanionStats** | **Allow Spending = off.** Entries: the rows you want for the wolf. Rows for stats the wolf doesn't have are hidden by Hide If Missing. |
| **WBP_CompanionMenu (Class Defaults)** | **Level Format:** `Level {Level}`. |

The companion's name and portrait come from **BP_Companion_Wolf → Character Info**: **Character Display Name**, **Character Portrait** and, for the preview, **Preview Animation** (7d).

### 8d. In WBP_CharacterMenu

- Replace the Companion tab's placeholder with **WBP_CompanionMenu**, as the third child of **TabSwitcher**, named `CompanionTab`.
- Tab 2's **Preview Subject** must be **Companion** (section 3). That's what puts the wolf, not the player, in this tab's preview.

### 8e. What happens by itself

- **Companion out:** ContentPanel shows. Every part points at the companion: health, poise, abilities with their cooldowns, stats, name, level and portrait. The preview shows the wolf playing its Preview Animation.
- **No companion** (never summoned, dead, dismissed): ContentPanel collapses and EmptyStatePanel shows. Every bar is let go of, so nothing holds on to a dead wolf. The studio doesn't capture on this tab.
- **Summoned, died or dismissed while the menu is open:** the panels switch and the preview follows straight away.
- **Each opening:** the level, name and portrait are re-read, and the widget re-finds the companion after a respawn.
- **The player's stats never end up here:** the menu puts the player's pawn into Attribute Panels everywhere except inside a Companion Menu Widget.
- **Extra effects:** use the **On Companion Shown** event (Companion, or null) for a howl sound or a fade-in.
