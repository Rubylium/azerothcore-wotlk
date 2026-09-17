import os
import re
import shutil

TEMP = os.environ['TEMP']
SRC = os.path.join(TEMP, 'claude', 'slglue', 'Interface', 'GlueXML')
OUT = os.path.join(TEMP, 'claude', 'spike', 'payload', 'Interface', 'GlueXML')
CLASS_TOKEN = 'TESTCLASS'


def replace_once(text, pattern, replacement):
    new, count = re.subn(pattern, replacement, text, count=1)
    assert count == 1, pattern
    return new


lua = open(os.path.join(SRC, 'CharacterCreate.lua'), encoding='latin-1', newline='').read()

# French labels (the file is saved as UTF-8 below, like the client's own GlueStrings)
lua = replace_once(lua, r'CLASS_DISABLED = "[^"]*";',
                   'CLASS_DISABLED = "Vous devez choisir une autre race pour jouer cette classe";')
lua = replace_once(lua, r'CUSTOMIZE = "Appearance";', 'CUSTOMIZE = "Apparence";')
lua = replace_once(lua, r'NEXT = "Appearance";', 'NEXT = "Apparence";')
lua = replace_once(lua, r'FINISH = "Finish";', 'FINISH = "Terminer";')
lua = replace_once(lua, r'text = "Not in GlueXML\."', 'text = ""')

# Custom class icon and texts
lua = replace_once(lua, r'(\t\["ENGINEER"\] = \{[^}]*\})',
                   r'\1,\r\n\t["' + CLASS_TOKEN + r'"] = {0.5, 0.75, 0.5, 0.75}')
lua += ('\r\n-- Custom classes\r\n'
        'CLASS_' + CLASS_TOKEN + ' = "Classe de test.";\r\n'
        'CLASS_INFO_' + CLASS_TOKEN + '0 = "- Classe de test";\r\n')

# The original blocks the 11th class button (reserved for an unused "Engineer" class): it is our class now
lua = replace_once(lua, r'\tif id == 11 then\r?\n\t\treturn\r?\n\tend\r?\n', '')

if os.path.exists(OUT):
    shutil.rmtree(OUT)
os.makedirs(OUT)
open(os.path.join(OUT, 'CharacterCreate.lua'), 'w', encoding='utf-8', newline='').write(lua)
for name in ('CharacterCreate.xml', 'GlueParent.lua'):
    shutil.copyfile(os.path.join(SRC, name), os.path.join(OUT, name))
print('Shadowlands GlueXML adapted in', OUT)

# Class row layout: the original anchors button 11 to button 6 but declares it first, so the anchor never
# resolves and button 11 is not drawn. Declare it after button 6, and space the 11 buttons wider (110 px,
# row kept centered) so long names such as "Chevalier de la mort" do not run into their neighbour.
xml_path = os.path.join(OUT, 'CharacterCreate.xml')
xml = open(xml_path, encoding='latin-1', newline='').read()
button11 = re.search(r'[ \t]*<CheckButton name="CharCreateClassButton11".*?</CheckButton>\r?\n', xml, re.S).group(0)
xml = xml.replace(button11, '', 1)
button6_end = re.search(r'<CheckButton name="CharCreateClassButton6".*?</CheckButton>\r?\n', xml, re.S).end()
xml = xml[:button6_end] + button11 + xml[button6_end:]
xml, spaced = re.subn(r'(relativeTo="CharCreateClassButton\d+" relativePoint="BOTTOMLEFT" )x="100"', r'\1x="110"', xml)
assert spaced == 10, spaced
xml = replace_once(xml, r'(<CheckButton name="CharCreateClassButton1" [^>]*>\s*<Anchors>\s*<Anchor point="BOTTOM" )x="-325"',
                   r'\1x="-425"')
open(xml_path, 'w', encoding='latin-1', newline='').write(xml)
print('class row fixed')

