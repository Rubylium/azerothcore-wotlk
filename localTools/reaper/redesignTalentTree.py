"""The Faucheur's talent trees redesigned (.agents/plans/reaper-talents/reaper-talents.DESIGN.md), from the imported ones.

Run once over localTools/reaper/talentTree.json (it reads the imported trees from git's copy of that file, so it can be
run again): each tree gets its own shape, merged nodes keep their source nodes' rank spells (mod-reaper reads them),
the spec's core abilities, its raid buff and its flat stats move to the base kit (the spec tree's specSpells; their
rank spells kept generated through `baseRanks`), a few nodes are dropped. Points: one every 3 levels (24 at 80).

Usage: python localTools/reaper/redesignTalentTree.py
"""

import copy
import json
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PATH = REPO / "localTools" / "reaper" / "talentTree.json"
SOURCE = "33364f3e3e:localTools/reaper/talentTree.json"   # the imported trees, as last laid out before the redesign

LEVEL_STEP = 3
POINTS = {1: (80 - 10) // LEVEL_STEP + 1, 2: (80 - 11) // LEVEL_STEP + 1}

old = json.loads(subprocess.run(["git", "-C", str(REPO), "show", SOURCE], capture_output=True, check=True)
                 .stdout.decode("utf-8"))
nodes = {node["id"]: node for tree in old["trees"] for node in tree["nodes"]}
next_spell = [98533]


def new_spell():
    spell = next_spell[0]
    next_spell[0] += 1
    return spell


def take(node_id, row, col, parents=(), **changes):
    node = copy.deepcopy(nodes[node_id])
    node.update(row=row, col=col)
    node.pop("parents", None)
    if parents:
        node["parents"] = list(parents)
    node.update(changes)
    return node


def two_ranks(node_id, row, col, parents, low):
    """An aura node given a lower first rank: a new spell for it, its own spell the top rank"""
    node = take(node_id, row, col, parents)
    top = node["values"][-1]
    node["spells"] = [new_spell()] + node["spells"]
    node["values"] = [low, top]
    if "auraValues" in node:
        node["auraValues"] = [node["auraValues"][-1] * low // top, node["auraValues"][-1]]
    for aura in node["aura"] if isinstance(node.get("aura"), list) else []:
        if "values" in aura:
            aura["values"] = [aura["values"][-1] * low // top, aura["values"][-1]]
    return node


def merged(node_id, row, col, parents, spells, values, text, aura=None, name=None):
    node = take(node_id, row, col, parents)
    node.update(spells=spells, values=values, text=text)
    node.pop("auraValues", None)
    if aura is not None:
        node["aura"] = aura
    else:
        node.pop("aura", None)
    if name:
        node["name"] = name
    return node


def aura_of(node_id, values, index=None):
    aura = nodes[node_id]["aura"]
    aura = copy.deepcopy(aura[index] if index is not None else aura)
    aura["values"] = values
    return aura


def spell_of(node_id):
    return nodes[node_id]["spells"][0]


def base(node_id):
    """A node moved to the base kit: kept as it is, its rank spells still generated"""
    node = copy.deepcopy(nodes[node_id])
    for key in ("row", "col", "parents"):
        node.pop(key, None)
    return node


# --- The class tree: "Le Sablier" -----------------------------------------------------------------------------------
class_nodes = [
    take(101, 0, 1), take(104, 0, 3), take(109, 0, 5),
    merged(108, 1, 0, [101], [98405, 98406, 98407], [5, 10, 10],
           "Augmente de {0} m la portée de Foulée spectrale et de {0} la puissance runique qu'elle génère. "
           "Au rang 3, Foulée spectrale génère aussi une Âme moissonnée.",
           aura=[aura_of(108, [5, 10, 10], 0), aura_of(108, [50, 100, 100], 1)]),
    take(106, 1, 2, [101, 104]), take(115, 1, 4, [104, 109]), two_ranks(135, 1, 6, [109], 10),
    take(113, 2, 1, [108, 106]), take(110, 2, 3, [106, 115]), take(118, 2, 5, [115, 135]),
    merged(117, 3, 2, [113, 110], [98412, 98414], [100, 100],
           "Générer une Âme moissonnée déchaîne Moisson d'âme sur votre cible : des dégâts d'Ombre, et vous récupérez "
           "{0}% des dégâts infligés. Au rang 2, ses soins augmentent de 50%."),
    take(130, 3, 4, [110, 118]),
    take(116, 4, 3, [117, 130]),
    merged(131, 5, 2, [116], [98422, 98423], [15, 15],
           "Obtenir l'Infusion d'âme augmente votre armure de {0}% pendant 15 s. Au rang 2, elle vous empêche aussi "
           "d'être ralenti sous 70% de votre vitesse normale pendant 6 s.", name="Infusion protectrice"),
    take(123, 5, 4, [116]),
    take(124, 6, 1, [131]), take(125, 6, 3, [131, 123]), take(126, 6, 5, [123]),
    take(128, 7, 0, [124]), two_ranks(129, 7, 2, [124, 125], 15), take(134, 7, 4, [125, 126]),
    take(127, 7, 6, [126]),
]
# Lame spectrale and the class's filler: everyone's, from the base kit
class_base = [base(102), base(107), base(114), base(119)]
class_spec_spells = [spell_of(103), 98401, 98404, 98410, 98413]

# --- Moisson: "La Faux" ----------------------------------------------------------------------------------------------
moisson_nodes = [
    take(226, 0, 4), merged(211, 0, 6, [], [98435, 98436, 98437], [5, 10, 10],
                            "Augmente de {0}% les chances de coup critique de Lacération funeste. Au rang 3, ses "
                            "coups critiques vous rendent une Âme moissonnée.",
                            aura=[aura_of(211, [5, 10, 10])]),
    merged(205, 1, 3, [226], [98430, 98445], [25, 25],
           "Augmente de {0}% les dégâts de Faucher et de Meurtre. Au rang 2, augmente aussi de 30% les dégâts de "
           "Lacération funeste et de Massacre.",
           aura=[aura_of(205, [25, 25]), aura_of(222, [0, 30])]),
    take(203, 1, 5, [226, 211]),
    take(209, 2, 2, [205]), take(206, 2, 4, [205, 203]),
    take(216, 3, 1, [209]), take(220, 3, 3, [209, 206]), take(221, 3, 5, [206]),
    take(217, 4, 0, [216]), take(214, 4, 2, [216, 220]), take(218, 4, 4, [220, 221]),
    merged(215, 4, 6, [221], [98439, 98453], [10, 10],
           "Augmente de {0}% les soins de Moissonneur, et de 25% de plus tant que vous vous tenez dans votre Champ "
           "de moisson. Au rang 2, augmente aussi de 30% les soins absorbés par Lacération funeste.",
           aura=[aura_of(230, [0, 30])]),
    take(213, 5, 1, [217, 214]), take(210, 5, 3, [214, 218]), take(233, 5, 5, [218, 215]),
    take(229, 6, 0, [213]), two_ranks(225, 6, 2, [213, 210], 5), take(223, 6, 4, [210, 233]),
    take(231, 7, 0, [229]), take(232, 7, 2, [225]), two_ranks(237, 7, 4, [225, 223], 5),
    take(234, 7, 6, [223]),
    take(219, 8, 3, [232, 237]),
    take(238, 9, 4, [219]),
]
moisson_base = [base(204), base(228), base(236)]
moisson_spec_spells = [spell_of(201), spell_of(202), spell_of(208), 98429, 98451, spell_of(236)]

# --- Âme: "Entre deux mondes" ----------------------------------------------------------------------------------------
ame_nodes = [
    merged(306, 0, 1, [], [98468, 98469], [15, 15],
           "Augmente de {0}% les dégâts de Chasse-mort et de 3% vos chances de toucher. Au rang 2, Chasse-mort "
           "génère 15 points de puissance runique supplémentaires.",
           aura=[aura_of(306, [15, 15], 0), aura_of(306, [3, 3], 1),
                 dict(copy.deepcopy(nodes[307]["aura"]), values=[0, 150])]),
    two_ranks(312, 0, 5, [], 5),
    take(311, 1, 0, [306]),
    merged(303, 1, 2, [306], [98462, 98463], [10, 20],
           "Augmente de {0}% les dégâts de Faucher, de Complainte et de Reliquaire des perdus, et de 15% par rang "
           "ceux de Vent de mort, de Meurtre et de Frappe d'âme.",
           aura=[aura_of(303, [10, 20]), aura_of(305, [15, 30])]),
    take(313, 1, 4, [312]), two_ranks(310, 1, 6, [312], 5),
    take(308, 2, 1, [311, 303]), take(314, 2, 3, [303, 313]), take(327, 2, 5, [313, 310]),
    take(309, 3, 3, [308, 327]),
    take(315, 4, 1, [309]), take(316, 4, 3, [309]), take(328, 4, 5, [309]),
    take(318, 5, 0, [315]), take(321, 5, 2, [315, 316]), take(304, 5, 4, [316, 328]), take(320, 5, 6, [328]),
    two_ranks(323, 6, 1, [318, 321], 25), take(317, 6, 3, [321, 304]),
    merged(325, 6, 5, [304, 320], [98485, 98484], [30, 30],
           "Augmente de {0}% les dégâts de Foulée spectrale et réduit les chances des ennemis de vous détecter "
           "camouflé. Au rang 2, Foulée spectrale inflige en plus 25% de ses dégâts sur 6 s.",
           aura=[aura_of(325, [30, 30], 0), aura_of(325, [15, 15], 1)]),
    take(326, 7, 0, [323]), take(336, 7, 2, [323, 317]), take(331, 7, 4, [317, 325]), two_ranks(337, 7, 6, [325], 3),
    take(330, 8, 2, [326, 336]), take(332, 8, 4, [331, 337]),
    take(333, 9, 3, [330, 332]),
]
ame_base = [base(329)]
ame_spec_spells = [spell_of(301), spell_of(302), 98489]

# --- Domination: "Le Rempart" ----------------------------------------------------------------------------------------
domination_nodes = [
    take(409, 0, 3),
    merged(410, 1, 2, [409], [98504, 98505, 98506], [3, 3, 3],
           "Frappe d'âme vous rend {0}% de vos points de vie manquants de plus. Au rang 2, elle inflige aussi des "
           "dégâts supplémentaires égaux à 30% de votre Force. Au rang 3, elle coûte 10 points de puissance runique "
           "de moins.", aura=[dict(copy.deepcopy(nodes[412]["aura"]), values=[0, 0, -100])],
           name="Frappe dévorante"),
    merged(405, 1, 4, [409], [98499, 98500, 98529], [2, 3, 4],
           "Réduit de {0}% les dégâts magiques que vous subissez. Au rang 3, augmente aussi de 2% vos chances de "
           "parer et d'esquiver.",
           aura=[aura_of(405, [-2, -3, -4], 1), {"type": 47, "values": [0, 0, 2]}, {"type": 49, "values": [0, 0, 2]}],
           name="Rempart d'âme"),
    take(406, 2, 1, [410]), take(413, 2, 3, [410, 405]), two_ranks(407, 2, 5, [405], 3),
    take(418, 3, 0, [406]), take(415, 3, 2, [406, 413]), take(417, 3, 4, [413, 407]), take(423, 3, 6, [407]),
    take(420, 4, 0, [418]), take(416, 4, 2, [415, 417]), take(424, 4, 4, [417]), take(425, 4, 6, [423]),
    take(419, 5, 1, [420, 416]), take(426, 5, 3, [416, 424]), take(429, 5, 5, [424, 425]),
    take(427, 6, 1, [419]), take(428, 6, 3, [426]), take(430, 6, 5, [429]),
    take(432, 7, 0, [427]), take(433, 7, 2, [427, 428]), take(434, 7, 4, [428, 430]), take(436, 7, 6, [430]),
    take(437, 8, 3, [433, 434]),
]
domination_base = [base(404), base(408), base(421), base(105), base(112)]
domination_spec_spells = [spell_of(401), spell_of(402), spell_of(403), 98498, 98503, 98515, 98402, 98409]


def order_of(tree_nodes):
    """Bot order: top to bottom, left to right, first option of a choice"""
    order = []
    for node in sorted(tree_nodes, key=lambda node: (node["row"], node["col"])):
        order.append(f"{node['id']}:1" if node["kind"] == "choice" else node["id"])
    return order


def fill(tree_nodes, points, gates, skip=()):
    """A preset: nodes in order, full ranks, as far as the points go, gates and parents respected"""
    picked, spent = {}, 0
    by_row = sorted(tree_nodes, key=lambda node: (node["row"], node["col"]))
    progress = True
    while progress and spent < points:
        progress = False
        for node in by_row:
            if node["id"] in picked or node["id"] in skip:
                continue
            ranks = 1 if node["kind"] in ("active", "choice") else len(node["spells"])
            if spent + ranks > points:
                continue
            needed = max([gate["cost"] for gate in gates if node["row"] >= gate["row"]] or [0])
            above = sum(value for other, value in picked.items()
                        if next(n for n in tree_nodes if n["id"] == other)["row"] < node["row"])
            if above < needed:
                continue
            parents = node.get("parents", [])
            if parents and not any(parent in picked for parent in parents):
                continue
            picked[node["id"]] = ranks
            spent += ranks
            progress = True
    return picked


new = copy.deepcopy(old)
new["_comment"] = [
    "The Faucheur's retail-style talent trees (format: localTools/oathblade/talentTree.json), redesigned from the",
    "imported Ascension Reaper (localTools/reaper/redesignTalentTree.py, .agents/plans/reaper-talents/): a shape per",
    "tree - the class tree an hourglass, Moisson a scythe, Âme two crossing lanes, Domination a rampart - the specs'",
    "core abilities, raid buffs and flat stats in their base kit (specSpells; their rank spells kept by `baseRanks`).",
    "Active nodes name abilities authored in localTools/reaper/Spells.ps1; the other ranks are generated by",
    "localTools/talentTree/TalentRankSpells.ps1, real auras where they declare one and otherwise dummies",
    "modules/mod-reaper reads. Rank ids 98400-98799. One point every 3 levels: 24 a tree at 80.",
]
layouts = {1: (class_nodes, class_base, []), 2: (moisson_nodes, moisson_base, moisson_spec_spells),
           3: (ame_nodes, ame_base, ame_spec_spells), 4: (domination_nodes, domination_base, domination_spec_spells)}
for tree in new["trees"]:
    tree_nodes, tree_base, extra = layouts[tree["id"]]
    tree["nodes"] = tree_nodes
    tree["baseRanks"] = tree_base
    tree["levelStep"] = LEVEL_STEP
    if tree["kind"] == "spec":
        tree["specSpells"] = list(dict.fromkeys(tree["specSpells"] + extra + class_spec_spells))
    if "botBuild" in tree:
        tree["botBuild"] = order_of(tree_nodes)
    ids = [node["id"] for node in tree_nodes]
    assert len(ids) == len(set(ids)), tree["name"]
    cells = [(node["row"], node["col"]) for node in tree_nodes]
    assert len(cells) == len(set(cells)), tree["name"]
    for node in tree_nodes:
        for parent in node.get("parents", []):
            other = next(n for n in tree_nodes if n["id"] == parent)
            assert other["row"] == node["row"] - 1 and abs(other["col"] - node["col"]) <= 2, (tree["name"], node["id"])

class_tree = new["trees"][0]
class_fill = fill(class_nodes, POINTS[1], class_tree["gates"])
presets = []
for tree in new["trees"][1:]:
    spec_fill = fill(tree["nodes"], POINTS[2], tree["gates"])
    for kind in ("single", "aoe"):
        picks = {str(node_id): (1 if value and next(n for n in tree["nodes"] if n["id"] == node_id)["kind"] ==
                                "choice" else value) for node_id, value in spec_fill.items()}
        presets.append({"spec": tree["id"], "kind": kind,
                        "nodes": {**{str(k): v for k, v in class_fill.items()}, **picks}})
new["presets"] = presets

PATH.write_text(json.dumps(new, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
for tree in new["trees"]:
    ranks = sum(1 if node["kind"] in ("active", "choice") else len(node["spells"]) for node in tree["nodes"])
    points = POINTS[1 if tree["kind"] == "class" else 2]
    print(f"{tree['name']}: {len(tree['nodes'])} nodes, {ranks} ranks for {points} points, "
          f"base kit {len(tree['baseRanks'])} ranks, spec spells {tree.get('specSpells', [])}")
print("new rank spells", 98533, "-", next_spell[0] - 1)
