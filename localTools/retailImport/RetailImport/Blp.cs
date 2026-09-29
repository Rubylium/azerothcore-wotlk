namespace RetailImport;

/// <summary>
/// Retail item textures and icons are BLP2 files a 3.3.5 client reads as they are (DXT1/3/5 or raw BGRA), with
/// one difference: retail reuses the "has mipmaps" byte for more than a flag (16, 17...). The 3.3.5 client reads
/// any non-zero value as "a full mip chain follows", so it is set to exactly what the file holds.
/// </summary>
public static class Blp
{
    public static byte[] ForClassic(byte[] blp, string what, out string description)
    {
        if (blp.Length < 148 || blp[0] != 'B' || blp[1] != 'L' || blp[2] != 'P' || blp[3] != '2')
            throw new Exception($"{what}: not a BLP2 texture");
        var compression = blp[8];
        var alphaDepth = blp[9];
        var alphaType = blp[10];
        var width = BitConverter.ToInt32(blp, 12);
        var height = BitConverter.ToInt32(blp, 16);
        var supported = compression switch
        {
            1 => true,                                               // palettized
            2 => alphaType is 0 or 1 or 7,                           // DXT1, DXT3, DXT5
            3 => true,                                               // raw BGRA
            _ => false,
        };
        if (!supported)
            throw new Exception(
                $"{what}: BLP compression {compression} alpha type {alphaType} is not readable by 3.3.5");
        if (width > 2048 || height > 2048 || (width & (width - 1)) != 0 || (height & (height - 1)) != 0)
            throw new Exception($"{what}: {width}x{height} is not a power of two up to 2048");

        var levels = 0;
        for (var i = 0; i < 16; ++i)
        {
            var offset = BitConverter.ToUInt32(blp, 20 + i * 4);
            var size = BitConverter.ToUInt32(blp, 84 + i * 4);
            if (offset == 0 || size == 0)
                break;
            if (offset + size > blp.Length)
                throw new Exception($"{what}: mip {i} runs past the end of the file");
            ++levels;
        }
        var fullChain = (int)Math.Log2(Math.Max(width, height)) + 1;
        var copy = (byte[])blp.Clone();
        copy[11] = (byte)(levels >= fullChain ? 1 : 0);
        var format = compression switch
        {
            1 => "palettized",
            2 => alphaType switch { 0 => "DXT1", 1 => "DXT3", _ => "DXT5" },
            _ => "BGRA",
        };
        description = $"{width}x{height} {format} alpha {alphaDepth}, {levels}/{fullChain} mips";
        return copy;
    }
}
