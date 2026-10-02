using System.Text;

namespace RetailImport;

/// <summary>
/// The pieces of a retail model file (Legion and later: chunked, MD21 wrapping the MD20 data) that the conversion
/// needs, plus the 3.3.5 (version 264) post-pass and validation that run after MultiConverter.
/// wowdev.wiki "M2" and "M2/.skin" describe every offset used here.
/// </summary>
public sealed class RetailM2
{
    public byte[] Md20 { get; }
    public Dictionary<string, byte[]> Chunks { get; } = new();
    public uint[] SkinFileIds { get; } = [];
    public uint[] TextureFileIds { get; } = [];

    public RetailM2(byte[] file)
    {
        if (Encoding.ASCII.GetString(file, 0, 4) == "MD20")
        {
            Md20 = file;
            return;
        }
        for (var pos = 0; pos + 8 <= file.Length;)
        {
            var tag = Encoding.ASCII.GetString(file, pos, 4);
            var size = BitConverter.ToInt32(file, pos + 4);
            Chunks[tag] = file.AsSpan(pos + 8, size).ToArray();
            pos += 8 + size;
        }
        Md20 = Chunks["MD21"];
        if (Chunks.TryGetValue("SFID", out var sfid))
            SkinFileIds = ReadIds(sfid);
        if (Chunks.TryGetValue("TXID", out var txid))
            TextureFileIds = ReadIds(txid);
    }

    public uint Version => BitConverter.ToUInt32(Md20, 4);
    public string Name
    {
        get
        {
            var (length, offset) = Array(0x08);
            return Encoding.ASCII.GetString(Md20, (int)offset, (int)length).TrimEnd('\0');
        }
    }
    public uint ViewCount => BitConverter.ToUInt32(Md20, 0x44);

    public uint[] GlobalLoops()
    {
        var (count, offset) = Array(0x14);
        return Enumerable.Range(0, (int)count).Select(i => BitConverter.ToUInt32(Md20, (int)offset + i * 4)).ToArray();
    }
    public (uint count, uint offset) Array(int headerOffset) =>
        (BitConverter.ToUInt32(Md20, headerOffset), BitConverter.ToUInt32(Md20, headerOffset + 4));

    /// <summary>Sequences whose keyframes live outside the model (.anim files): neither inline (0x20) nor
    /// alias (0x40).</summary>
    public List<(ushort id, ushort sub)> ExternalSequences()
    {
        var result = new List<(ushort, ushort)>();
        var (count, offset) = Array(0x1C);
        for (var i = 0; i < count; ++i)
        {
            var at = (int)offset + i * 64;
            var flags = BitConverter.ToUInt32(Md20, at + 0x0C);
            if ((flags & 0x20) == 0 && (flags & 0x40) == 0)
                result.Add((BitConverter.ToUInt16(Md20, at), BitConverter.ToUInt16(Md20, at + 2)));
        }
        return result;
    }

    /// <summary>The same file with only its MD21 chunk and the skin list: what MultiConverter is given.
    /// Leaving TXID out keeps its texture naming (which reorders duplicate texture ids) from running; ours
    /// runs after.</summary>
    public byte[] ForMultiConverter()
    {
        using var stream = new MemoryStream();
        foreach (var tag in new[] { "MD21", "SFID" })
        {
            if (!Chunks.TryGetValue(tag, out var chunk))
                continue;
            stream.Write(Encoding.ASCII.GetBytes(tag));
            stream.Write(BitConverter.GetBytes(chunk.Length));
            stream.Write(chunk);
        }
        return stream.ToArray();
    }

    private static uint[] ReadIds(byte[] chunk)
    {
        var ids = new uint[chunk.Length / 4];
        for (var i = 0; i < ids.Length; ++i)
            ids[i] = BitConverter.ToUInt32(chunk, i * 4);
        return ids;
    }
}

/// <summary>Edits and checks a converted version 264 model in place.</summary>
public sealed class ClassicM2
{
    public byte[] Data { get; private set; }

