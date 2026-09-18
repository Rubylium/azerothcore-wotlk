using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;

namespace Evolutions
{
    public sealed class Updater
    {
        const string ManifestUrl = "https://github.com/Rubylium/Evolutions/releases/latest/download/manifest.json";

        readonly HttpClient http;
        readonly Settings settings;

        public event Action<string> Status;
        public event Action<long, long> Progress;

        // Bytes downloaded so far out of the update's total, raised while files are downloading
        public event Action<long, long> Transfer;

        // Reinstalls every addon folder even when the settings say it is current (the player asked for a repair)
        public bool Repair { get; set; }

        public static string ExecutablePath => Process.GetCurrentProcess().MainModule.FileName;

        public Updater(HttpClient http, Settings settings)
        {
            this.http = http;
            this.settings = settings;
        }

        // A local manifest.json to read instead of the published one (previewing a release before it goes out)
        public static string ManifestOverride { get; set; }

        public async Task<Manifest> FetchManifestAsync()
        {
            string json = ManifestOverride != null
                ? File.ReadAllText(ManifestOverride)
                : await http.GetStringAsync(ManifestUrl);
            return Manifest.Parse(json);
        }

        // Replaces the running launcher with the release's one and starts it; returns true when this copy must exit
        public async Task<bool> UpdateSelfAsync(Manifest manifest)
        {
            if (manifest.Launcher == null || string.IsNullOrEmpty(manifest.Launcher.Sha256))
                return false;

            string exePath = ExecutablePath;
            if (Files.SameHash(Files.Sha256(exePath), manifest.Launcher.Sha256))
                return false;

            Status?.Invoke("Mise à jour du launcher…");
            string folder = Path.GetDirectoryName(exePath);
            string downloaded = await Files.DownloadVerifiedAsync(http, manifest.Launcher, folder, null);

            // A running executable can be renamed, not overwritten: move it aside, put the new one in its place
            string old = exePath + ".old";
            if (File.Exists(old))
                File.Delete(old);
            File.Move(exePath, old);
            File.Move(downloaded, exePath);

            Process.Start(new ProcessStartInfo(exePath) { UseShellExecute = false });
            return true;
        }

        public static void CleanUpOldLauncher()
        {
            string old = ExecutablePath + ".old";
            try
            {
                if (File.Exists(old))
                    File.Delete(old);
            }
            catch (IOException)
            {
                // The previous copy may still be closing; the next start removes it
            }
        }

