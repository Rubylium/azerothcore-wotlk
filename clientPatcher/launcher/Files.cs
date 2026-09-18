using System;
using System.IO;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;

namespace Evolutions
{
    public static class Files
    {
        public static string Sha256(string path)
        {
            using (var sha = SHA256.Create())
            using (var stream = File.OpenRead(path))
                return ToHex(sha.ComputeHash(stream));
        }

        public static string Sha256(byte[] data)
        {
            using (var sha = SHA256.Create())
                return ToHex(sha.ComputeHash(data));
        }

        static string ToHex(byte[] hash)
        {
            var text = new StringBuilder(hash.Length * 2);
            foreach (byte value in hash)
                text.Append(value.ToString("x2"));
            return text.ToString();
        }

        public static bool SameHash(string left, string right) =>
            string.Equals(left, right, StringComparison.OrdinalIgnoreCase);

        // The hash of a game file, read again only when its size or date changed since the last check
        public static string CachedSha256(Settings settings, string relativePath, string fullPath)
        {
            var info = new FileInfo(fullPath);
            if (!info.Exists)
                return null;

            string key = relativePath.ToLowerInvariant();
            if (settings.Hashes.TryGetValue(key, out CachedHash cached) && cached.Size == info.Length &&
                cached.LastWriteTicks == info.LastWriteTimeUtc.Ticks)
                return cached.Sha256;

            string sha = Sha256(fullPath);
            settings.Hashes[key] = new CachedHash
            {
                Size = info.Length,
                LastWriteTicks = info.LastWriteTimeUtc.Ticks,
                Sha256 = sha,
            };
            return sha;
        }

        // Downloads to a temporary file and returns it only once its hash matches: a broken or cut download never
        // replaces a game file
        public static async Task<string> DownloadVerifiedAsync(HttpClient http, RemoteFile file, string tempFolder,
            Action<long> onBytes)
        {
            Directory.CreateDirectory(tempFolder);
            string tempPath = Path.Combine(tempFolder, file.Sha256 + ".part");

            using (var response = await http.GetAsync(file.Url, HttpCompletionOption.ResponseHeadersRead))
            {
                response.EnsureSuccessStatusCode();
                using (var input = await response.Content.ReadAsStreamAsync())
                using (var output = File.Create(tempPath))
                {
                    var buffer = new byte[81920];
                    int read;
                    while ((read = await input.ReadAsync(buffer, 0, buffer.Length)) > 0)
                    {
                        await output.WriteAsync(buffer, 0, read);
                        onBytes?.Invoke(read);
                    }
                }
            }

            if (!SameHash(Sha256(tempPath), file.Sha256))
            {
                File.Delete(tempPath);
                throw new InvalidDataException("Le fichier téléchargé est corrompu : " + file.Url);
            }
            return tempPath;
        }

        // Replaces a whole folder with the content of a verified zip. The zip is extracted beside the target first,
        // so a bad archive never leaves the game with half a folder, and no extracted path is much longer than the
        // installed one (addon libraries nest deep enough to reach the 260 character limit). backupPath, when given,
        // keeps the folder being replaced (the player's own copy); otherwise the old folder is simply removed.
        public static void InstallBundle(string zipPath, string folderPath, string backupPath)
        {
            string extracted = folderPath + ".new";
            if (Directory.Exists(extracted))
                Directory.Delete(extracted, true);
            try
            {
                System.IO.Compression.ZipFile.ExtractToDirectory(zipPath, extracted);
            }
            catch
            {
                if (Directory.Exists(extracted))
                    Directory.Delete(extracted, true);
                throw;
            }

            if (Directory.Exists(folderPath))
            {
                if (backupPath != null && !Directory.Exists(backupPath))
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(backupPath));
                    Directory.Move(folderPath, backupPath);
                }
                else
                {
                    Directory.Delete(folderPath, true);
                }
            }
            Directory.CreateDirectory(Path.GetDirectoryName(folderPath));
            Directory.Move(extracted, folderPath);
            File.Delete(zipPath);
        }

        // Puts a verified download in place. The file it replaces, if any, is kept once in the backup folder.
        public static void Install(string tempPath, string targetPath, string backupPath)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(targetPath));
            if (File.Exists(targetPath))
            {
                if (!File.Exists(backupPath))
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(backupPath));
                    File.Copy(targetPath, backupPath);
                }
                File.Delete(targetPath);
            }
            File.Move(tempPath, targetPath);
        }
    }
}
