# Player Bar: setup and hierarchy

The player bar is one widget, **WBP_PlayerBar**, that you add to your HUD. It is built from five small building-block widgets. Each block holds one piece of the kit; the player bar places them and the C++ fills them with data.

**Names matter.** Any widget written as **`Name`** below must be named exactly that, or C++ won't find it. Every named part is optional, so the bar works while it's only half built.

---

## 0. Code

1. Copy these files into the module, replacing your old versions where they exist:
   - `UIBarMaterial.h/.cpp` (new)
   - `PlayerBarWidget.h/.cpp` (new)
   - `ResourceWidget`, `PoiseBarWidget`, `AbilityProgressWidget`, `AbilitySlotWidget`, `AbilityBarWidget`
2. **Hand edit: `BaseCharacter.h`.** Add this to the public section:
   ```cpp
   /** Name shown in UI (player bar, pet panel, nameplates). */
   UFUNCTION(BlueprintPure, Category = "Character|Info")
   FText GetCharacterDisplayName() const { return CharacterDisplayName; }

   /** Portrait shown in UI (the pet panel's ring, a party frame). */
   UFUNCTION(BlueprintPure, Category = "Character|Info")
   UTexture2D* GetCharacterPortrait() const { return CharacterPortrait; }
   ```
   And this to the protected section:
   ```cpp
   UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Info")
   FText CharacterDisplayName;

   UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Info")
   TObjectPtr<UTexture2D> CharacterPortrait;
   ```
   Add `class UTexture2D;` near the other forward declarations if it isn't there already.
3. Compile.
4. In the wolf Blueprint, set **Character Info → Character Display Name** and **Character Portrait**. Do the same for the player if you want.

---

## 1. Building blocks

Each block's root is a **Size Box** with the size given, holding an **Overlay**. Inside the overlay are three layers, back to front: background, then **content**, then frame. Set every Image in the overlay to **Fill / Fill** alignment.

For the **Box** brushes, set the brush's **Draw As** to *Box* and enter the margin from the kit's table.

### 1a. WBP_ResourceBar (health, focus, pet health)

**Parent class:** Resource Widget.

```
SizeBox            Height Override 30 (width comes from where it's placed)
└ Overlay
  ├ Image  BarBG          T_UI_BarFrame_BG        Box 0.097 / 0.3 / 0.097 / 0.3
  ├ Image  BarFill        (any; the material is set per instance)   Slot Padding 26, 9, 26, 9
  ├ Image  BarFrame       T_UI_BarFrame           Box 0.097 / 0.3 / 0.097 / 0.3
  └ Text   ResourceValueText   Center / Center
```

Text style, used everywhere: the kit font, outline 1–2 px in near-black, and a slight shadow.

### 1b. WBP_StaggerBar (player and pet stagger)

**Parent class:** Poise Bar Widget.

```
SizeBox            Height Override 19
└ Overlay
  ├ Image  BarBG          T_UI_BarFrameStagger_BG   Box 0.091 / 0.395 / 0.091 / 0.395
  ├ Image  BarFill        Slot Padding ≈ 18, 6, 18, 6 (nudge until the fill sits inside the rim)
  └ Image  BarFrame       T_UI_BarFrameStagger      Box 0.091 / 0.395 / 0.091 / 0.395
```

### 1c. WBP_CastBar

**Parent class:** Ability Progress Widget.

```
SizeBox            Width 260, Height 18
└ Overlay
  ├ Image  BarBG          T_UI_BarFrameThin_BG      Box 0.058 / 0.417 / 0.058 / 0.417
  ├ Image  BarFill        Slot Padding ≈ 14, 5, 14, 5 (nudge)
  ├ Image  BarFrame       T_UI_BarFrameThin         Box 0.058 / 0.417 / 0.058 / 0.417
  └ Text   ProgressLabel  Center / Center
```

**Class Defaults:** set **Bar Settings → Material** to `MI_UI_Bar_Cast`. **Manage Visibility** is already on: the bar hides itself when no cast is happening, and fades and slides in when one starts.

A charge (Charged Shot) fills from 0 to 1, and a channel (Rapid Fire) drains from 1 to 0. If Rapid Fire's bar fills instead of draining, tick **Invert Count**.

### 1d. Slots: WBP_Slot, WBP_SlotSpecial, WBP_PetSlot

**Parent classes:**
- **WBP_Slot** and **WBP_SlotSpecial:** Ability Slot Widget.
- **WBP_PetSlot:** Companion Slot Widget. Pet slots are display only.

```
SizeBox            62 × 62     (Special 74 × 74, Pet 52 × 52)
└ Overlay
  ├ Image  SlotBG         T_UI_Slot_BG       (Special: T_UI_SlotSpecial_BG, Pet: T_UI_SlotPet_BG)
  ├ Image  IconImage      Slot Padding 10    (Special 11, Pet 9)
  ├ Image  SlotFrame      T_UI_Slot_Frame    (Special / Pet frames)
  └ Text   KeybindText    Right / Bottom, Slot Padding 0, 0, 12, 9  (Special: 0, 0, 13, 10)
```