    // Header arrays of a 3.3.5 model and the size of one element of each
    private static readonly (string name, int offset, int size)[] HeaderArrays =
    [
        ("name", 0x08, 1), ("global loops", 0x14, 4), ("sequences", 0x1C, 64), ("sequence lookup", 0x24, 2),
        ("bones", 0x2C, 88), ("key bone lookup", 0x34, 2), ("vertices", 0x3C, 48), ("colors", 0x48, 40),
        ("textures", 0x50, 16), ("transparency", 0x58, 20), ("texture animations", 0x60, 60),
        ("texture replace", 0x68, 2), ("materials", 0x70, 4), ("bone lookup", 0x78, 2),
        ("texture lookup", 0x80, 2), ("texture unit lookup", 0x88, 2), ("transparency lookup", 0x90, 2),
        ("uv animation lookup", 0x98, 2), ("collision indices", 0xD8, 2), ("collision vertices", 0xE0, 12),
        ("collision normals", 0xE8, 12), ("attachments", 0xF0, 40), ("attachment lookup", 0xF8, 2),
        ("events", 0x100, 36), ("lights", 0x108, 156), ("cameras", 0x110, 100), ("camera lookup", 0x118, 2),
        ("ribbons", 0x120, 176), ("particles", 0x128, 476),
    ];

    // Global flags a 3.3.5 client knows: tilt x / y and the blend mode overrides (0x08, added by MultiConverter)
    private const uint KnownGlobalFlags = 0x01 | 0x02 | 0x08;

    public ClassicM2(byte[] data) => Data = data;

    private uint U32(int at) => BitConverter.ToUInt32(Data, at);
    private void SetU32(int at, uint value) => BitConverter.GetBytes(value).CopyTo(Data, at);
    public (uint count, uint offset) Array(int headerOffset) => (U32(headerOffset), U32(headerOffset + 4));

    public void MaskGlobalFlags() => SetU32(0x10, U32(0x10) & KnownGlobalFlags);

    /// <summary>Names every texture the model hardcodes (type 0) with its 3.3.5 path, appended at the end of the
    /// file; the others (type 2 = the item's own texture, from ItemDisplayInfo) carry no name.</summary>
    public void NameTextures(IReadOnlyList<string> names)
    {
        var (count, offset) = Array(0x50);
        if (names.Count != count)
            throw new Exception($"{count} textures in the model, {names.Count} names given");
        for (var i = 0; i < count; ++i)
        {
            var record = (int)offset + i * 16;
            // Stock 3.3.5 models give the other textures an empty name: one NUL byte
            var name = U32(record) != 0 ? "" : names[i];
            var encoded = Encoding.ASCII.GetBytes(name + "\0");
            var at = (Data.Length + 15) & ~15;
            var grown = new byte[at + encoded.Length];
            Buffer.BlockCopy(Data, 0, grown, 0, Data.Length);
            Buffer.BlockCopy(encoded, 0, grown, at, encoded.Length);
            Data = grown;
            SetU32(record + 8, (uint)encoded.Length);
            SetU32(record + 12, (uint)at);
        }
    }

    /// <summary>
    /// An item model only ever plays Stand. Stock 3.3.5 item models hold that one sequence and no sequence lookup;
    /// retail ones often carry the whole character animation list (the T21 shoulders: 239 sequences, ids up to
    /// 808, past the 505 rows of 3.3.5's AnimationData). Keeping only the first, a Stand, matches stock. Nothing
    /// else moves: every animated track still holds its sequence-0 keyframes first.
    /// </summary>
    public int KeepFirstStand()
    {
        var (count, offset) = Array(0x1C);
        if (count <= 1)
            return 0;
        if (BitConverter.ToUInt16(Data, (int)offset) != 0)
            throw new Exception("the first sequence is not Stand; this model needs its animations kept");
        SetU32(0x1C, 1);
        SetU32(0x24, 0);
        BitConverter.GetBytes((short)-1).CopyTo(Data, (int)offset + 0x3C);   // variationNext
        BitConverter.GetBytes((ushort)0).CopyTo(Data, (int)offset + 0x3E);   // aliasNext: itself
        return (int)count - 1;
    }

