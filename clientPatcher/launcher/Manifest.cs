using System.Collections.Generic;
using System.Web.Script.Serialization;

namespace Evolutions
{
    // The release description the launcher downloads from the latest GitHub release (manifest.json)
    public sealed class Manifest
    {
        public string Version { get; set; }
        public RemoteFile Launcher { get; set; }
        public RemoteFile WowExePatch { get; set; }
        public string Realmlist { get; set; }
        public List<string> Addons { get; set; } = new List<string>();
        public List<string> RemovedAddons { get; set; } = new List<string>();
        public List<ClientFile> Files { get; set; } = new List<ClientFile>();
        public List<Bundle> Bundles { get; set; } = new List<Bundle>();
        public List<NewsItem> News { get; set; } = new List<NewsItem>();

        public static Manifest Parse(string json) =>
            new JavaScriptSerializer { MaxJsonLength = int.MaxValue }.Deserialize<Manifest>(json);
    }

    // An announcement shown on the launcher's home page, newest first. Image names one of the pictures built into
    // the launcher (news-<Image>.jpg).
    public sealed class NewsItem
    {
        public string Title { get; set; }
        public string Date { get; set; }
        public string Tag { get; set; }
        public string Image { get; set; }
        public string Body { get; set; }
    }

    // A whole folder shipped as one zip (an addon): installed by replacing the folder
    public sealed class Bundle : RemoteFile
    {
        public string Folder { get; set; }
    }

    public class RemoteFile
    {
        public string Url { get; set; }
        public string Sha256 { get; set; }
        public long Size { get; set; }
    }

    // A file installed into the game folder; Path is relative to it, with forward slashes
    public sealed class ClientFile : RemoteFile
    {
        public string Path { get; set; }
    }

    // Byte patches to Wow.exe (WowExePatch.json), each checked against the stock client before it is written
    public sealed class WowExePatchSpec
    {
        public string File { get; set; }
        public int Build { get; set; }
        public List<WowExePatch> Patches { get; set; } = new List<WowExePatch>();

        public static WowExePatchSpec Parse(string json) =>
            new JavaScriptSerializer().Deserialize<WowExePatchSpec>(json);
    }

    public sealed class WowExePatch
    {
        public string Name { get; set; }
        public string Va { get; set; }
        public long Offset { get; set; }
        public string Stock { get; set; }
        public string Patched { get; set; }
    }
}