**Class Defaults:** set **Slot Material** to `MI_UI_Slot`. C++ drives the slot material for you:

| Parameter | Driven by |
|---|---|
| `Icon` | The ability's icon. |
| `Cooldown` | Remaining cooldown. |
| `Disabled` | 1 when you can't afford the ability. |
| `Pressed` | 1 while the ability is running. |
| `Highlight` | A 0.2 s flash when the cooldown ends. |

The **IconImage** hides while the slot is empty, so only the stone and frame show. Your old Blueprint events (tooltip, drag visual, drag hover) still work.

For **WBP_PetSlot**, delete **KeybindText** or leave it in. The pet's slots show the ability's own key label, which is usually empty.

### 1e. WBP_AbilityBar (one row of slots)

**Parent class:** Ability Bar Widget.

```
HorizontalBox  SlotContainer
```

That's the whole block. The slots are created inside it at runtime, and **Slot Spacing** goes between them.

---

## 2. WBP_PlayerBar

**Parent class:** Player Bar Widget.

The root is a **Canvas Panel** that fills the screen. Everything below sits inside one inner canvas, **BarCanvas**, so the whole bar moves as one piece.

**BarCanvas slot settings:**
- **Anchors:** bottom-centre.
- **Alignment:** 0.5, 1.
- **Position:** 0, −16.
- **Size:** 1600 × 330.
- **Size To Content:** off.

All positions below are **inside BarCanvas** and measured from its top-left corner, so 800 is the horizontal centre. They are starting values from the kit's measurements: check them against your screenshot and nudge.

**Draw order:** a Canvas draws its children in list order, so the first child is at the back. Keep this order.

```
Canvas Panel (root, fill screen)
└ Canvas Panel  BarCanvas
  │
  ├ Canvas Panel  PetClip            Pos 1271, 181   Size 369 × 129   Clipping: Clip to Bounds
  │ └ Overlay     PetPanel           (fill PetClip)
  │   ├ Image     T_UI_PetPanel      369 × 129
  │   ├ Overlay   (portrait)         Left/Top, Padding 66, 21  Size 87 × 87
  │   │ ├ Image   PetPortrait        Padding 8 (circular mask material or plain)
  │   │ ├ Image   T_UI_PetPortrait   87 × 87
  │   │ └ Text    PetLevelText       Left/Top, Padding 58, 59 (badge centre ≈ 70, 70)
  │   └ VerticalBox                  Left/Top, Padding 162, 16
  │     ├ WBP_ResourceBar  PetHealthBar    Size 190 × 30
  │     ├ WBP_StaggerBar   PetStaggerBar   Size 190 × 19, Padding top 2
  │     └ WBP_AbilityBar   PetSlots        Padding top 6
  │
  ├ Image  PanelBG          T_UI_Panel (Box 0.039 / 0.276 / 0.039 / 0.276)   Pos 325, 200   Size 950 × 90
  ├ Image  CornerTL         T_UI_PanelCorner  45 × 45  Pos 325, 200
  ├ Image  CornerTR         same, Render Transform Scale X −1           Pos 1230, 200
  ├ Image  CornerBL         same, Scale Y −1                            Pos 325, 245
  ├ Image  CornerBR         same, Scale −1, −1                          Pos 1230, 245
  │
  ├ Overlay  XPGroup                                     Pos 375, 288   Size 850 × 18
  │ ├ Image  T_UI_BarFrameThin_BG    (Box, thin margins)
  │ ├ Image  XPFill                  Padding ≈ 14, 5, 14, 5
  │ ├ Image  T_UI_BarFrameThin       (Box, thin margins)
  │ └ Text   XPText                  Right/Center, Padding right 24
  │
  ├ HorizontalBox  SlotRow                               Pos 363.5, 180   Size To Content
  │ ├ WBP_AbilityBar  LeftSlots      V-Align Bottom, Padding bottom 14
  │ ├ Spacer          13 × 1
  │ ├ Overlay         Pod            Size 179 × 111
  │ │ ├ Image  T_UI_Pod
  │ │ └ WBP_AbilityBar  PodSlots     H-Center, V-Bottom, Padding bottom 14
  │ ├ Spacer          13 × 1
  │ └ WBP_AbilityBar  RightSlots     V-Align Bottom, Padding bottom 14
  │
  ├ Image  EndCapL          T_UI_EndCap_L  140 × 170   Pos 220, 160
  ├ Image  EndCapR          T_UI_EndCap_R  140 × 170   Pos 1240, 160
  ├ Image  SocketGem        T_UI_SocketGem  35 × 35    Pos 1275.5, 227.5
  │
  ├ WBP_ResourceBar  HealthBar     Pos 463, 111   Size 300 × 30
  ├ WBP_ResourceBar  FocusBar      Pos 837, 111   Size 300 × 30
  ├ WBP_StaggerBar   StaggerBar    Pos 533, 92    Size 180 × 19
  │
  ├ Overlay  MedallionGroup        Pos 728, 65    Size 144 × 123
  │ ├ Image  T_UI_Medallion
  │ ├ Image  MedallionGlow         T_UI_Medallion_Glow, material MI_UI_MedallionGlow
  │ └ Text   LevelText             Left/Top, Padding so it's centred on (72, 61): e.g. Center/Center, Padding bottom 2
  │
  └ WBP_CastBar      CastBar       Pos 670, 40    Size 260 × 18
```

