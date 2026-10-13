using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Web.Script.Serialization;

namespace Evolutions
{
    public sealed class Settings
    {
        public string GameFolder { get; set; }

        // From the last manifest read, so the home page has something to show before (or without) the network
        public string Realmlist { get; set; }
        public List<Realm> Realms { get; set; } = new List<Realm>();
        public string DefaultRealm { get; set; }
        // The realm the player picked on the realm card (a Realm.Id); empty until they pick one
        public string SelectedRealm { get; set; }
        public string Version { get; set; }
        public List<NewsItem> News { get; set; } = new List<NewsItem>();

        // What the last update installed, so a file a later release stops shipping can be taken back out
        public List<string> Installed { get; set; } = new List<string>();

        // The manifest's CacheReset last applied (the game's Cache\WDB emptied for it)
        public int CacheReset { get; set; }

        // Folder -> SHA-256 of the bundle it was last installed from
        public Dictionary<string, string> Bundles { get; set; } = new Dictionary<string, string>();

        // Hashes of the files already checked, keyed by path: a file whose size and date are unchanged is not
        // read again, which keeps a start from hashing the whole patch every time
        public Dictionary<string, CachedHash> Hashes { get; set; } = new Dictionary<string, CachedHash>();

        // The player's pick, else the release's default, else the first; null when the release lists no realms
        public Realm CurrentRealm() =>
            Realms?.FirstOrDefault(realm => realm.Id == SelectedRealm)
            ?? Realms?.FirstOrDefault(realm => realm.Id == DefaultRealm)
            ?? Realms?.FirstOrDefault();

        // The address the game is pointed at and the realm card pings
        public string RealmAddress() => CurrentRealm()?.Address ?? Realmlist;

        static string Folder =>
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Evolutions");

        static string FilePath => Path.Combine(Folder, "settings.json");

        public static Settings Load()
        {
            try
            {
                if (File.Exists(FilePath))
                    return new JavaScriptSerializer().Deserialize<Settings>(File.ReadAllText(FilePath)) ?? new Settings();
            }
            catch (Exception)
            {
                // A damaged settings file only costs the remembered folder and hashes
            }
            return new Settings();
        }

        public void Save()
        {
            Directory.CreateDirectory(Folder);
            var serializer = new JavaScriptSerializer { MaxJsonLength = int.MaxValue };
            File.WriteAllText(FilePath, serializer.Serialize(this));
        }
    }

    public sealed class CachedHash
    {
        public long Size { get; set; }
        public long LastWriteTicks { get; set; }
        public string Sha256 { get; set; }
    }
}