        public async Task SyncAsync(Manifest manifest, string gameFolder)
        {
            string workFolder = Path.Combine(gameFolder, "_Evolutions");
            string backupFolder = Path.Combine(workFolder, "backup");
            string downloadFolder = Path.Combine(workFolder, "download");

            // What a previous release installed and this one dropped goes first, so it cannot remove anything this
            // update is about to install
            RemoveDroppedFiles(manifest, gameFolder, backupFolder);
            RemoveDroppedBundles(manifest, gameFolder, backupFolder);

            Status?.Invoke("Vérification des fichiers…");
            var missing = new List<ClientFile>();
            int checkedCount = 0;
            foreach (ClientFile file in manifest.Files)
            {
                string fullPath = FullPath(gameFolder, file.Path);
                string sha = await Task.Run(() => Files.CachedSha256(settings, file.Path, fullPath));
                if (!Files.SameHash(sha, file.Sha256))
                    missing.Add(file);
                Progress?.Invoke(++checkedCount, manifest.Files.Count);
            }

            // A bundle is up to date when its folder was last installed from this very zip
            var staleBundles = manifest.Bundles.Where(bundle => Repair ||
                !settings.Bundles.TryGetValue(bundle.Folder, out string installed) ||
                !Files.SameHash(installed, bundle.Sha256) ||
                !Directory.Exists(FullPath(gameFolder, bundle.Folder))).ToList();

            long total = missing.Sum(file => file.Size) + staleBundles.Sum(bundle => bundle.Size);
            long done = 0;
            Action<long> onBytes = bytes =>
            {
                done += bytes;
                Progress?.Invoke(done, Math.Max(total, 1));
                Transfer?.Invoke(done, total);
            };

            foreach (ClientFile file in missing)
            {
                Status?.Invoke("Téléchargement : " + Path.GetFileName(file.Path));
                string downloaded = await Files.DownloadVerifiedAsync(http, file, downloadFolder, onBytes);
                string fullPath = FullPath(gameFolder, file.Path);
                Files.Install(downloaded, fullPath, FullPath(backupFolder, file.Path));
                Files.CachedSha256(settings, file.Path, fullPath);
            }

            foreach (Bundle bundle in staleBundles)
            {
                Status?.Invoke("Téléchargement : " + Path.GetFileName(bundle.Folder));
                string downloaded = await Files.DownloadVerifiedAsync(http, bundle, downloadFolder, onBytes);
                // A folder the launcher never installed, whole or file by file, is the player's own: it is kept once
                // in the backup folder
                string prefix = bundle.Folder.TrimEnd('/') + "/";
                bool ours = settings.Bundles.ContainsKey(bundle.Folder) ||
                    settings.Installed.Any(path => path.StartsWith(prefix, StringComparison.OrdinalIgnoreCase));
                await Task.Run(() => Files.InstallBundle(downloaded, FullPath(gameFolder, bundle.Folder),
                    ours ? null : FullPath(backupFolder, bundle.Folder)));
                settings.Bundles[bundle.Folder] = bundle.Sha256;
                settings.Save();
            }

            if (manifest.WowExePatch != null && !string.IsNullOrEmpty(manifest.WowExePatch.Url))
            {
                Status?.Invoke("Préparation du client…");
                byte[] data = await http.GetByteArrayAsync(manifest.WowExePatch.Url);
                if (!Files.SameHash(Files.Sha256(data), manifest.WowExePatch.Sha256))
                    throw new InvalidDataException("Le correctif de Wow.exe téléchargé est corrompu.");
                WowExePatcher.Apply(WowExePatchSpec.Parse(Encoding.UTF8.GetString(data)), gameFolder, backupFolder);
            }

            ClientSetup.RetireAddons(gameFolder, manifest.RemovedAddons, backupFolder);
            ClientSetup.EnableAddons(gameFolder, manifest.Addons);
            ClientSetup.SetRealmlist(gameFolder, manifest.Realmlist, backupFolder);

            settings.Installed = manifest.Files.Select(file => file.Path).ToList();
            settings.Save();
            if (Directory.Exists(downloadFolder))
                Directory.Delete(downloadFolder, true);
        }

        // A file a previous release installed and this one no longer ships goes back to what the player had. A file
        // inside a folder this release ships whole is left alone: the bundle replaces that folder anyway.
        void RemoveDroppedFiles(Manifest manifest, string gameFolder, string backupFolder)
        {
            var shipped = new HashSet<string>(manifest.Files.Select(file => file.Path), StringComparer.OrdinalIgnoreCase);
            var bundled = manifest.Bundles.Select(bundle => bundle.Folder.TrimEnd('/') + "/").ToList();
            foreach (string path in settings.Installed.Where(path => !shipped.Contains(path)).ToList())
            {
                settings.Hashes.Remove(path.ToLowerInvariant());
                if (bundled.Any(folder => path.StartsWith(folder, StringComparison.OrdinalIgnoreCase)))
                    continue;

                string fullPath = FullPath(gameFolder, path);
                string backup = FullPath(backupFolder, path);
                if (File.Exists(fullPath))
                    File.Delete(fullPath);
                if (File.Exists(backup))
                    File.Move(backup, fullPath);
            }
        }

        // A folder a previous release shipped whole and this one no longer does goes back to the player's own copy
        void RemoveDroppedBundles(Manifest manifest, string gameFolder, string backupFolder)
        {
            var shipped = new HashSet<string>(manifest.Bundles.Select(bundle => bundle.Folder),
                StringComparer.OrdinalIgnoreCase);
            foreach (string folder in settings.Bundles.Keys.Where(folder => !shipped.Contains(folder)).ToList())
            {
                string fullPath = FullPath(gameFolder, folder);
                string backup = FullPath(backupFolder, folder);
                if (Directory.Exists(fullPath))
                    Directory.Delete(fullPath, true);
                if (Directory.Exists(backup))
                    Directory.Move(backup, fullPath);
                settings.Bundles.Remove(folder);
            }
        }

        static string FullPath(string root, string relativePath) =>
            Path.Combine(root, relativePath.Replace('/', Path.DirectorySeparatorChar));
    }
}
