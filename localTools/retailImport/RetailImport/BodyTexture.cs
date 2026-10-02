namespace RetailImport;

/// <summary>
/// Body textures (the regions of the character texture: ArmUpper ... Foot) the way a 3.3.5 client pastes them onto a
/// character: palettized BLP2, 256 colours and an 8-bit alpha when there is any transparency, with a full mip chain -
/// the format of every stock body texture (Leather_RaidDruid_B_01_Chest_TU_M.blp: 128x64, palette, alpha 8). Retail
/// ships them DXT-compressed, Legion's at twice that size (256x128), Midnight's at four times (512x256): they are
/// brought down to at most maxWidth (every region is 128 wide in stock, 256 in Ascension's conversions, which we
/// keep), the compression is not kept. Model textures (helmets, shoulders) stay as they are (Blp.ForClassic);
/// capes are filled where retail cuts them out (CapeForClassic).
/// </summary>
public static class BodyTexture
{
    public static byte[] ToPalettized(byte[] blp, string what, int maxWidth, out string description)
    {
        var (width, height, pixels) = Decode(blp, what);
        return Encode(width, height, pixels, maxWidth, out description);
    }

    /// <summary>
    /// A cape for the 3.3.5 character model, which draws its cape opaque (HumanMale.m2's cape batches: blend mode 0):
    /// retail cuts a cape's hem out with the texture's alpha (cape_leather_raidrogue_r_01_mythic2long: a diagonal
    /// cut at the bottom left), and 3.3.5 shows whatever colour lies under the cut, white. The cut is filled by
    /// repeating the cape's own pattern downwards, at the vertical period that best matches the rest of it (what
    /// Ascension's copy of that cape does by hand). The layout is stock's: half a cape the model mirrors, the clasp in
    /// the top left corner and an unused transparent strip under it, left as it is.
    /// </summary>
    public static byte[] CapeForClassic(byte[] blp, string what, out string description)
    {
        var (width, height, pixels) = Decode(blp, what);
        bool Opaque(int x, int y) => pixels[(y * width + x) * 4 + 3] >= 128;

        // The cape's body starts at the first column opaque over most of the texture's second quarter
        var bodyX = 0;
        while (bodyX < width && Enumerable.Range(height / 4, height / 4).Count(y => Opaque(bodyX, y)) * 2 < height / 4)
            ++bodyX;
        var holes = new List<int>();
        for (var y = 0; y < height; ++y)
            for (var x = bodyX; x < width; ++x)
                if (!Opaque(x, y))
                    holes.Add(y * width + x);
        if (holes.Count == 0 || bodyX >= width)
            return Encode(width, height, pixels, width, out description);

        // The period: the shift whose rows best match the rows it would copy, over the cape's lower two thirds
        var period = 0;
        var bestScore = double.MaxValue;
        for (var dy = Math.Max(8, height / 16); dy <= height / 2; dy += 2)
        {
            long difference = 0, pairs = 0;
            for (var y = Math.Max(height / 3, dy); y < height; ++y)
                for (var x = bodyX; x < width; ++x)
                {
                    if (!Opaque(x, y) || !Opaque(x, y - dy))
                        continue;
                    int p = (y * width + x) * 4, q = ((y - dy) * width + x) * 4;
                    difference += Math.Abs(pixels[p] - pixels[q]) + Math.Abs(pixels[p + 1] - pixels[q + 1])
                                  + Math.Abs(pixels[p + 2] - pixels[q + 2]);
                    ++pairs;
                }
            if (pairs * 20 < (long)(width - bodyX) * height)
                continue;
            var score = (double)difference / pairs;
            if (score < bestScore)
            {
                bestScore = score;
                period = dy;
            }
        }

        // Top down, so a hole copies a pixel already filled when the cut is taller than the period; a hole with no
        // cape above it (none in retail's hem cuts) takes the nearest opaque pixel of its row
        var filled = 0;
        foreach (var hole in holes)
        {
            int x = hole % width, y = hole / width;
            var source = period > 0 && y >= period ? hole - period * width : -1;
            if (source < 0 || pixels[source * 4 + 3] < 128)
            {
                source = -1;
                for (var distance = 1; distance < width && source < 0; ++distance)
                    foreach (var sx in new[] { x + distance, x - distance })
                        if (sx >= bodyX && sx < width && Opaque(sx, y))
                        {
                            source = y * width + sx;
                            break;
                        }
                if (source < 0)
                    continue;
            }
            Array.Copy(pixels, source * 4, pixels, hole * 4, 3);
            pixels[hole * 4 + 3] = 255;
            ++filled;
        }
        var bytes = Encode(width, height, pixels, width, out description);
        description += $", hem cut filled ({filled} pixels, pattern repeated every {period} rows)";
        return bytes;
    }