    public int TextureType(int index) => (int)U32((int)Array(0x50).offset + index * 16);

    private int HeaderSize => (U32(0x10) & 0x08) != 0 ? 0x138 : 0x130;

    /// <summary>
    /// With blend overrides (flag 0x08) the header grows by their array, 0x130-0x137. A retail file without them
    /// starts its data there, the model name first, and MultiConverter writes the new array over it: the name is
    /// written again at the end of the file.
    /// </summary>
    public void RestoreName(string name)
    {
        var (_, offset) = Array(0x08);
        if (offset >= HeaderSize)
            return;
        var encoded = Encoding.ASCII.GetBytes(name + "\0");
        var at = (Data.Length + 15) & ~15;
        var grown = new byte[at + encoded.Length];
        Buffer.BlockCopy(Data, 0, grown, 0, Data.Length);
        Buffer.BlockCopy(encoded, 0, grown, at, encoded.Length);
        Data = grown;
        SetU32(0x08, (uint)encoded.Length);
        SetU32(0x0C, (uint)at);
    }

    /// <summary>
    /// The same overwrite, for the global loops: a model whose data starts with them (a name of its own elsewhere:
    /// the helmets) has its first loop durations under the blend override array. They are written again at the end
    /// of the file, from the retail model's.
    /// </summary>
    public void RestoreGlobalLoops(uint[] loops)
    {
        var (count, offset) = Array(0x14);
        if (count == 0 || offset >= HeaderSize)
            return;
        if (count != loops.Length)
            throw new Exception($"{count} global loops in the model, {loops.Length} in the retail one");
        var at = (Data.Length + 15) & ~15;
        var grown = new byte[at + loops.Length * 4];
        Buffer.BlockCopy(Data, 0, grown, 0, Data.Length);
        for (var i = 0; i < loops.Length; ++i)
            BitConverter.GetBytes(loops[i]).CopyTo(grown, at + i * 4);
        Data = grown;
        SetU32(0x18, (uint)at);
    }

    /// <summary>
    /// Moves the model from where retail places it to where 3.3.5 does: p' = (p - (x, 0, z)) / scale, on everything
    /// that holds a position (vertices, bone pivots and translation keyframes, attachments, events, lights, ribbons,
    /// particles, cameras, collision, the bounds, the skins' sort centres). Retail re-fitted the helmets to its new
    /// character models by moving them forward and up (Importer.HeadFits).
    /// </summary>
    public void Refit(float scale, float x, float z, IReadOnlyList<byte[]> skins)
    {
        float F(int at) => BitConverter.ToSingle(Data, at);
        void SetF(int at, float value) => BitConverter.GetBytes(value).CopyTo(Data, at);
        void Point(byte[] data, int at)
        {
            BitConverter.GetBytes((BitConverter.ToSingle(data, at) - x) / scale).CopyTo(data, at);
            BitConverter.GetBytes(BitConverter.ToSingle(data, at + 4) / scale).CopyTo(data, at + 4);
            BitConverter.GetBytes((BitConverter.ToSingle(data, at + 8) - z) / scale).CopyTo(data, at + 8);
        }
        void Each(int headerOffset, int size, int position)
        {
            var (count, offset) = Array(headerOffset);
            for (var i = 0; i < count; ++i)
                Point(Data, (int)offset + i * size + position);
        }
        // A track of vec3 offsets (a bone's translation): scaled, not moved
        var scaledKeys = new HashSet<uint>();
        void ScaleTrack(int track)
        {
            var (sequences, offset) = Array(track + 12);
            for (var s = 0; s < sequences; ++s)
            {
                var (keys, keyOffset) = Array((int)offset + s * 8);
                if (keys == 0 || !scaledKeys.Add(keyOffset))
                    continue;
                for (var k = 0; k < keys * 3; ++k)
                    SetF((int)keyOffset + k * 4, F((int)keyOffset + k * 4) / scale);
            }
        }

        Each(0x3C, 48, 0);                                  // vertices
        Each(0x2C, 88, 0x4C);                               // bone pivots
        if (scale != 1)
        {
            var (bones, boneOffset) = Array(0x2C);
            for (var i = 0; i < bones; ++i)
                ScaleTrack((int)boneOffset + i * 88 + 0x10);
        }
        Each(0xF0, 40, 8);                                  // attachments
        Each(0x100, 36, 12);                                // events
        Each(0x108, 156, 4);                                // lights
        Each(0x110, 100, 36);                               // cameras: position base
        Each(0x110, 100, 68);                               //   target base
        Each(0x120, 176, 8);                                // ribbons
        Each(0x128, 476, 8);                                // particles
        Each(0xE0, 12, 0);                                  // collision vertices
        foreach (var box in new[] { 0xA0, 0xBC })           // bounding and collision boxes, then their radius
        {
            Point(Data, box);
            Point(Data, box + 12);
            SetF(box + 24, F(box + 24) / scale);
        }
        foreach (var skin in skins)
        {
            var (count, offset) = (BitConverter.ToUInt32(skin, 0x1C), BitConverter.ToUInt32(skin, 0x20));
            for (var i = 0; i < count; ++i)
            {
                var at = (int)offset + i * 48;
                Point(skin, at + 0x14);                     // centre
                Point(skin, at + 0x20);                     // sort centre
                BitConverter.GetBytes(BitConverter.ToSingle(skin, at + 0x2C) / scale).CopyTo(skin, at + 0x2C);
            }
        }
    }