### Where the numbers come from
- **Slot row:** 5 × 62 + 4 × 6 = 334 per group. Add 13 + 179 (the pod) + 13 between them, so the row is 873 wide, centred at 800.
- **Pod and medallion:** the pod starts at y 180. The medallion sits so its bottom gem touches the pod's top gem.
- **Health and focus bars:** they tuck 35 px under the medallion's side blades, centred on its height.
- **Stagger bar:** rides on top of the health bar, 70 px in from its left end.
- **End caps:** they overlap the panel by 35 px and are centred on its height. The right cap's socket centre is at 1293, 245.
- **Pet panel:** its key gem (22, 64) sits on the socket centre. It pokes past BarCanvas's right edge; that's fine, because a Canvas doesn't clip unless you tell it to. The **PetClip** box starts there, so the panel slides out from behind the crest. The crest is drawn later in the list, so it stays in front.

---

## 3. Per-instance Details in WBP_PlayerBar

Select each part in the hierarchy and set these:

| Part | Settings |
|---|---|
| **HealthBar** | Displayed Resource = Health. Bar Settings → Material = `MI_UI_Bar_Health`, Low Pulse Threshold = 0.25. |
| **FocusBar** | Displayed Resource = Focus. Material = `MI_UI_Bar_Energy`. Its `Reverse` = 1 is already on the instance, so it drains toward the medallion. |
| **StaggerBar** | Material = `MI_UI_Bar_Stagger`. Hide When Full = off, so it always shows like the mock-up. |
| **PetHealthBar** | Displayed Resource = Health. Material = `MI_UI_Bar_PetHealth`. **Find Resource Component Automatically = off.** |
| **PetStaggerBar** | Material = `MI_UI_Bar_PetStagger`. **Find Combatant Automatically = off.** Hide When Full = off. |
| **LeftSlots** | Slot Widget Class = WBP_Slot. Slot Indices = 0, 1, 2, 3, 4. Slot Spacing = 6. |
| **RightSlots** | Slot Widget Class = WBP_Slot. Slot Indices = 5, 6, 7, 8, 9. Slot Spacing = 6. |
| **PodSlots** | Slot Widget Class = WBP_SlotSpecial. Slot Indices = your LMB and RMB slot numbers (see below). Slot Spacing = 6. |
| **PetSlots** | Slot Widget Class = WBP_PetSlot. Min Granted Slots = 4. Max Granted Slots = 4. Slot Spacing = 6. |
| **WBP_PlayerBar (Class Defaults)** | XP Bar Settings → Material = `MI_UI_Bar_XP`. |
| **XPFill image** | Leave as is: the material comes from XP Bar Settings. |

**Finding your LMB/RMB slot numbers:** open the player's **Action Bar Component**. Slot numbers are positions in its **Slot Definitions** list, counting from 0. If LMB and RMB are the last two of 12, use 10 and 11. Put whatever order matches your keys into Left and Right.

---

## 4. Add it to the HUD

Create **WBP_PlayerBar** and add it to the viewport, or put it inside your existing HUD widget. You don't need to call anything else. The bar finds the player's pawn by itself, re-finds it after a respawn, and follows the companion as it comes and goes.

## 5. What happens by itself
- **Health, Focus and pet health:** the pale damage strip eases down over 0.6 s after a hit. **Health pulses below 25%.**
- **Stagger:** drops instantly and eases back up. A break holds the bar empty briefly.
- **Cast bar:** pops in for Charged Shot (fills) and Rapid Fire (drains). Its text reads "Charged Shot  75%".
- **Slots:** cooldown sweep, a ready flash, Disabled when you're out of focus, Pressed while the ability runs. Drag-and-drop and right-click to clear still work.
- **Level-up:** the medallion glows and fades over 1.5 s, and the level number updates.
- **XP:** the bar and its percentage.
- **Pet panel:** slides out on summon or revive, and slides back on death or dismiss. The socket gem stays until the panel's own gem has slid into the socket, and comes back as soon as the panel starts leaving.
- **If your end cap's socket isn't hollow:** put PetClip *after* EndCapR in the list, so it's drawn in front, and place it so the panel's gem sits on the socket centre. The panel then grows out of the socket.

**Tuning:** in WBP_PlayerBar's Class Defaults, **Pet Slide Duration**, **Pet Slide Offset** and **Level Up Glow Duration**. On each bar, **Trail Delay** and **Trail Duration**.
