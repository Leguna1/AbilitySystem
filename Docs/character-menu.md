# Character menu (full-screen) — design

One full-screen menu with tabs: Character (stats + paper doll + inventory), Skills, Companion, later Quests and Map. Each tab has its own key (I, K, P, ...); pressing the same key again closes the menu; Q/E cycles tabs.

## Layout (Character tab)
- Left: stats panel (existing stats widget).
- Middle: 3D character preview (render target from the preview studio), equipment slots around it (Head, Chest, Hands, Legs, Feet left; Cape, Neck, Ring, Ring, Belt right; weapon + quiver under).
- Right: inventory (existing widget), filter tabs, tooltip with comparison against equipped.

## Preview studio (chosen over moving the real camera)
- An actor spawned far below/away from the playable world while the menu is open.
- Holds mirror copies of the subject's mesh components (player or companion), its own light rig, and a SceneCapture2D writing to a render target shown in the widget.
- Capture uses the show-only list so only the subject renders; transparent background (SceneColorHDR alpha, inverted in the UI material).
- Studio lights and preview meshes on their own lighting channel, so world lights don't touch the preview and studio lights don't touch the world.
- Captures every frame only while the menu is open.
- Preview plays a single idle animation (not the gameplay AnimBP). Drag to rotate.
- Refreshes when equipment changes, so tier recolours and enhancement glows show instantly.
- Companion tab uses the same studio with the companion as subject.

## Feel
- Hover an item: green/red deltas vs equipped, stats panel previews the change.
- Rarity = slot border colour + corner gem (M_UI_Slot parameter); "+N" for enhancement.
- Slots that fit the dragged item light up.

## Rules
- Menu shell is a C++ base (UUserWidget or CommonUI activatable), designed in the Blueprint designer, every bound part BindWidgetOptional.
- Input: UI only with cursor while open, game paused or slowed, player bar hidden.
- Companion tab: new widget (stats, health/poise, abilities, preview).
