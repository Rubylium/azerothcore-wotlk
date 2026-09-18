using System;
using System.Collections.Generic;
using System.IO;

namespace Evolutions
{
    // Applies the byte patches of WowExePatch.json: every range must hold either its stock or its patched bytes,
    // or nothing is written at all. The untouched Wow.exe is kept once in the backup folder.
    public static class WowExePatcher
    {
        public static int Apply(WowExePatchSpec spec, string gameFolder, string backupFolder)
        {
            string exePath = Path.Combine(gameFolder, spec.File);
            byte[] bytes = File.ReadAllBytes(exePath);

            var pending = new List<(WowExePatch Patch, byte[] Bytes)>();
            foreach (WowExePatch patch in spec.Patches)
            {
                byte[] stock = FromHex(patch.Stock);
                byte[] patched = FromHex(patch.Patched);
                if (bytes.Length < patch.Offset + patched.Length)
                    throw new InvalidDataException("Wow.exe n'est pas le client WotLK 3.3.5a (12340) attendu.");

                if (Matches(bytes, patch.Offset, patched))
                    continue;
                if (!Matches(bytes, patch.Offset, stock))
                    throw new InvalidDataException(
                        $"Wow.exe a été modifié par un autre outil à l'endroit de « {patch.Name} » ({patch.Va}). " +
                        "Rien n'a été changé.");
                pending.Add((patch, patched));
            }

            if (pending.Count == 0)
                return 0;

            string backupPath = Path.Combine(backupFolder, spec.File);
            if (!File.Exists(backupPath))
            {
                Directory.CreateDirectory(backupFolder);
                File.Copy(exePath, backupPath);
            }

            foreach (var (patch, patched) in pending)
                Array.Copy(patched, 0, bytes, patch.Offset, patched.Length);
            File.WriteAllBytes(exePath, bytes);

            byte[] written = File.ReadAllBytes(exePath);
            foreach (var (patch, patched) in pending)
                if (!Matches(written, patch.Offset, patched))
                    throw new IOException($"L'écriture de « {patch.Name} » dans Wow.exe a échoué.");
            return pending.Count;
        }

        static bool Matches(byte[] bytes, long offset, byte[] expected)
        {
            for (int i = 0; i < expected.Length; ++i)
                if (bytes[offset + i] != expected[i])
                    return false;
            return true;
        }

        static byte[] FromHex(string hex)
        {
            var bytes = new byte[hex.Length / 2];
            for (int i = 0; i < bytes.Length; ++i)
                bytes[i] = Convert.ToByte(hex.Substring(i * 2, 2), 16);
            return bytes;
        }
    }
}
