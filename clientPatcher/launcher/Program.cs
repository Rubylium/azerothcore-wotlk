using System;
using System.Linq;
using System.Net;
using System.Windows;

namespace Evolutions
{
    static class Program
    {
        [STAThread]
        static void Main(string[] args)
        {
            // GitHub only accepts TLS 1.2 and later
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12 | (SecurityProtocolType)12288;

            var app = new Application { ShutdownMode = ShutdownMode.OnMainWindowClose };
            app.Resources.MergedDictionaries.Add((ResourceDictionary)Application.LoadComponent(
                new Uri("/Evolutions;component/Theme.xaml", UriKind.Relative)));

            // Development switches: --no-self-update keeps a local build from replacing itself with the published
            // launcher, --manifest <file> previews a release that is not published yet, --page news|settings opens
            // on that page
            int manifestArg = Array.IndexOf(args, "--manifest");
            if (manifestArg >= 0 && manifestArg + 1 < args.Length)
                Updater.ManifestOverride = args[manifestArg + 1];
            int pageArg = Array.IndexOf(args, "--page");
            if (pageArg >= 0 && pageArg + 1 < args.Length)
                MainWindow.StartPage = args[pageArg + 1];
            app.Run(new MainWindow(!args.Contains("--no-self-update")));
        }
    }
}
