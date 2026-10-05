using System.Text.Json;
using RetailImport;

// Retail item model importer - see ../README.md.
//
//   RetailImport probe <itemId...>                  item -> appearances -> display -> model / texture FileDataIDs
//   RetailImport find <text...>                     items whose name holds the text, with their displays
//   RetailImport look <text...>                     a set's displays and items by slot, from its files' names
//   RetailImport extract <fdid> <outFile> [...]      raw retail files, several pairs at once
//   RetailImport import [items.json]                 convert every entry of items.json into the client assets
//
// Options: --retail <install folder> (default C:\Program Files (x86)\World of Warcraft)

var toolRoot = FindToolRoot();
var repoRoot = Path.GetFullPath(Path.Combine(toolRoot, "..", ".."));
var depsDir = Path.Combine(toolRoot, ".deps");
var retailDir = @"C:\Program Files (x86)\World of Warcraft";
var positional = new List<string>();
for (var i = 0; i < args.Length; ++i)
{
    if (args[i] == "--retail")
        retailDir = args[++i];
    else
        positional.Add(args[i]);
}

if (positional.Count == 0)
{
    Console.WriteLine("usage: RetailImport probe <itemId...> | extract <fdid> <out> | import [items.json]");
    return 2;
}

var retail = new Retail(retailDir, depsDir);
switch (positional[0])
{
    case "probe":
        foreach (var item in positional.Skip(1))
            Probe.Item(retail, int.Parse(item));
        return 0;
    case "probe-model":
        foreach (var model in positional.Skip(1))
            Probe.Model(retail, uint.Parse(model));
        return 0;
    case "find":
        foreach (var text in positional.Skip(1))
            Probe.Find(retail, text);
        return 0;
    case "look":
        foreach (var text in positional.Skip(1))
            Probe.Look(retail, text);
        return 0;
    case "atlas":
        foreach (var text in positional.Skip(1))
            Probe.Atlas(retail, text);
        return 0;
    case "probe-texture":
        foreach (var texture in positional.Skip(1))
            Probe.Texture(retail, uint.Parse(texture));
        return 0;
    case "extract":
        // extract <fdid> <out> [<fdid> <out> ...]: several files for one load of the install
        for (var pair = 1; pair + 1 < positional.Count; pair += 2)
            File.WriteAllBytes(positional[pair + 1], retail.Open(uint.Parse(positional[pair])));
        return 0;
    case "import":
    {
        var manifestPath = positional.Count > 1 ? positional[1] : Path.Combine(toolRoot, "items.json");
        var manifest = JsonSerializer.Deserialize<Manifest>(File.ReadAllText(manifestPath),
            new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true, ReadCommentHandling = JsonCommentHandling.Skip,
            });
        return new Importer(retail, repoRoot, Path.Combine(depsDir, "work")).Run(manifest) ? 0 : 1;
    }
    default:
        Console.WriteLine($"unknown command {positional[0]}");
        return 2;
}

static string FindToolRoot()
{
    var dir = AppContext.BaseDirectory;
    while (dir != null && !File.Exists(Path.Combine(dir, "items.json")))
        dir = Path.GetDirectoryName(dir);
    return dir ?? throw new Exception("localTools/retailImport not found above " + AppContext.BaseDirectory);
}
