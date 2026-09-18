# mod-custom-classes

Framework for custom playable classes (ChrClasses ids 10 and 12-15).

A class is declared in two places:

- **World table `custom_class`** (`data/sql/updates/pending_db_world/*_custom_classes.sql`): the class id and
  the `TemplateClass` whose combat rules it borrows. The core takes the per-class tables (crit, dodge, parry,
  miss, regen, combat rating scaling) from the template, and this module answers the engine's `IsClass`
  question with it, so attack power, equipment rules, reactive abilities and trainers behave like the template.
  `InheritSpells` decides whether the class starts from the template's spellbook and trainers (a variant of
  that class) or only knows what it is given (a class with its own kit).
- **`localTools/customClasses/classes.json`**: everything the client and the world database need - name, power
  type, races, start level and position, base stats, skills, armor and weapon proficiencies, class icon.
  `python localTools/customClasses/buildCustomClasses.py` generates the client DBC patch data and the SQL.

What a class still needs of its own: spells, talents and icons, authored like the Combat rogue rework.

## Adding a class

1. Add an entry to `localTools/customClasses/classes.json` (id 10 or 12-15, class token, template class,
   power type, races, start level, icon cell, class color).
2. Run `python localTools/customClasses/buildCustomClasses.py`. It rewrites, from untouched backups:
   the four class DBCs for both the client patch and `server/Data/dbc`, the world SQL in
   `data/sql/db-world/base/custom_classes_generated.sql`, and `CustomClasses.lua` for the interface.
3. Rebuild the client patch (`clientPatcher/Build-FriendPatch.ps1` runs steps 2 and 3 itself, or
   `node localTools/mpq-builder/buildInterfacePatch.js` for the local client only).
4. Restart the worldserver: the updater applies the generated SQL, and `ObjectMgr::LoadCustomClasses`
   picks up the new `custom_class` row.

Playerbots never roll a custom class (`PlayerbotAIConfig`, `RandomPlayerbotFactory`): the bot AI has no
strategy for one.
