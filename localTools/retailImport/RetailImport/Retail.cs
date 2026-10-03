using DBCD;
using DBCD.Providers;
using TACTSharp;

namespace RetailImport;

/// <summary>
/// Read-only access to the local retail install: files by FileDataID through TACTSharp, DB2 tables through DBCD,
/// and file names from the community listfile. Nothing is ever written under the install folder: TACTSharp's cache
/// (group index, anything fetched from the CDN) goes to .deps/cache.
/// </summary>
public sealed class Retail : IDBCProvider, IDBDProvider
{
    public string Version { get; }
    private readonly BuildInstance build;
    private readonly Dictionary<uint, string> names = new();
    private readonly Dictionary<string, uint> ids = new(StringComparer.OrdinalIgnoreCase);
    private readonly string dbdDir;
    private readonly Dictionary<string, IDBCDStorage> tables = new();
    private readonly DBCD.DBCD dbcd;

    public Retail(string baseDir, string depsDir, string product = "wow")
    {
        dbdDir = Path.Combine(depsDir, "dbd");
        Directory.CreateDirectory(dbdDir);

        build = new BuildInstance();
        build.Settings.BaseDir = baseDir;
        build.Settings.Product = product;
        build.Settings.Region = "eu";
        build.Settings.Locale = RootInstance.LocaleFlags.enUS;
        build.Settings.CacheDir = Path.Combine(depsDir, "cache");
        build.Settings.ListfileFallback = false;
        Settings.LogLevel = TSLogLevel.Warn;

        var buildInfo = new BuildInfo(Path.Combine(baseDir, ".build.info"), build.Settings, build.cdn);
        var entry = buildInfo.Entries.First(x => x.Product == product);
        build.Settings.BuildConfig = entry.BuildConfig;
        build.Settings.CDNConfig = entry.CDNConfig;
        if (string.IsNullOrEmpty(build.cdn.ProductDirectory))
            build.cdn.ProductDirectory = entry.CDNPath;
        build.LoadConfigs(build.Settings.BuildConfig, build.Settings.CDNConfig);
        build.Load();
        Version = entry.Version;
        Console.WriteLine($"Retail {Version} loaded from {baseDir}");

        var listfile = Path.Combine(depsDir, "community-listfile.csv");
        foreach (var line in File.ReadLines(listfile))
        {
            var split = line.IndexOf(';');
            if (split <= 0 || !uint.TryParse(line.AsSpan(0, split), out var id))
                continue;
            var name = line[(split + 1)..];
            names[id] = name;
            ids.TryAdd(name, id);
        }

        dbcd = new DBCD.DBCD(this, this);
    }

    public string NameOf(uint fileDataId) => names.TryGetValue(fileDataId, out var name) ? name : null;

    /// <summary>The files whose listfile name holds a text</summary>
    public HashSet<uint> FilesNamed(string text) => names.Where(pair => pair.Value.Contains(text,
        StringComparison.OrdinalIgnoreCase)).Select(pair => pair.Key).ToHashSet();

    public uint IdOf(string name) => ids.TryGetValue(name.Replace('\\', '/'), out var id) ? id : 0;

    public byte[] Open(uint fileDataId) => build.OpenFileByFDID(fileDataId);

    public IDBCDStorage Table(string name)
    {
        if (!tables.TryGetValue(name, out var table))
            tables[name] = table = dbcd.Load(name, Version);
        return table;
    }

    public Stream StreamForTableName(string tableName, string build)
    {
        var id = IdOf($"dbfilesclient/{tableName.ToLowerInvariant()}.db2");
        if (id == 0)
            throw new FileNotFoundException($"{tableName}.db2 is not in the listfile");
        return new MemoryStream(Open(id));
    }

    Stream IDBDProvider.StreamForTableName(string tableName, string build)
    {
        var path = Path.Combine(dbdDir, tableName + ".dbd");
        if (!File.Exists(path))
        {
            using var http = new HttpClient();
            var url = $"https://raw.githubusercontent.com/wowdev/WoWDBDefs/master/definitions/{tableName}.dbd";
            File.WriteAllBytes(path, http.GetByteArrayAsync(url).Result);
        }
        return new MemoryStream(File.ReadAllBytes(path));
    }
}
