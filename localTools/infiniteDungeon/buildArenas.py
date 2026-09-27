"""Builds the Infinite Dungeon arenas (mod-stat-growth src/infinite/InfiniteDungeonArenas.cpp) and the creature
copies they spawn (data/sql/db-world/base/stat_growth_infinite_dungeon_creatures.sql).

An arena is a room of an existing dungeon: where the players arrive (the safety bubble), where the boss stands, and
the spots its trash packs stand on. The positions were read from the stock spawns of each map (a boss room and the
trash spawned in it, level with the boss): they are on the ground and on the dungeon's navmesh. The server checks the
trash spots against the boss spot's line of sight when it builds a floor and falls back on the line between the bubble
and the boss (InfiniteDungeon.cpp).

Every creature an arena spawns is a copy of a stock one, under its own entry (920010 and up), so the stock creature
stays untouched: no loot, no experience (the floor pays it), our own script, a rank by role (trash normal, elite
and boss elite, the boss with the boss portrait) and a short aggro range (12 yards, the boss 10), so leaving the bubble
pulls one pack at a time. Run it again after changing ARENAS:

    python localTools/infiniteDungeon/buildArenas.py
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CPP = os.path.join(ROOT, "modules", "mod-stat-growth", "src", "infinite", "InfiniteDungeonArenas.cpp")
SQL = os.path.join(ROOT, "modules", "mod-stat-growth", "data", "sql", "db-world", "base",
                   "stat_growth_infinite_dungeon_creatures.sql")

FIRST_COPY = 920010
LAST_COPY = 920299
ROLE_TRASH, ROLE_ELITE, ROLE_BOSS = 0, 1, 2

# (levels, English name, French name, map, entry spot, boss spot, trash spots, trash entries, elite, boss)
ARENAS = [
    # 15-29: the Deadmines, Shadowfang Keep, the Scarlet Monastery
    ((15, 29), "The Deadmines: the foundry", "Les Mortemines : la fonderie", 36,
     (-139.83, -569.44, 19.79, 3.27), (-177.36, -574.46, 19.31, 0.13),
     [(-178.32, -581.50, 19.31), (-178.40, -565.43, 19.31), (-196.75, -582.34, 20.98), (-186.88, -553.57, 19.31),
      (-193.10, -550.66, 19.31), (-149.15, -580.59, 19.31)],
     [1731, 622, 4416], 4417, 1763),
    ((15, 29), "The Deadmines: the Juggernaut", "Les Mortemines : le Mastodonte", 36,
     (-79.58, -788.13, 38.82, 5.27), (-59.62, -820.13, 41.61, 2.13),
     [(-67.64, -809.50, 40.87), (-77.81, -815.10, 40.02), (-78.28, -824.78, 40.00), (-65.92, -794.40, 39.40),
      (-40.28, -797.97, 39.33), (-53.00, -791.27, 38.61)],
     [657, 1732], 636, 647),
    ((15, 29), "Shadowfang Keep: the chapel", "Donjon d'Ombrecroc : la chapelle", 33,
     (-256.98, 2249.59, 100.98, 0.28), (-222.59, 2259.44, 102.84, 3.42),
     [(-221.78, 2252.10, 102.84), (-229.85, 2261.64, 102.84), (-232.23, 2250.59, 101.89), (-241.66, 2256.07, 100.98),
      (-250.08, 2262.77, 100.97)],
     [3877, 3875], 3857, 4278),
    ((15, 29), "Shadowfang Keep: the kitchens", "Donjon d'Ombrecroc : les cuisines", 33,
     (-230.60, 2284.89, 75.08, 5.52), (-202.60, 2257.96, 76.28, 2.38),
     [(-193.13, 2257.30, 76.28), (-192.79, 2249.88, 76.28), (-179.08, 2236.49, 76.32)],
     [3875, 3853], 3864, 3886),
    ((15, 29), "Scarlet Monastery: the interrogation", "Monastère écarlate : l'interrogatoire", 189,
     (1755.47, 1146.65, 7.57, 5.66), (1786.55, 1124.44, 7.57, 2.52),
     [(1793.45, 1143.31, 7.57), (1759.54, 1121.24, 7.49), (1763.58, 1103.48, 6.90)],
     [4306, 4283], 4540, 3983),
    # 30-44: Uldaman, Zul'Farrak, Maraudon
    ((30, 44), "Uldaman: the Chamber of Khaz'mul", "Uldaman : la chambre de Khaz'mul", 70,
     (90.96, 301.70, -52.12, 5.14), (104.34, 272.31, -51.70, 2.00),
     [(102.63, 255.19, -51.70), (119.47, 263.30, -51.70), (88.09, 265.33, -51.70), (89.73, 282.81, -51.70),
      (121.00, 280.15, -51.70), (106.74, 290.62, -51.70)],
     [7309, 7077, 7076], 10120, 2748),
    ((30, 44), "Uldaman: the Stonevault", "Uldaman : la Voûte de pierre", 70,
     (-232.01, 198.06, -44.53, 4.88), (-225.60, 161.22, -44.55, 1.74),
     [(-233.51, 170.85, -44.63), (-228.12, 177.46, -44.63), (-231.57, 137.17, -46.63), (-224.21, 134.80, -46.63),
      (-230.61, 129.84, -46.63)],
     [4851, 4852], 4850, 6910),
    ((30, 44), "Zul'Farrak: the Altar of Sacrifice", "Zul'Farrak : l'autel du sacrifice", 209,
     (1804.25, 886.57, 8.98, 3.94), (1778.01, 859.67, 8.88, 0.80),
     [(1783.67, 853.47, 8.96), (1784.11, 865.57, 8.96), (1766.60, 856.17, 8.88), (1767.67, 850.92, 8.88),
      (1777.98, 873.71, 9.09), (1785.51, 873.03, 8.96)],
     [5649, 7247, 5650], 7246, 7272),
    ((30, 44), "Zul'Farrak: Gahz'rilla's pool", "Zul'Farrak : le bassin de Gahz'rilla", 209,
     (1713.24, 1247.76, 9.95, 4.33), (1698.24, 1210.74, 9.42, 1.19),
     [(1714.38, 1218.58, 8.96), (1699.81, 1233.84, 8.96), (1714.92, 1190.31, 10.34)],
     [5649, 5650], 8095, 7795),
    ((30, 44), "Maraudon: the Wicked Grotto", "Maraudon : la grotte maléfique", 349,
     (721.57, -211.07, -47.17, 5.98), (748.87, -219.65, -47.69, 2.84),
     [(740.94, -204.99, -47.69), (740.68, -234.44, -47.74), (721.64, -227.51, -47.17)],
     [11792, 11791], 12217, 12236),
    # 45-59: Blackrock Depths, Stratholme, Scholomance
    ((45, 59), "Blackrock Depths: the Grim Guzzler", "Profondeurs de Rochenoire : le Sinistre Écluseur", 230,
     (849.49, -179.32, -49.67, 0.74), (878.12, -153.07, -49.76, 3.88),
     [(875.47, -161.67, -49.67), (881.50, -163.23, -49.68), (870.15, -144.16, -49.67), (868.53, -161.39, -49.67),
      (877.04, -166.49, -49.67), (872.04, -166.16, -49.67)],
     [9545, 9547, 9554], 9541, 9537),
    ((45, 59), "Blackrock Depths: the Ring of Law's gate", "Profondeurs de Rochenoire : les geôles", 230,
     (583.08, -286.59, -82.16, 0.53), (615.52, -267.40, -83.59, 3.68),
     [(606.65, -262.92, -83.49), (620.12, -276.79, -83.80), (626.24, -272.32, -83.92), (627.17, -261.77, -83.89),
      (601.20, -267.20, -82.99), (625.55, -278.92, -83.88)],
     [8892, 8891, 8921], 8909, 9025),
    ((45, 59), "Stratholme: Festival Lane", "Stratholme : l'allée du Festival", 329,
     (3594.25, -3470.98, 134.98, 4.75), (3595.75, -3509.93, 137.50, 1.61),
     [(3605.81, -3515.00, 138.07), (3612.31, -3495.04, 136.79), (3603.74, -3486.70, 135.76),
      (3591.79, -3484.97, 135.11), (3621.50, -3514.44, 137.11), (3618.75, -3497.01, 137.19)],
     [10390, 10391, 10405], 10381, 10558),
    ((45, 59), "Stratholme: the Crimson Hall", "Stratholme : la salle Écarlate", 329,
     (3490.27, -3084.98, 135.08, 3.63), (3455.96, -3103.41, 136.54, 0.49),
     [(3441.98, -3092.17, 135.09), (3473.06, -3093.76, 136.63), (3436.74, -3090.19, 135.09),
      (3443.41, -3083.56, 135.00), (3465.89, -3073.21, 135.09)],
     [10424, 11043, 10425], 10422, 10811),
    ((45, 59), "Scholomance: the Reliquary", "Scholomance : le reliquaire", 289,
     (237.61, -10.24, 85.31, 0.30), (274.88, 1.33, 85.31, 3.44),
     [(267.65, 8.46, 85.31), (268.14, -7.63, 85.31), (263.73, -4.29, 84.92), (262.46, 5.08, 84.92),
      (256.29, 0.65, 84.92), (256.57, -8.36, 85.31)],
     [10480, 10481], 10495, 10901),
    ((45, 59), "Scholomance: the Great Ossuary", "Scholomance : le grand ossuaire", 289,
     (239.97, 100.05, 95.91, 5.53), (268.22, 73.63, 95.92, 2.39),
     [(271.31, 90.31, 95.91), (267.35, 96.31, 95.91), (278.55, 93.96, 95.82), (272.73, 96.88, 95.91),
      (250.89, 90.55, 95.91), (254.89, 94.73, 95.91)],
     [10485, 10481], 10495, 10503),
    # 60-69: Hellfire Citadel, Auchindoun, Coilfang Reservoir, Tempest Keep
    ((60, 69), "Hellfire Ramparts: the watch", "Remparts des Flammes infernales : le guet", 543,
     (-1314.25, 1622.67, 91.83, 0.15), (-1278.21, 1628.18, 91.72, 3.29),
     [(-1270.92, 1627.81, 91.75), (-1270.05, 1622.27, 91.76), (-1288.11, 1637.25, 91.82), (-1268.38, 1639.36, 91.62),
      (-1275.13, 1643.13, 91.66), (-1296.52, 1636.16, 91.83)],
     [17259, 17269, 17270], 17271, 17306),
    ((60, 69), "The Blood Furnace: the cells", "La Fournaise du sang : les cellules", 542,
     (234.02, -106.41, 9.61, 1.79), (226.37, -71.30, 9.62, 4.93),
     [(241.83, -68.38, 9.62), (231.51, -91.57, 9.62), (246.38, -85.31, 9.62), (224.59, -96.00, 9.62)],
     [17397, 17477], 17371, 17377),
    ((60, 69), "Mana-Tombs: the ethereal hall", "Tombes-mana : la salle éthérienne", 557,
     (-277.12, -4.83, 16.68, 0.43), (-246.70, 9.23, 16.79, 3.57),
     [(-253.22, 5.28, 16.87), (-258.09, 13.03, 17.16), (-260.96, 7.86, 16.79), (-259.23, 2.10, 16.87),
      (-227.72, -7.85, 17.03), (-222.66, 19.06, 17.03)],
     [18312, 18315, 19306], 19307, 18341),
    ((60, 69), "Auchenai Crypts: the bone hall", "Cryptes Auchenaï : la salle des ossements", 558,
     (-22.89, -360.50, 26.59, 3.50), (-57.94, -373.70, 26.59, 0.36),
     [(-60.74, -366.53, 26.59), (-51.68, -369.06, 26.59), (-62.41, -381.28, 26.59), (-52.70, -362.13, 26.60),
      (-47.77, -365.05, 26.59), (-37.73, -365.69, 26.59)],
     [18700, 18493], 18702, 18373),
    ((60, 69), "The Slave Pens: the pumping station", "Les enclos aux esclaves : la station de pompage", 547,
     (39.15, -395.50, 3.12, 1.45), (43.23, -362.79, 3.04, 4.59),
     [(39.14, -371.74, 3.04), (47.70, -371.66, 3.04), (42.18, -378.45, 3.04), (49.48, -380.22, 3.04),
      (33.90, -379.37, 3.12)],
     [17962, 17940], 21127, 17941),
    ((60, 69), "The Underbog: the Murkblood camp", "La Basse-tourbière : le camp Bourbesang", 546,
     (167.61, -440.29, 72.49, 1.47), (171.28, -402.48, 72.32, 4.62),
     [(162.62, -424.84, 72.40), (173.53, -428.71, 72.53), (197.16, -389.32, 72.46), (158.77, -429.58, 72.35)],
     [17728, 17771, 17729], 17735, 17882),
    ((60, 69), "The Mechanar: the Mechanar's heart", "Le Méchanar : le cœur du Méchanar", 554,
     (290.62, 29.12, 25.47, 5.38), (309.39, 5.29, 25.52, 2.24),
     [(309.33, 15.13, 25.47), (309.52, 20.28, 25.47), (326.52, 13.20, 27.92), (293.92, -14.85, 25.38)],
     [19168, 19510], 19735, 19221),
    ((60, 69), "The Botanica: the laboratory", "La Botanica : le laboratoire", 553,
     (-152.40, 372.66, -17.61, 2.22), (-174.27, 401.28, -17.61, 5.36),
     [(-170.73, 408.12, -17.61), (-166.86, 398.47, -17.61), (-177.22, 408.96, -17.61), (-173.27, 390.13, -17.61),
      (-166.66, 391.76, -17.61), (-178.10, 414.29, -17.61)],
     [19507, 19513], 19865, 17978),
    # 70-80: Utgarde, Azjol-Nerub, Gundrak, the Halls of Stone and of Lightning, the Icecrown five-mans
    ((70, 255), "Utgarde Keep: the forge", "Donjon d'Utgarde : la forge", 574,
     (254.02, -27.65, 24.76, 5.03), (264.44, -59.66, 24.76, 1.89),
     [(269.37, -54.37, 24.68), (257.96, -66.89, 24.68), (268.93, -73.46, 24.76), (265.14, -42.03, 24.68),
      (256.11, -40.82, 24.76)],
     [24078, 24080, 28419], 24085, 23953),
    ((70, 255), "Utgarde Pinnacle: the Ymirjar hall", "Cime d'Utgarde : la salle des Ymirjar", 575,
     (419.46, -454.22, 75.25, 2.84), (383.63, -442.98, 75.20, 5.98),
     [(383.84, -435.35, 75.20), (372.71, -436.91, 75.07), (396.42, -442.99, 75.20), (396.32, -435.55, 75.20),
      (370.79, -432.29, 75.12), (355.94, -445.53, 75.25)],
     [26694, 26696, 28368], 26555, 26687),
    ((70, 255), "Azjol-Nerub: the Gatewatcher's hall", "Azjol-Nérub : la salle du Gardien des portes", 601,
     (543.83, 665.12, 776.25, 3.16), (506.52, 664.38, 776.98, 0.02),
     [(521.82, 659.47, 776.31), (526.66, 663.61, 775.80), (531.03, 658.17, 776.24), (529.56, 646.23, 777.41)],
     [28732, 28734, 28733], 28730, 28684),
    ((70, 255), "Gundrak: the snake pit", "Gundrak : la fosse aux serpents", 604,
     (1758.89, 646.90, 124.42, 5.29), (1779.46, 615.21, 124.57, 2.15),
     [(1780.98, 622.13, 124.39), (1775.80, 608.97, 124.47), (1782.26, 608.28, 124.48), (1771.86, 614.09, 124.48),
      (1787.19, 616.25, 124.48), (1775.57, 622.65, 124.27)],
     [29768, 29774, 29819], 29838, 29304),
    ((70, 255), "Halls of Stone: the Dark Rune hall", "Les salles de Pierre : la salle des Runes sombres", 599,
     (1032.76, 666.05, 202.71, 2.88), (1000.07, 674.69, 202.46, 6.02),
     [(1007.62, 680.39, 201.98), (986.87, 666.32, 202.87), (1014.99, 659.89, 201.98), (1008.86, 651.21, 201.98)],
     [27963, 27960, 27962], 27969, 27977),
    ((70, 255), "Halls of Lightning: the Stormforged gallery", "Les salles de Foudre : la galerie des Forge-foudre",
     602, (1061.99, -169.47, 56.71, 0.10), (1096.84, -166.11, 58.69, 3.24),
     [(1090.95, -171.73, 56.74), (1084.47, -168.47, 56.72), (1085.67, -175.53, 56.73), (1088.67, -147.48, 61.19),
      (1099.90, -142.79, 61.30), (1110.21, -146.21, 61.30)],
     [28837, 28581, 28836], 28838, 28586),
    ((70, 255), "Pit of Saron: the slave pens", "La fosse de Saron : les enclos", 658,
     (587.98, 198.15, 509.65, 4.78), (590.58, 161.38, 509.50, 1.64),
     [(586.03, 153.35, 510.67), (586.46, 171.18, 509.62), (592.98, 176.10, 510.16), (584.15, 177.79, 509.62),
      (572.31, 168.01, 509.94), (588.73, 183.09, 509.12)],
     [37711, 37712, 37713], 36879, 36494),
    ((70, 255), "The Forge of Souls: the soul hall", "La Forge des âmes : la salle des âmes", 632,
     (5164.60, 2339.48, 668.24, 0.85), (5181.87, 2359.08, 668.24, 3.99),
     [(5174.47, 2348.92, 668.24), (5185.28, 2340.88, 668.24), (5162.51, 2354.95, 668.24)],
     [36564, 36620, 36516], 36522, 36497),
]


def cpp_string(text):
    return '"' + text.replace('\\', '\\\\').replace('"', '\\"') + '"'


def main():
    copies = {}         # (stock entry, role) -> copy entry

    def copy_of(stock, role):
        key = (stock, role)
        if key not in copies:
            copies[key] = FIRST_COPY + len(copies)
            if copies[key] > LAST_COPY:
                raise SystemExit("out of copy entries")
        return copies[key]

    arenas = []
    for levels, name_en, name_fr, map_id, entry, boss, trash, trash_entries, elite, boss_entry in ARENAS:
        if len(trash) > 6 or len(trash_entries) > 3:
            raise SystemExit(f"{name_en}: too many trash spots or entries")
        arenas.append((levels, name_en, name_fr, map_id, entry, boss, trash,
                       [copy_of(e, ROLE_TRASH) for e in trash_entries], copy_of(elite, ROLE_ELITE),
                       copy_of(boss_entry, ROLE_BOSS)))

    def spot(values):
        return "{ " + ", ".join(f"{v:.2f}f" for v in values) + " }"

    lines = [
        "// Generated by localTools/infiniteDungeon/buildArenas.py: change the arenas there and run it again.",
        "#include \"InfiniteDungeonArenas.h\"",
        "",
        "namespace InfiniteDungeon",
        "{",
        "std::vector<Arena> const& GetArenas()",
        "{",
        "    static std::vector<Arena> const arenas = {",
    ]
    for levels, name_en, name_fr, map_id, entry, boss, trash, trash_entries, elite, boss_entry in arenas:
        entries = ", ".join(str(e) for e in trash_entries + [0] * (3 - len(trash_entries)))
        lines += [
            f"        {{ {cpp_string(name_en)},",
            f"          {cpp_string(name_fr)},",
            f"          {map_id}, {levels[0]}, {levels[1]},",
            f"          {spot(entry)},",
            f"          {spot(boss)},",
            "          { {",
        ]
        lines += [f"              {spot(t)}," for t in trash]
        lines += [
            f"          }} }}, {len(trash)},",
            f"          {{ {{ {entries} }} }}, {elite}, {boss_entry} }},",
        ]
    lines += [
        "    };",
        "    return arenas;",
        "}",
        "}",
        "",
    ]
    with open(CPP, "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(lines))

    roles = {ROLE_TRASH: "trash", ROLE_ELITE: "elite", ROLE_BOSS: "boss"}
    sql = [
        "-- Infinite Dungeon (Donjon infini) creatures: copies of stock dungeon creatures under their own entries, so the",
        "-- stock ones stay untouched. Generated by localTools/infiniteDungeon/buildArenas.py.",
        "-- A copy keeps its model, weapons, movement and French name; the floor sets its level, health and damage",
        "-- (InfiniteDungeon.cpp). No loot, no experience (the floor pays), our script, rank by role: trash normal, elite",
        "-- and boss elite, the boss with the boss portrait (type flag 4).",
        "",
        f"DELETE FROM `creature_template_locale` WHERE `entry` BETWEEN {FIRST_COPY} AND {LAST_COPY};",
        f"DELETE FROM `creature_template_movement` WHERE `CreatureId` BETWEEN {FIRST_COPY} AND {LAST_COPY};",
        f"DELETE FROM `creature_equip_template` WHERE `CreatureID` BETWEEN {FIRST_COPY} AND {LAST_COPY};",
        f"DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN {FIRST_COPY} AND {LAST_COPY};",
        f"DELETE FROM `creature_template` WHERE `entry` BETWEEN {FIRST_COPY} AND {LAST_COPY};",
        "",
        "DROP TEMPORARY TABLE IF EXISTS `tmp_infinite_dungeon_copy`;",
        "CREATE TEMPORARY TABLE `tmp_infinite_dungeon_copy` (",
        "    `entry` INT UNSIGNED NOT NULL,",
        "    `source` INT UNSIGNED NOT NULL,",
        "    `role` TINYINT UNSIGNED NOT NULL,",
        "    PRIMARY KEY (`entry`)",
        ") ENGINE=InnoDB;",
        "INSERT INTO `tmp_infinite_dungeon_copy` (`entry`, `source`, `role`) VALUES",
    ]
    rows = sorted(copies.items(), key=lambda item: item[1])
    sql += [f"    ({entry}, {stock}, {role}){',' if index < len(rows) - 1 else ';'} -- {roles[role]}"
            for index, ((stock, role), entry) in enumerate(rows)]
    sql += [
        "",
        "DROP TEMPORARY TABLE IF EXISTS `tmp_infinite_dungeon_template`;",
        "CREATE TEMPORARY TABLE `tmp_infinite_dungeon_template` LIKE `creature_template`;",
    ]
    for (stock, role), entry in rows:
        sql += [
            f"INSERT INTO `tmp_infinite_dungeon_template` SELECT * FROM `creature_template` WHERE `entry` = {stock};",
            f"UPDATE `tmp_infinite_dungeon_template` SET `entry` = {entry} WHERE `entry` = {stock};",
        ]
    sql += [
        "UPDATE `tmp_infinite_dungeon_template` t JOIN `tmp_infinite_dungeon_copy` c ON c.`entry` = t.`entry` SET",
        "    t.`difficulty_entry_1` = 0, t.`difficulty_entry_2` = 0, t.`difficulty_entry_3` = 0,",
        "    t.`KillCredit1` = 0, t.`KillCredit2` = 0, t.`gossip_menu_id` = 0, t.`npcflag` = 0, t.`faction` = 14,",
        "    t.`rank` = IF(c.`role` = 0, 0, 1),",
        "    t.`type_flags` = IF(c.`role` = 2, t.`type_flags` | 4, t.`type_flags` & ~4),",
        "    t.`unit_flags` = 0, t.`unit_flags2` = 0, t.`dynamicflags` = 0, t.`VehicleId` = 0,",
        "    t.`lootid` = 0, t.`pickpocketloot` = 0, t.`skinloot` = 0, t.`mingold` = 0, t.`maxgold` = 0,",
        "    t.`AIName` = '', t.`MovementType` = 0, t.`HealthModifier` = 1, t.`ManaModifier` = 1,",
        "    t.`ArmorModifier` = 1, t.`DamageModifier` = 1, t.`ExperienceModifier` = 1, t.`RegenHealth` = 1,",
        "    t.`CreatureImmunitiesId` = 0, t.`flags_extra` = 64, t.`ScriptName` = 'npc_infinite_dungeon_creature',",
        "    t.`detection_range` = IF(c.`role` = 2, 10, 12),",
        "    t.`VerifiedBuild` = NULL;",
        "INSERT INTO `creature_template` SELECT * FROM `tmp_infinite_dungeon_template`;",
        "",
        "INSERT INTO `creature_template_model`",
        "    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)",
        "SELECT c.`entry`, m.`Idx`, m.`CreatureDisplayID`, m.`DisplayScale`, m.`Probability`, NULL",
        "FROM `tmp_infinite_dungeon_copy` c JOIN `creature_template_model` m ON m.`CreatureID` = c.`source`;",
        "",
        "INSERT INTO `creature_equip_template` (`CreatureID`, `ID`, `ItemID1`, `ItemID2`, `ItemID3`, `VerifiedBuild`)",
        "SELECT c.`entry`, e.`ID`, e.`ItemID1`, e.`ItemID2`, e.`ItemID3`, NULL",
        "FROM `tmp_infinite_dungeon_copy` c JOIN `creature_equip_template` e ON e.`CreatureID` = c.`source`;",
        "",
        "INSERT INTO `creature_template_movement`",
        "    (`CreatureId`, `Ground`, `Swim`, `Flight`, `Rooted`, `Chase`, `Random`, `InteractionPauseTimer`)",
        "SELECT c.`entry`, v.`Ground`, v.`Swim`, v.`Flight`, 0, v.`Chase`, 0, v.`InteractionPauseTimer`",
        "FROM `tmp_infinite_dungeon_copy` c JOIN `creature_template_movement` v ON v.`CreatureId` = c.`source`;",
        "",
        "INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`)",
        "SELECT c.`entry`, l.`locale`, l.`Name`, l.`Title`, NULL",
        "FROM `tmp_infinite_dungeon_copy` c JOIN `creature_template_locale` l ON l.`entry` = c.`source`;",
        "",
        "DROP TEMPORARY TABLE `tmp_infinite_dungeon_template`;",
        "DROP TEMPORARY TABLE `tmp_infinite_dungeon_copy`;",
        "",
    ]
    with open(SQL, "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(sql))
    print(f"{len(arenas)} arenas, {len(copies)} creature copies ({FIRST_COPY}-{FIRST_COPY + len(copies) - 1})")


if __name__ == "__main__":
    main()