    /// <summary>Every structural problem a 3.3.5 client could trip on, empty when the model and skins are
    /// sound.</summary>
    public List<string> Validate(IReadOnlyList<byte[]> skins)
    {
        var problems = new List<string>();
        if (Encoding.ASCII.GetString(Data, 0, 4) != "MD20")
            problems.Add("magic is not MD20");
        if (U32(4) != 264)
            problems.Add($"version {U32(4)}, not 264");
        var arrays = HeaderArrays.ToList();
        if ((U32(0x10) & 0x08) != 0)
            arrays.Add(("blend overrides", 0x130, 2));
        foreach (var (name, headerOffset, size) in arrays)
        {
            var (count, offset) = Array(headerOffset);
            if (count > 0 && (ulong)offset + (ulong)count * (ulong)size > (ulong)Data.Length)
                problems.Add($"{name}: {count} x {size} bytes at 0x{offset:X} runs past the end (0x{Data.Length:X})");
            if (count > 0 && offset < HeaderSize)
                problems.Add($"{name}: data at 0x{offset:X} overlaps the 0x{HeaderSize:X}-byte header");
        }
        if (problems.Count > 0)
            return problems;

        var (vertexCount, _) = Array(0x3C);
        var (textureCount, textureOffset) = Array(0x50);
        for (var i = 0; i < textureCount; ++i)
        {
            var record = (int)textureOffset + i * 16;
            var nameLength = U32(record + 8);
            var nameOffset = U32(record + 12);
            if (U32(record) == 0 && nameLength <= 1)
                problems.Add($"texture {i} is hardcoded (type 0) but has no file name");
            if (nameLength > 0 && nameOffset + nameLength > Data.Length)
                problems.Add($"texture {i} name runs past the end");
        }
        var materialCount = Array(0x70).count;
        var (lookupCount, lookupOffset) = Array(0x80);
        for (var i = 0; i < lookupCount; ++i)
        {
            var texture = BitConverter.ToInt16(Data, (int)lookupOffset + i * 2);
            if (texture >= textureCount)
                problems.Add($"texture lookup {i} names texture {texture} of {textureCount}");
        }
        if (U32(0x44) != skins.Count)
            problems.Add($"the header counts {U32(0x44)} skins, {skins.Count} written");

        for (var s = 0; s < skins.Count; ++s)
        {
            var skin = skins[s];
            if (Encoding.ASCII.GetString(skin, 0, 4) != "SKIN")
            {
                problems.Add($"skin {s}: magic is not SKIN");
                continue;
            }
            (uint count, uint offset) SkinArray(int at) =>
                (BitConverter.ToUInt32(skin, at), BitConverter.ToUInt32(skin, at + 4));
            var (skinVertexCount, skinVertexOffset) = SkinArray(0x04);
            var (indexCount, indexOffset) = SkinArray(0x0C);
            var (submeshCount, submeshOffset) = SkinArray(0x1C);
            var (batchCount, batchOffset) = SkinArray(0x24);
            foreach (var (name, at, size) in new[] { ("vertices", 0x04, 2), ("indices", 0x0C, 2), ("bones", 0x14, 4),
                         ("submeshes", 0x1C, 48), ("batches", 0x24, 24) })
            {
                var (count, offset) = SkinArray(at);
                if (count > 0 && (ulong)offset + (ulong)count * (ulong)size > (ulong)skin.Length)
                    problems.Add($"skin {s} {name} run past the end");
            }
            if (problems.Count > 0)
                continue;
            for (var i = 0; i < skinVertexCount; ++i)
                if (BitConverter.ToUInt16(skin, (int)skinVertexOffset + i * 2) >= vertexCount)
                {
                    problems.Add($"skin {s} vertex {i} is past the model's {vertexCount} vertices");
                    break;
                }
            for (var i = 0; i < indexCount; ++i)
                if (BitConverter.ToUInt16(skin, (int)indexOffset + i * 2) >= skinVertexCount)
                {
                    problems.Add($"skin {s} index {i} is past the skin's {skinVertexCount} vertices");
                    break;
                }
            for (var i = 0; i < submeshCount; ++i)
            {
                var at = (int)submeshOffset + i * 48;
                var vStart = BitConverter.ToUInt16(skin, at + 4);
                var vCount = BitConverter.ToUInt16(skin, at + 6);
                var iStart = BitConverter.ToUInt16(skin, at + 8);
                var iCount = BitConverter.ToUInt16(skin, at + 10);
                if (vStart + vCount > skinVertexCount || iStart + iCount > indexCount)
                    problems.Add($"skin {s} submesh {i} runs past its vertices or indices");
            }
            var unitLookupCount = Array(0x88).count;
            var transparencyLookupCount = Array(0x90).count;
            var uvLookupCount = Array(0x98).count;
            for (var i = 0; i < batchCount; ++i)
            {
                var at = (int)batchOffset + i * 24;
                var shader = BitConverter.ToUInt16(skin, at + 2);
                var submesh = BitConverter.ToUInt16(skin, at + 4);
                var material = BitConverter.ToUInt16(skin, at + 0x0A);
                var textureCountInBatch = BitConverter.ToUInt16(skin, at + 0x0E);
                var textureCombo = BitConverter.ToUInt16(skin, at + 0x10);
                var unitCombo = BitConverter.ToUInt16(skin, at + 0x12);
                var transparencyCombo = BitConverter.ToUInt16(skin, at + 0x14);
                var uvCombo = BitConverter.ToUInt16(skin, at + 0x16);
                if (shader == 0x8000)
                    continue; // a batch MultiConverter disabled (its submesh was past 65535 vertices)
                if (submesh >= submeshCount)
                    problems.Add($"skin {s} batch {i}: submesh {submesh} of {submeshCount}");
                if (material >= materialCount)
                    problems.Add($"skin {s} batch {i}: material {material} of {materialCount}");
                if (textureCombo + textureCountInBatch > lookupCount)
                    problems.Add($"skin {s} batch {i}: textures {textureCombo}+{textureCountInBatch} of {lookupCount}");
                if (unitCombo + textureCountInBatch > unitLookupCount)
                    problems.Add(
                        $"skin {s} batch {i}: texture units {unitCombo}+{textureCountInBatch} of {unitLookupCount}");
                if (transparencyCombo >= transparencyLookupCount)
                    problems.Add($"skin {s} batch {i}: transparency {transparencyCombo} of {transparencyLookupCount}");
                if (uvCombo >= uvLookupCount && uvCombo != 0xFFFF)
                    problems.Add($"skin {s} batch {i}: uv animation {uvCombo} of {uvLookupCount}");
            }
        }
        return problems;
    }
}