    private static byte[] Encode(int width, int height, byte[] pixels, int maxWidth, out string description)
    {
        while (width > maxWidth && width > 1 && height > 1)
            (width, height, pixels) = Downsample(width, height, pixels);

        var hasAlpha = false;
        for (var i = 3; i < pixels.Length; i += 4)
            if (pixels[i] != 255)
            {
                hasAlpha = true;
                break;
            }

        var palette = MedianCut(pixels, 256);
        var mips = new List<(int w, int h, byte[] rgba)> { (width, height, pixels) };
        while (mips[^1].w > 1 || mips[^1].h > 1)
            mips.Add(Downsample(mips[^1].w, mips[^1].h, mips[^1].rgba));

        var nearest = new Dictionary<int, byte>();
        var blocks = new List<byte[]>();
        foreach (var (w, h, rgba) in mips)
        {
            var count = w * h;
            var block = new byte[count * (hasAlpha ? 2 : 1)];
            for (var p = 0; p < count; ++p)
            {
                block[p] = Nearest(palette, nearest, rgba[p * 4], rgba[p * 4 + 1], rgba[p * 4 + 2]);
                if (hasAlpha)
                    block[count + p] = rgba[p * 4 + 3];
            }
            blocks.Add(block);
        }

        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream);
        writer.Write("BLP2"u8);
        writer.Write(1);                                    // type: BLP2 with a colour encoding
        writer.Write((byte)1);                              // palettized
        writer.Write((byte)(hasAlpha ? 8 : 0));             // alpha depth
        writer.Write((byte)8);                              // alpha type, as stock palettized textures carry
        writer.Write((byte)1);                              // a full mip chain follows
        writer.Write(width);
        writer.Write(height);
        var offset = 148 + 256 * 4;
        for (var i = 0; i < 16; ++i)
        {
            writer.Write(i < blocks.Count ? offset : 0);
            if (i < blocks.Count)
                offset += blocks[i].Length;
        }
        for (var i = 0; i < 16; ++i)
            writer.Write(i < blocks.Count ? blocks[i].Length : 0);
        for (var i = 0; i < 256; ++i)
        {
            var colour = i < palette.Count ? palette[i] : (r: 0, g: 0, b: 0);
            writer.Write((byte)colour.b);
            writer.Write((byte)colour.g);
            writer.Write((byte)colour.r);
            writer.Write((byte)0);
        }
        foreach (var block in blocks)
            writer.Write(block);
        description = $"{width}x{height} palettized ({palette.Count} colours) alpha {(hasAlpha ? 8 : 0)},"
                      + $" {blocks.Count} mips";
        return stream.ToArray();
    }

    // --- Decoding ------------------------------------------------------------------------------------------------

    /// <summary>The largest mip as RGBA</summary>
    public static (int width, int height, byte[] rgba) Decode(byte[] blp, string what)
    {
        if (blp.Length < 148 || blp[0] != 'B' || blp[1] != 'L' || blp[2] != 'P' || blp[3] != '2')
            throw new Exception($"{what}: not a BLP2 texture");
        var compression = blp[8];
        var alphaDepth = blp[9];
        var alphaType = blp[10];
        var width = BitConverter.ToInt32(blp, 12);
        var height = BitConverter.ToInt32(blp, 16);
        var offset = (int)BitConverter.ToUInt32(blp, 20);
        var rgba = new byte[width * height * 4];
        switch (compression)
        {
            case 1:
            {
                var count = width * height;
                for (var p = 0; p < count; ++p)
                {
                    var index = blp[offset + p];
                    rgba[p * 4] = blp[148 + index * 4 + 2];
                    rgba[p * 4 + 1] = blp[148 + index * 4 + 1];
                    rgba[p * 4 + 2] = blp[148 + index * 4];
                    rgba[p * 4 + 3] = alphaDepth switch
                    {
                        8 => blp[offset + count + p],
                        1 => (byte)(((blp[offset + count + p / 8] >> (p % 8)) & 1) * 255),
                        4 => (byte)(((blp[offset + count + p / 2] >> (p % 2 * 4)) & 0xF) * 17),
                        _ => 255,
                    };
                }
                break;
            }
            case 2:
                DecodeDxt(blp, offset, width, height, alphaType switch { 0 => 1, 1 => 3, _ => 5 }, alphaDepth, rgba);
                break;
            case 3:
                for (var p = 0; p < width * height; ++p)
                {
                    rgba[p * 4] = blp[offset + p * 4 + 2];
                    rgba[p * 4 + 1] = blp[offset + p * 4 + 1];
                    rgba[p * 4 + 2] = blp[offset + p * 4];
                    rgba[p * 4 + 3] = blp[offset + p * 4 + 3];
                }
                break;
            default:
                throw new Exception($"{what}: BLP compression {compression} cannot be decoded");
        }
        return (width, height, rgba);
    }

    private static void DecodeDxt(byte[] data, int offset, int width, int height, int format, int alphaDepth,
        byte[] rgba)
    {
        var blockSize = format == 1 ? 8 : 16;
        var colours = new byte[4, 4];
        var alphas = new byte[16];
        for (var by = 0; by < (height + 3) / 4; ++by)
        for (var bx = 0; bx < (width + 3) / 4; ++bx)
        {
            var block = offset + (by * ((width + 3) / 4) + bx) * blockSize;
            var colourBlock = format == 1 ? block : block + 8;
            if (format == 3)
                for (var i = 0; i < 16; ++i)
                    alphas[i] = (byte)(((data[block + i / 2] >> (i % 2 * 4)) & 0xF) * 17);
            else if (format == 5)
            {
                int a0 = data[block], a1 = data[block + 1];
                var table = new int[8];
                table[0] = a0;
                table[1] = a1;
                if (a0 > a1)
                    for (var i = 2; i < 8; ++i)
                        table[i] = ((8 - i) * a0 + (i - 1) * a1) / 7;
                else
                {
                    for (var i = 2; i < 6; ++i)
                        table[i] = ((6 - i) * a0 + (i - 1) * a1) / 5;
                    table[6] = 0;
                    table[7] = 255;
                }
                ulong bits = 0;
                for (var i = 0; i < 6; ++i)
                    bits |= (ulong)data[block + 2 + i] << (8 * i);
                for (var i = 0; i < 16; ++i)
                    alphas[i] = (byte)table[(bits >> (3 * i)) & 7];
            }

            int c0 = BitConverter.ToUInt16(data, colourBlock), c1 = BitConverter.ToUInt16(data, colourBlock + 2);
            Expand565(c0, colours, 0);
            Expand565(c1, colours, 1);
            var fourColours = format != 1 || c0 > c1;
            for (var ch = 0; ch < 3; ++ch)
            {
                if (fourColours)
                {
                    colours[2, ch] = (byte)((2 * colours[0, ch] + colours[1, ch]) / 3);
                    colours[3, ch] = (byte)((colours[0, ch] + 2 * colours[1, ch]) / 3);
                }
                else
                {
                    colours[2, ch] = (byte)((colours[0, ch] + colours[1, ch]) / 2);
                    colours[3, ch] = 0;
                }
            }
            var indices = BitConverter.ToUInt32(data, colourBlock + 4);
            for (var i = 0; i < 16; ++i)
            {
                int x = bx * 4 + i % 4, y = by * 4 + i / 4;
                if (x >= width || y >= height)
                    continue;
                var index = (int)((indices >> (2 * i)) & 3);
                var p = (y * width + x) * 4;
                rgba[p] = colours[index, 0];
                rgba[p + 1] = colours[index, 1];
                rgba[p + 2] = colours[index, 2];
                rgba[p + 3] = format == 1
                    ? (byte)(!fourColours && index == 3 && alphaDepth > 0 ? 0 : 255)
                    : alphas[i];
            }
        }
    }

    private static void Expand565(int colour, byte[,] colours, int slot)
    {
        colours[slot, 0] = (byte)(((colour >> 11) & 31) * 255 / 31);
        colours[slot, 1] = (byte)(((colour >> 5) & 63) * 255 / 63);
        colours[slot, 2] = (byte)((colour & 31) * 255 / 31);
    }

    private static (int, int, byte[]) Downsample(int width, int height, byte[] rgba)
    {
        int w = Math.Max(1, width / 2), h = Math.Max(1, height / 2);
        var result = new byte[w * h * 4];
        for (var y = 0; y < h; ++y)
        for (var x = 0; x < w; ++x)
        for (var ch = 0; ch < 4; ++ch)
        {
            var sum = 0;
            var samples = 0;
            for (var dy = 0; dy < 2; ++dy)
            for (var dx = 0; dx < 2; ++dx)
            {
                int sx = Math.Min(width - 1, x * 2 + dx), sy = Math.Min(height - 1, y * 2 + dy);
                sum += rgba[(sy * width + sx) * 4 + ch];
                ++samples;
            }
            result[(y * w + x) * 4 + ch] = (byte)(sum / samples);
        }
        return (w, h, result);
    }

    // --- Palette ---------------------------------------------------------------------------------------------------

    /// <summary>Median cut over the opaque-enough pixels' colours, weighted by how often each appears</summary>
    private static List<(int r, int g, int b)> MedianCut(byte[] rgba, int colours)
    {
        var counts = new Dictionary<int, int>();
        for (var p = 0; p < rgba.Length; p += 4)
        {
            var key = (rgba[p] << 16) | (rgba[p + 1] << 8) | rgba[p + 2];
            counts[key] = counts.TryGetValue(key, out var c) ? c + 1 : 1;
        }
        var boxes = new List<List<(int key, int count)>> { counts.Select(kv => (kv.Key, kv.Value)).ToList() };
        while (boxes.Count < colours)
        {
            // The box with the widest channel range among those that can still be split
            var best = -1;
            var bestRange = 0;
            var bestChannel = 0;
            for (var i = 0; i < boxes.Count; ++i)
            {
                if (boxes[i].Count < 2)
                    continue;
                for (var ch = 0; ch < 3; ++ch)
                {
                    var shift = 16 - ch * 8;
                    int min = 255, max = 0;
                    foreach (var (key, _) in boxes[i])
                    {
                        var v = (key >> shift) & 255;
                        min = Math.Min(min, v);
                        max = Math.Max(max, v);
                    }
                    if (max - min > bestRange)
                    {
                        bestRange = max - min;
                        best = i;
                        bestChannel = ch;
                    }
                }
            }
            if (best < 0)
                break;
            var box = boxes[best];
            var s = 16 - bestChannel * 8;
            box.Sort((a, b) => ((a.key >> s) & 255).CompareTo((b.key >> s) & 255));
            var total = box.Sum(entry => (long)entry.count);
            long running = 0;
            var split = 1;
            for (var i = 0; i < box.Count - 1; ++i)
            {
                running += box[i].count;
                if (running * 2 >= total)
                {
                    split = i + 1;
                    break;
                }
                split = i + 1;
            }
            boxes[best] = box.GetRange(0, split);
            boxes.Add(box.GetRange(split, box.Count - split));
        }
        return boxes.Where(box => box.Count > 0).Select(box =>
        {
            long r = 0, g = 0, b = 0, n = 0;
            foreach (var (key, count) in box)
            {
                r += ((key >> 16) & 255) * (long)count;
                g += ((key >> 8) & 255) * (long)count;
                b += (key & 255) * (long)count;
                n += count;
            }
            return ((int)(r / n), (int)(g / n), (int)(b / n));
        }).ToList();
    }

    private static byte Nearest(List<(int r, int g, int b)> palette, Dictionary<int, byte> cache, int r, int g, int b)
    {
        var key = (r << 16) | (g << 8) | b;
        if (cache.TryGetValue(key, out var found))
            return found;
        var best = 0;
        var bestDistance = int.MaxValue;
        for (var i = 0; i < palette.Count; ++i)
        {
            int dr = palette[i].r - r, dg = palette[i].g - g, db = palette[i].b - b;
            var distance = dr * dr * 3 + dg * dg * 4 + db * db * 2;
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = i;
            }
        }
        cache[key] = (byte)best;
        return (byte)best;
    }
}