# Retail-style round icons (textures from make_round_icons.py): round atlases, round shadow / hover / selected
# glows, no square bevel, exact atlas cells for the class icons.
BACKSLASH = chr(92)
GLUES = BACKSLASH.join(['Interface', 'Glues', 'CharacterCreate', ''])
xml = open(xml_path, encoding='latin-1', newline='').read()
for old, new in (('UI-CharacterCreate-Races', 'RoundRaces'), ('UI-CharacterCreate-Classes', 'RoundClasses'),
                 ('UI-CharacterCreate-Gender', 'RoundGender')):
    xml, count = re.subn('file="' + re.escape(GLUES + old) + '"',
                         lambda _m, new=new: 'file="' + GLUES + new + '"', xml, flags=re.I)
    assert count == 2, (old, count)


def rework_template(xml, name, button_size):
    start = xml.index('<CheckButton name="' + name + '"')
    end = xml.index('</CheckButton>', start)
    block = xml[start:end]
    glow_size = int(button_size * 1.36)

    def glow_element(tag, texture):
        return ('<' + tag + ' file="' + GLUES + texture + '" alphaMode="ADD">\r\n'
                '\t\t\t<Size x="' + str(glow_size) + '" y="' + str(glow_size) + '"/>\r\n'
                '\t\t\t<Anchors>\r\n\t\t\t\t<Anchor point="CENTER" x="0" y="0"/>\r\n\t\t\t</Anchors>\r\n'
                '\t\t</' + tag + '>')

    block, count = re.subn(r'<HighlightTexture\b.*?</HighlightTexture>',
                           lambda _m: glow_element('HighlightTexture', 'RoundHighlight'), block, flags=re.S)
    assert count == 1, (name, 'highlight', count)
    block, count = re.subn(r'<CheckedTexture\b.*?</CheckedTexture>',
                           lambda _m: glow_element('CheckedTexture', 'RoundSelected'), block, flags=re.S)
    assert count == 1, (name, 'checked', count)
    shadow_size = int(button_size * 1.2)
    block, count = re.subn(
        r'<Texture name="\$parentShadow"[^>]*>.*?</Texture>',
        lambda _m: ('<Texture name="$parentShadow" file="' + GLUES + 'RoundShadow">\r\n'
                    '\t\t\t\t\t<Size x="' + str(shadow_size) + '" y="' + str(shadow_size) + '"/>\r\n'
                    '\t\t\t\t\t<Anchors>\r\n\t\t\t\t\t\t<Anchor point="CENTER" x="0" y="-1"/>\r\n'
                    '\t\t\t\t\t</Anchors>\r\n\t\t\t\t</Texture>'), block, count=1, flags=re.S)
    assert count == 1, (name, 'shadow', count)
    # Square bevel overlays (live ones only; commented-out ones stay as they are)
    block = re.sub(r'(?<!<!-- )<Texture name="\$parentBevelEdge" ', '<Texture name="$parentBevelEdge" hidden="true" ',
                   block)
    return xml[:start] + block + xml[end:]


xml = rework_template(xml, 'CharCreateIconButtonTemplate', 52)
xml = rework_template(xml, 'CharCreateClassButtonTemplate', 52)
xml = rework_template(xml, 'CharacterCreateGenderButtonTemplate', 38)
open(xml_path, 'w', encoding='latin-1', newline='').write(xml)

lua_path = os.path.join(OUT, 'CharacterCreate.lua')
lua = open(lua_path, encoding='utf-8', newline='').read()
table_start = lua.index('CLASS_ICON_TCOORDS = {')
table_end = lua.index('};', table_start)
table = lua[table_start:table_end]
for old, new in (('0.49609375', '0.5'), ('0.7421875', '0.75'), ('0.98828125', '1.0')):
    table = table.replace(old, new)
lua = lua[:table_start] + table + lua[table_end:]
open(lua_path, 'w', encoding='utf-8', newline='').write(lua)
print('round icon textures wired')
