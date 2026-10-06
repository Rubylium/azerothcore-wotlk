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
   - `isEmpty(state)` for the out-of-combat fade;
   - `isHidden(state)` (optional) when the HUD belongs to one specialization: not shown while it returns true (the
     state is nil before the first message). The server sends a "not this spec" payload (the Warrior's `-`).
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
art (`ReaperAtlas.blp`, 512², and `ReaperInfusedFlipbook.blp`, 2048² = 8×8 frames of 256, played at 20 fps:
Ascension's 60 felt frantic): the crossed
scythes frame, three soul skulls (empty, one or two thirds filled with the fragments of the next soul, full green,
infused purple), the purple glow and the infusion flipbook (additive, looping) while the Infusion d'âme lasts.

## The Gladiateur's HUD

`ClassHudWarrior.lua` (class token `WARRIOR`, the Gladiateur spec only), prefix `GLADIATOR`, payload
`bleed%:opening:brokenGuard:execute:coupDeGrace:duel:resets` or `-` (mod-warrior `SyncGladiatorHud`, every 250 ms when
it changed). On the Faucheur's model, the same size (162 x 70): two gladii lying mirrored, three gladiator's helmets
over them (`gladiatorHudV2Atlas.blp`, 1024 x 512; art `clientPatcher/assets/gladiatorHud/v2`, its
`assetManifest.json` for the boxes and placements; AI-painted from `.agents/plans/gladiator-hud/gladiator-hud.ASSETS.md`
with the Faucheur's HUD as the style reference):
- the helmets fill with Plaie du gladiateur's blood on the selected enemy, in ninths (each helmet a third of the way,
  the next one by thirds) against `warrior.glad_hud_ripe_ap` times the attack power (1: the bench's bleed peaks near
  there - 3 was never reached); a helmet filling pops and flashes, one emptied fades upwards;
- a crimson glow beats behind them when Execute is worth pressing: mod-warrior `GladiatorExecuteNow`, the payload's
  8th field - the same rule the bots and `.cheat rotation` execute on (mod-playerbots `GladiatorExecuteWorth` calls
  it), so the HUD and the rotation never disagree;
- Coup de grâce: every helmet molten gold, a gold glow and energy along the blades;
- the jewel in the boss: Revenge, gold under Ouverture (a pop as it comes), a red crack under Garde brisée;
- a gold ring out of the jewel each time Shield Slam comes back (`gladiatorShockwave.blp`, the resets count went up).

A first version - a big round shield, the bleed around its rim - read badly in game (too big, an awkward shape): keep
to the Faucheur's proportions (a small wide weapon frame, three sockets in a row) for any class HUD.

## The Pestiféré's HUD

`ClassHudPestifere.lua` (class token `PESTIFERE`, the tank only: a Sangsue healer - Transfusion learned, Carapace
nécrosée not carried - gets `-`), prefix `PESTIFERE`, payload
`carapace%:chair%:peste%:pourriture:avatar:riposte:sepulcre%:detonations:ripostes` (mod-pestifere `SyncHud`, every 250 ms when it
changed). On the Faucheur's model: two corroded cleavers, three flasks over them (`pestifereHudAtlas.blp`, 1024 x 512;
art `clientPatcher/assets/pestifereHud`, built by `localTools/interface/buildPestifereHudArt.py` from the paintings of
`.agents/plans/pestifere-hud/pestifere-hud.ASSETS.md`):
- the flasks are the plagues carried (Virulence), left to right Carapace nécrosée, Chair putride, Peste virulente;
  their liquid lowers with the time left (only out of combat: in a fight they last as long as they are carried) -
  the full painting cut from the top over the empty one, between the manifest's `flaskFillBounds`
  (`LIQUID_TOP`/`LIQUID_BOTTOM` in the Lua);
- the boil in the knot is Pourriture on the selected enemy: flat, small (1-2), swollen (3-5), ripe and throbbing at 6;
  a bile splash out of it each time a Détonation blew something up (the count went up);
- Avatar de la peste: the flasks boil over and a toxic glow pulses; Riposte purulente usable (its dodge / parry /
  block aura state, the spell ready): bile drips from the blades, and a burst of them on every cast (the rotation
  casts it too fast for "usable" to be seen); Sépulcre: a violet smoke behind, faint while it is up and holds nothing,
  thicker as it owes more (full at half the maximum health).

A glow pulsing on an `Alpha` animation: set the texture's alpha, then `Play()` the pulse. A `UIFrameFadeIn` started
under it left the glow at the alpha the animation started from (0) - the Pestiféré's and the Gladiateur's glows were
never seen in game until this changed.
