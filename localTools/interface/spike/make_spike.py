import os
import shutil
import struct

TEMP = os.environ['TEMP']
GLUE_SRC = os.path.join(TEMP, 'claude', 'glue')
OUT = os.path.join(TEMP, 'claude', 'spike', 'payload')
SERVER_DBC = 'C:/Users/alexi/Documents/GitHub/TestWoW/server/Data/dbc/'

CLASS_ID = 10
CLASS_NAME = 'Testeur'
CLASS_TOKEN = 'TESTCLASS'
RACES = [1, 2, 3, 4, 5, 6, 7, 8, 10, 11]


def read_dbc(path):
    data = open(path, 'rb').read()
    assert data[:4] == b'WDBC'
    count, fields, record_size, string_size = struct.unpack_from('<4I', data, 4)
    records = [bytearray(data[20 + i * record_size:20 + (i + 1) * record_size]) for i in range(count)]
    strings = bytearray(data[20 + count * record_size:])
    assert len(strings) == string_size
    return fields, record_size, records, strings


def write_dbc(path, fields, record_size, records, strings):
    header = b'WDBC' + struct.pack('<4I', len(records), fields, record_size, len(strings))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as file:
        file.write(header)
        for record in records:
            file.write(record)
        file.write(strings)


def add_string(strings, value):
    offset = len(strings)
    strings += value.encode('utf8') + b'\0'
    return offset


def build_chrclasses(source, target):
    fields, record_size, records, strings = read_dbc(source)
    assert fields == 60
    assert all(struct.unpack_from('<I', r, 0)[0] != CLASS_ID for r in records)
    record = bytearray(records[0])  # clone Warrior
    struct.pack_into('<I', record, 0, CLASS_ID)
    name = add_string(strings, CLASS_NAME)
    token = add_string(strings, CLASS_TOKEN)
    for group in (4, 21, 38):  # name, female name, male name: 16 locale slots + flags
        for locale in range(16):
            struct.pack_into('<I', record, (group + locale) * 4, name)
    struct.pack_into('<I', record, 55 * 4, token)
    struct.pack_into('<I', record, 58 * 4, 0)  # no cinematic
    struct.pack_into('<I', record, 59 * 4, 0)  # no expansion requirement
    records.append(record)
    write_dbc(target, fields, record_size, records, strings)


def build_charbaseinfo(source, target):
    fields, record_size, records, strings = read_dbc(source)
    assert record_size == 2
    existing = {(r[0], r[1]) for r in records}
    for race in RACES:
        if (race, CLASS_ID) not in existing:
            records.append(bytearray([race, CLASS_ID]))
    write_dbc(target, fields, record_size, records, strings)


def patch_glue():
    glue = os.path.join(GLUE_SRC, 'Interface', 'GlueXML')
    lua = open(os.path.join(glue, 'CharacterCreate.lua'), encoding='utf8', newline='').read()
    old = 'MAX_CLASSES_PER_RACE = 10;'
    assert lua.count(old) == 1
    lua = lua.replace(old, 'MAX_CLASSES_PER_RACE = 11;')
    old = '\t["DEATHKNIGHT"]\t= {0.25, 0.49609375, 0.5, 0.75},'
    assert lua.count(old) == 1
    lua = lua.replace(old, old + '\r\n\t["' + CLASS_TOKEN + '"]\t= {0, 0.25, 0, 0.25},')
    lua += ('\r\n-- Custom class test\r\nCLASS_' + CLASS_TOKEN + ' = "Classe de test.";\r\n'
            'CLASS_INFO_' + CLASS_TOKEN + '0 = "- Classe de test";\r\n')

    xml = open(os.path.join(glue, 'CharacterCreate.xml'), encoding='utf8', newline='').read()
    anchor = ('<CheckButton name="CharacterCreateClassButton10" inherits="CharacterCreateClassButtonTemplate" '
              'id="10">')
    start = xml.index(anchor)
    end = xml.index('</CheckButton>', start) + len('</CheckButton>')
    button10 = xml[start:end]
    button11 = (button10.replace('CharacterCreateClassButton10', 'CharacterCreateClassButton11')
                .replace('id="10"', 'id="11"')
                .replace('relativeTo="CharacterCreateClassButton5"', 'relativeTo="CharacterCreateClassButton6"'))
    xml = xml[:end] + '\r\n\t\t\t\t\t\t\t' + button11 + xml[end:]

    target = os.path.join(OUT, 'Interface', 'GlueXML')
    os.makedirs(target, exist_ok=True)
    open(os.path.join(target, 'CharacterCreate.lua'), 'w', encoding='utf8', newline='').write(lua)
    open(os.path.join(target, 'CharacterCreate.xml'), 'w', encoding='utf8', newline='').write(xml)


if os.path.exists(OUT):
    shutil.rmtree(OUT)

client_dbc = os.path.join(GLUE_SRC, 'DBFilesClient')
build_chrclasses(os.path.join(client_dbc, 'ChrClasses.dbc'), os.path.join(OUT, 'DBFilesClient', 'ChrClasses.dbc'))
build_charbaseinfo(os.path.join(client_dbc, 'CharBaseInfo.dbc'),
                   os.path.join(OUT, 'DBFilesClient', 'CharBaseInfo.dbc'))
patch_glue()

# Server copies, from untouched backups
for name in ('ChrClasses', 'CharBaseInfo'):
    backup = SERVER_DBC + name + '.before-new-class-spike.dbc'
    if not os.path.exists(backup):
        shutil.copyfile(SERVER_DBC + name + '.dbc', backup)
build_chrclasses(SERVER_DBC + 'ChrClasses.before-new-class-spike.dbc', SERVER_DBC + 'ChrClasses.dbc')
build_charbaseinfo(SERVER_DBC + 'CharBaseInfo.before-new-class-spike.dbc', SERVER_DBC + 'CharBaseInfo.dbc')
print('spike files written to', OUT)
