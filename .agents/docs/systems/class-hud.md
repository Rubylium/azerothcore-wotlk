# Class HUD

A small animated element that shows a class's own resource (the Faucheur's souls first), in the style of
Ascension's class resources. It is the one place class state gets custom UI: no other per-class frames. Ask the user
before giving a class one.

## What the player gets

- One frame per class, under the player frame by default. Left-drag moves it, right-click opens its menu: lock /
  unlock, fade out of combat, reset position, size (50-150%). `/classhud reset|lock|unlock` does the same.
- Position, size, lock and fade are saved per character (`CLASS_HUD_SETTINGS["Name-Realm"]`, through the stock
  FrameXML `RegisterForSave`, the account's SavedVariables file).
- Out of combat and empty, it fades to 45% (unless the player turned that off).
- Hovering shows the title, the class's tooltip lines and the move hints.

## How it works

```
server module ──"PREFIX\tpayload" (addon whisper)──▶ ClassHud.lua ──parse──▶ state ──update──▶ class art
                                     player auras ──fromAuras (until the first message)──┘
```

- `clientPatcher/interface/Interface/FrameXML/ClassHud.lua` is the framework: the frame, dragging, the menu,
  the saved settings, the fade, the tooltip, and the event plumbing (`CHAT_MSG_ADDON`, `UNIT_AURA`, combat).
- Each class adds `ClassHud<Class>.lua` with one `ClassHud_Register{...}` call; it draws only inside the frame the
  framework gives it. Both files are in `frameXmlFiles` of `localTools/mpq-builder/buildInterfacePatch.js`
  (framework first). Art goes in `clientPatcher/interface/Interface/ClassHud/`.
- The server owns the truth. Its module sends the whole state on every change, as an addon whisper to the player:
  `ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player, "PREFIX\t...")` (the client
  splits the prefix at the tab). Send only when something changed, and once at login. `fromAuras` is the fallback
  until the first message arrives (a stack aura per counter is the usual source); once the server has spoken its
  messages win over auras.

## Adding a class

1. Server: keep the resource in auras (so it survives relogs and shows in the buff bar) and send
   `PREFIX\t<fields separated by :>` whenever it changes (see `SyncHud` in `modules/mod-reaper/src/Reaper.cpp`).
2. Client: `ClassHud<Class>.lua` registering:
   - `token` (the class token `UnitClass` returns), `prefix`, `width`/`height`, default `anchor`
     `{ point, relativeTo, relativePoint, x, y }`, `title`, `tooltip(state)` (lines);
   - `create(frame)`: build textures, child frames and animation groups;
   - `parse(payload)` → state table (nil to ignore), `fromAuras()` → state;
   - `update(frame, state, previous)`: draw; compare with `previous` to animate what changed (it is nil on the
     first draw);
   - `isEmpty(state)` for the out-of-combat fade.
3. Add the file to `frameXmlFiles`, after `ClassHud.lua`; ship the art.
4. Document the class's resource in its module README.

## Animating on 3.3.5

- `Region:CreateAnimationGroup()` works on textures: `Scale` (`SetScale`), `Alpha` (`SetChange`, relative to the
  current alpha, undone when the animation ends), `Translation` (`SetOffset`); `SetOrder` chains them,
  `SetLooping("BOUNCE")` pulses. Use them for "gained" (pop + additive flash) and "spent" (a ghost copy that fades and
  rises) so the change reads at a glance.
- Flipbooks (an atlas of frames) are played in an `OnUpdate` with `SetTexCoord`: frame index =
  `floor(elapsed * fps) % frames`, cell = `(index % columns, floor(index / columns))`. Stop the `OnUpdate` when hidden.
- No atlas API, no `SetShown`, no `C_` namespaces: `SetTexture` + `SetTexCoord` from the atlas's pixel boxes.
  Additive glows: `SetBlendMode("ADD")`.
- Keep to the UI palette rules (gold/parchment on dark): the class art itself may be the class's colours.

## The Faucheur's HUD

`ClassHudReaper.lua`, prefix `REAPER`, payload `souls:fragments:infused` (mod-reaper `SyncHud`). Ascension's Reaper
art (`ReaperAtlas.blp`, 512², and `ReaperInfusedFlipbook.blp`, 2048² = 8×8 frames of 256, 60 fps): the crossed
scythes frame, three soul skulls (empty, one or two thirds filled with the fragments of the next soul, full green,
infused purple), the purple glow and the infusion flipbook (additive, looping) while the Infusion d'âme lasts.
