"""Reading and editing 3.3.5 M2 models (version 264) in place: their textures and every colour they animate.

Only what recolouring needs. Nothing moves inside the file: colours are rewritten where they are, and a texture's new
name is appended at the end with its reference pointed there, so every other offset stays valid.

Layout (wowdev.wiki "M2", WotLK):
- an array is (count uint32, offset uint32) from the start of the file
- a track (M2Track) is interpolation uint16, global sequence int16, timestamps (array of arrays), values (array of
  arrays): 20 bytes, one inner array per animation sequence
- a particle track (M2PartTrack) is timestamps (array), values (array): 16 bytes, no per-sequence nesting
"""
import struct

HEADER_ARRAYS = {
    'sequences': 0x1C,
    'colors': 0x48,
    'textures': 0x50,
    'lights': 0x108,
    'ribbons': 0x120,
    'particles': 0x128,
}
SEQUENCE_SIZE = 64
COLOR_SIZE = 40            # color M2Track<C3Vector>, alpha M2Track<fixed16>
TEXTURE_SIZE = 16          # type, flags, filename array
LIGHT_SIZE = 156
RIBBON_SIZE = 176
PARTICLE_SIZE = 476

LIGHT_COLOR_TRACKS = (16, 56)          # ambient and diffuse colour, M2Track<C3Vector>
RIBBON_COLOR_TRACK = 36                # M2Track<C3Vector>, 0-1
PARTICLE_COLOR_TRACK = 260             # M2PartTrack<C3Vector>, 0-255
PARTICLE_GEOMETRY_NAME = 24            # a model the particle draws instead of a quad
PARTICLE_RECURSION_NAME = 32


class M2:
    def __init__(self, data):
        self.data = bytearray(data)
        if self.data[:4] != b'MD20':
            raise ValueError('not an MD20 model')
        self.version = self.u32(4)
        if self.version != 264:
            raise ValueError(f'M2 version {self.version}, expected 264 (3.3.5)')

    def u32(self, offset):
        return struct.unpack_from('<I', self.data, offset)[0]

    def array(self, offset):
        return struct.unpack_from('<II', self.data, offset)

    def records(self, name, size):
        count, offset = self.array(HEADER_ARRAYS[name])
        return [offset + index * size for index in range(count)]

    @property
    def skin_count(self):
        return self.u32(0x44)

    def string(self, offset):
        count, target = self.array(offset)
        return self.data[target:target + count].split(b'\0')[0].decode('latin-1') if count else ''

    # --- textures ---------------------------------------------------------------------------------------------
    def textures(self):
        """(record offset, type, file name) of every texture; type 0 has its file name in the model"""
        return [(offset, self.u32(offset), self.string(offset + 8)) for offset in self.records('textures',
                                                                                             TEXTURE_SIZE)]

    def rename_texture(self, record, name):
        encoded = name.encode('latin-1') + b'\0'
        # Appended on a 16-byte boundary, as the file's own blocks are
        while len(self.data) % 16:
            self.data.append(0)
        offset = len(self.data)
        self.data += encoded
        struct.pack_into('<II', self.data, record + 8, len(encoded), offset)

    # --- sequences --------------------------------------------------------------------------------------------
    def external_sequences(self):
        """(animation id, sub id) of the sequences whose keyframes live in a separate .anim file"""
        external = []
        for offset in self.records('sequences', SEQUENCE_SIZE):
            animation, sub = struct.unpack_from('<HH', self.data, offset)
            flags = self.u32(offset + 12)
            if not flags & 0x20:
                external.append((animation, sub))
        return external

    # --- colours ----------------------------------------------------------------------------------------------
    def _track_values(self, track):
        """offsets of every C3Vector a full M2Track holds, across its sequences"""
        count, offset = self.array(track + 12)
        found = []
        for sequence in range(count):
            values, target = self.array(offset + sequence * 8)
            found += [target + index * 12 for index in range(values)]
        return found

    def _part_track_values(self, track):
        count, target = self.array(track + 8)
        return [target + index * 12 for index in range(count)]

    def colour_slots(self):
        """(offset, scale) of every animated colour: scale 1 for 0-1 colours, 255 for particles' 0-255 ones"""
        slots = []
        for record in self.records('colors', COLOR_SIZE):
            slots += [(value, 1.0) for value in self._track_values(record)]
        for record in self.records('lights', LIGHT_SIZE):
            for track in LIGHT_COLOR_TRACKS:
                slots += [(value, 1.0) for value in self._track_values(record + track)]
        for record in self.records('ribbons', RIBBON_SIZE):
            slots += [(value, 1.0) for value in self._track_values(record + RIBBON_COLOR_TRACK)]
        for record in self.records('particles', PARTICLE_SIZE):
            slots += [(value, 255.0) for value in self._part_track_values(record + PARTICLE_COLOR_TRACK)]
        return slots

    def colour(self, offset):
        return struct.unpack_from('<fff', self.data, offset)

    def set_colour(self, offset, rgb):
        struct.pack_into('<fff', self.data, offset, *rgb)

    def particle_models(self):
        """the other models particles draw, which a recoloured copy would still show in their own colours"""
        names = []
        for record in self.records('particles', PARTICLE_SIZE):
            for field in (PARTICLE_GEOMETRY_NAME, PARTICLE_RECURSION_NAME):
                name = self.string(record + field)
                if name:
                    names.append(name)
        return names
