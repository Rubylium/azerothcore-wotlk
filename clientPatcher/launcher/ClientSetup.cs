using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace Evolutions
{
    public static class ClientSetup
    {
        static readonly Regex LocaleFolder = new Regex("^(enUS|enGB|frFR|deDE|esES|esMX|ruRU|koKR|zhCN|zhTW)$");

        public static void SetRealmlist(string gameFolder, string realmlist, string backupFolder)
        {
            if (string.IsNullOrWhiteSpace(realmlist))
                return;

            string dataFolder = Path.Combine(gameFolder, "Data");
            foreach (string locale in Directory.GetDirectories(dataFolder))
            {
                if (!LocaleFolder.IsMatch(Path.GetFileName(locale)))
                    continue;

                string path = Path.Combine(locale, "realmlist.wtf");
                string content = "set realmlist " + realmlist.Trim() + "\r\n";
                if (File.Exists(path) && File.ReadAllText(path) == content)
                    continue;

                if (File.Exists(path))
                {
                    string backup = Path.Combine(backupFolder, "Data", Path.GetFileName(locale), "realmlist.wtf");
                    if (!File.Exists(backup))
                    {
                        Directory.CreateDirectory(Path.GetDirectoryName(backup));
                        File.Copy(path, backup);
                    }
                }
                File.WriteAllText(path, content, Encoding.ASCII);
            }

            // Config.wtf keeps the last realmlist too, and the client prefers it. It also remembers the realm by
            // name: the realm was renamed from AzerothCore to Evolutions, and a client still asking for the old name
            // would open the realm list instead of logging straight in.
            string config = Path.Combine(gameFolder, "WTF", "Config.wtf");
            if (File.Exists(config))
            {
                var lines = File.ReadAllLines(config).ToList();
                bool changed = SetConfigEntry(lines, "realmList", realmlist.Trim(), false);
                changed |= SetConfigEntry(lines, "realmName", RealmName, true);
                if (changed)
                    File.WriteAllLines(config, lines);
            }
        }

        public const string RealmName = "Evolutions";

        // Replaces a "SET name "value"" line of Config.wtf, or adds it when asked to; true if the file changed
        static bool SetConfigEntry(List<string> lines, string name, string value, bool add)
        {
            string entry = "SET " + name + " \"" + value + "\"";
            int index = lines.FindIndex(line => line.StartsWith("SET " + name + " ", StringComparison.OrdinalIgnoreCase));
            if (index >= 0)
            {
                if (lines[index] == entry)
                    return false;
                lines[index] = entry;
                return true;
            }
            if (!add)
                return false;
            lines.Add(entry);
            return true;
        }

        // Every character's AddOns.txt lists our addons as enabled
        public static void EnableAddons(string gameFolder, IEnumerable<string> addons)
        {
            string wtf = Path.Combine(gameFolder, "WTF");
            if (!Directory.Exists(wtf))
                return;

            foreach (string path in Directory.GetFiles(wtf, "AddOns.txt", SearchOption.AllDirectories))
            {
                var lines = File.ReadAllLines(path).ToList();
                bool changed = false;
                foreach (string addon in addons)
                {
                    string entry = addon + ": enabled";
                    int index = lines.FindIndex(line => line.StartsWith(addon + ":", StringComparison.OrdinalIgnoreCase));
                    if (index < 0)
                    {
                        lines.Add(entry);
                        changed = true;
                    }
                    else if (lines[index] != entry)
                    {
                        lines[index] = entry;
                        changed = true;
                    }
                }
                if (changed)
                    File.WriteAllLines(path, lines, new UTF8Encoding(false));
            }
        }

        // Addons a release replaced by something else move to the backup folder instead of being deleted
        public static void RetireAddons(string gameFolder, IEnumerable<string> addons, string backupFolder)
        {
            foreach (string addon in addons)
            {
                string path = Path.Combine(gameFolder, "Interface", "AddOns", addon);
                if (!Directory.Exists(path))
                    continue;

                string target = Path.Combine(backupFolder, "Interface", "AddOns", addon);
                if (Directory.Exists(target))
                    Directory.Delete(target, true);
                Directory.CreateDirectory(Path.GetDirectoryName(target));
                Directory.Move(path, target);
            }
        }
    }
}
