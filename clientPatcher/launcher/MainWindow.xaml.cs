using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Net.Sockets;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

namespace Evolutions
{
    public partial class MainWindow : Window
    {
        static readonly CultureInfo French = CultureInfo.GetCultureInfo("fr-FR");
        static readonly string[] NewsImages = { "icecrown", "ulduar", "wintergrasp", "malygos", "argent", "ruby",
            "raidfinder", "mythicplus" };

        readonly Settings settings = Settings.Load();
        readonly HttpClient http = new HttpClient { Timeout = TimeSpan.FromMinutes(30) };
        readonly bool selfUpdate;
        readonly DispatcherTimer realmTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(30) };
        readonly AnimatedGifPlayer animatedBackdrop;

        Manifest manifest;
        bool busy;
        bool launching;
        bool repairRequested;
        Action playAction;
        Stopwatch transferClock;

        public MainWindow(bool selfUpdate)
        {
            this.selfUpdate = selfUpdate;
            http.DefaultRequestHeaders.UserAgent.ParseAdd("Evolutions/" + Assembly.GetExecutingAssembly().GetName().Version);
            InitializeComponent();
            animatedBackdrop = new AnimatedGifPlayer(BackdropFrames,
                new Uri("pack://application:,,,/Evolutions;component/Assets/background-animated.gif"));

            SourceInitialized += (sender, args) => Dwm.Style(new WindowInteropHelper(this).Handle);
            StateChanged += (sender, args) =>
            {
                if (WindowState == WindowState.Minimized)
                    animatedBackdrop.Stop();
                else
                    animatedBackdrop.Start();
            };
            Closed += (sender, args) => animatedBackdrop.Dispose();
            realmTimer.Tick += async (sender, args) => await CheckRealmAsync();
            Loaded += async (sender, args) =>
            {
                ShowNews(settings.News);
                ShowVersion(settings.Version);
                ShowSettings();
                animatedBackdrop.Start();
                _ = LoadAnimatedBackdropAsync();
                realmTimer.Start();
                await CheckRealmAsync();
                await CheckAsync();
            };
        }

        async Task LoadAnimatedBackdropAsync()
        {
            try
            {
                await animatedBackdrop.LoadAsync();
            }
            catch (Exception)
            {
                // Keep the static JPG fallback when the animated resource cannot be decoded.
            }
        }

        // Update ---------------------------------------------------------------------------------------------------

        async Task CheckAsync()
        {
            if (busy)
                return;
            busy = true;
            SetBusy(true);
            try
            {
                Updater.CleanUpOldLauncher();
                SetState("Recherche des mises à jour…", "", "VÉRIFICATION", null);
                SetProgress(0, 1);

                var updater = new Updater(http, settings) { Repair = repairRequested };
                updater.Status += text => Dispatcher.Invoke(() => StatusText.Text = text);
                updater.Progress += (done, total) => Dispatcher.Invoke(() => SetProgress(done, total));
                updater.Transfer += (done, total) => Dispatcher.Invoke(() => ShowTransfer(done, total));

                try
                {
                    manifest = await updater.FetchManifestAsync();
                }
                catch (HttpRequestException)
                {
                    // Offline: the game can still start with what is installed
                    if (HasGameFolder())
                        SetState("Mises à jour indisponibles", "Le jeu peut quand même être lancé.", "JOUER", Play);
                    else
                        SetState("Impossible de joindre le serveur de mises à jour", "Vérifiez votre connexion.",
                            "RÉESSAYER", Retry);
                    return;
                }

                string knownRealm = settings.Realmlist;
                Remember(manifest);
                if (knownRealm != manifest.Realmlist)
                    await CheckRealmAsync();
                if (selfUpdate && await updater.UpdateSelfAsync(manifest))
                {
                    Close();
                    return;
                }

                if (!HasGameFolder() && !UseFolderBesideLauncher())
                {
                    SetState("Bienvenue !", "Indiquez le dossier de World of Warcraft 3.3.5a pour installer le jeu.",
                        "INSTALLER", async () =>
                        {
                            if (ChooseGameFolder())
                                await CheckAsync();
                        });
                    return;
                }

                if (IsGameRunning())
                {
                    SetState("World of Warcraft est ouvert", "Fermez le jeu pour installer la mise à jour.",
                        "RÉESSAYER", Retry);
                    return;
                }

                SetState("Vérification des fichiers…", "", "MISE À JOUR", null);
                transferClock = null;
                await updater.SyncAsync(manifest, settings.GameFolder);
                repairRequested = false;

                SetProgress(1, 1);
                SetState("Prêt à jouer", "Version " + manifest.Version, "JOUER", Play);
            }
            catch (UnauthorizedAccessException)
            {
                SetState("Accès refusé au dossier du jeu", "Placez le jeu hors de « Program Files ».", "RÉESSAYER",
                    Retry);
            }
            catch (Exception exception)
            {
                SetState("Une erreur est survenue", exception.Message, "RÉESSAYER", Retry);
            }
            finally
            {
                busy = false;
                SetBusy(false);
            }
        }

        async void Retry() => await CheckAsync();

        void Remember(Manifest latest)
        {
            settings.Realmlist = latest.Realmlist;
            settings.Version = latest.Version;
            settings.News = latest.News ?? new List<NewsItem>();
            settings.Save();
            ShowNews(settings.News);
            ShowVersion(latest.Version);
        }

        void SetState(string status, string detail, string action, Action onAction)
        {
            StatusText.Text = status;
            DetailText.Text = detail;
            PlayText.Text = action;
            playAction = onAction;
            PlayButton.IsEnabled = onAction != null;
        }

        void SetBusy(bool isBusy)
        {
            ChangeFolderButton.IsEnabled = !isBusy;
            RepairButton.IsEnabled = !isBusy && HasGameFolder();
            CacheButton.IsEnabled = !isBusy && HasGameFolder();
            OpenFolderButton.IsEnabled = HasGameFolder();
            ShowSettings();
        }

        void SetProgress(long done, long total)
        {
            double ratio = Math.Max(0, Math.Min(1, done / (double)Math.Max(total, 1)));
            double width = ProgressTrack.ActualWidth * ratio;
            ProgressFill.BeginAnimation(WidthProperty,
                new DoubleAnimation(width, TimeSpan.FromMilliseconds(180)) { FillBehavior = FillBehavior.HoldEnd });
        }

        void ShowTransfer(long done, long total)
        {
            if (transferClock == null)
                transferClock = Stopwatch.StartNew();
            double seconds = Math.Max(transferClock.Elapsed.TotalSeconds, 0.25);
            DetailText.Text = string.Format(French, "{0} sur {1} · {2}/s", Size(done), Size(total),
                Size((long)(done / seconds)));
        }

        static string Size(long bytes) =>
            bytes >= 1 << 20
                ? (bytes / 1048576.0).ToString("0.0", French) + " Mo"
                : Math.Max(1, bytes / 1024).ToString(French) + " Ko";

        void OnPlay(object sender, RoutedEventArgs e) => playAction?.Invoke();

        async void Play()
        {
            if (launching)
                return;

            launching = true;
            playAction = null;
            PlayButton.IsHitTestVisible = false;
            PlayText.Text = "LANCEMENT…";
            LaunchSpinner.Visibility = Visibility.Visible;
            LaunchSpinnerRotation.BeginAnimation(RotateTransform.AngleProperty, new DoubleAnimation(0, 360,
                TimeSpan.FromMilliseconds(850))
            {
                RepeatBehavior = RepeatBehavior.Forever,
            });
            StatusText.Text = "Lancement de World of Warcraft…";
            DetailText.Text = "Le client démarre, veuillez patienter.";

            await Dispatcher.Yield(DispatcherPriority.Render);

            try
            {
                Process game = Process.Start(new ProcessStartInfo(Path.Combine(settings.GameFolder, "Wow.exe"))
                {
                    WorkingDirectory = settings.GameFolder,
                    UseShellExecute = false,
                });
                if (game == null)
                    throw new InvalidOperationException("Windows n'a pas pu démarrer Wow.exe.");

                DateTime timeout = DateTime.UtcNow.AddSeconds(8);
                while (DateTime.UtcNow < timeout)
                {
                    await Task.Delay(100);
                    game.Refresh();
                    if (game.HasExited)
                        throw new InvalidOperationException("Wow.exe s'est fermé pendant son démarrage.");
                    if (game.MainWindowHandle != IntPtr.Zero)
                    {
                        await Task.Delay(250);
                        break;
                    }
                }

                Close();
            }
            catch (Exception exception)
            {
                launching = false;
                LaunchSpinnerRotation.BeginAnimation(RotateTransform.AngleProperty, null);
                LaunchSpinner.Visibility = Visibility.Collapsed;
                PlayButton.IsHitTestVisible = true;
                SetState("Impossible de lancer World of Warcraft", exception.Message, "RÉESSAYER", Play);
            }
        }

        // Game folder ----------------------------------------------------------------------------------------------

        bool HasGameFolder() =>
            !string.IsNullOrEmpty(settings.GameFolder) && IsGameFolder(settings.GameFolder);

        // A launcher placed in the game folder needs no question
        bool UseFolderBesideLauncher()
        {
            string own = Path.GetDirectoryName(Updater.ExecutablePath);
            return IsGameFolder(own) && UseGameFolder(own);
        }

        bool ChooseGameFolder()
        {
            while (true)
            {
                string folder = FolderPicker.Pick(new WindowInteropHelper(this).Handle,
                    "Dossier de World of Warcraft 3.3.5a (celui qui contient Wow.exe)", settings.GameFolder);
                if (folder == null)
                    return false;
                if (IsGameFolder(folder))
                    return UseGameFolder(folder);

                StatusText.Text = "Ce dossier ne contient pas le client WotLK 3.3.5a";
                DetailText.Text = "Choisissez le dossier où se trouve Wow.exe (build 12340).";
            }
        }

        bool UseGameFolder(string folder)
        {
            if (!string.Equals(settings.GameFolder, folder, StringComparison.OrdinalIgnoreCase))
            {
                settings.GameFolder = folder;
                settings.Hashes.Clear();
                settings.Installed.Clear();
                settings.Bundles.Clear();
                settings.Save();
            }
            ShowSettings();
            return true;
        }

        static bool IsGameFolder(string folder)
        {
            string exe = Path.Combine(folder, "Wow.exe");
            if (!File.Exists(exe))
                return false;
            FileVersionInfo version = FileVersionInfo.GetVersionInfo(exe);
            return version.FileMajorPart == 3 && version.FileMinorPart == 3 && version.FileBuildPart == 5 &&
                version.FilePrivatePart == 12340;
        }

        bool IsGameRunning()
        {
            string exe = Path.Combine(settings.GameFolder, "Wow.exe");
            return Process.GetProcessesByName("Wow").Any(process =>
            {
                try
                {
                    return string.Equals(process.MainModule.FileName, exe, StringComparison.OrdinalIgnoreCase);
                }
                catch (Exception)
                {
                    // A process we may not inspect is not proof the game is running from this folder
                    return false;
                }
            });
        }

        // Realm ----------------------------------------------------------------------------------------------------

        async Task CheckRealmAsync()
        {
            string realmlist = settings.Realmlist;
            if (string.IsNullOrWhiteSpace(realmlist))
            {
                ShowRealm(null, "Adresse inconnue");
                return;
            }

            string host = realmlist.Trim();
            int port = 3724;
            int colon = host.LastIndexOf(':');
            if (colon > 0 && int.TryParse(host.Substring(colon + 1), out int customPort))
            {
                port = customPort;
                host = host.Substring(0, colon);
            }

            var clock = Stopwatch.StartNew();
            bool online;
            using (var client = new TcpClient())
            {
                try
                {
                    Task connect = client.ConnectAsync(host, port);
                    online = await Task.WhenAny(connect, Task.Delay(3000)) == connect && client.Connected;
                }
                catch (Exception)
                {
                    online = false;
                }
            }
            ShowRealm(online, online ? "Latence " + clock.ElapsedMilliseconds + " ms" : "Réessai dans 30 s");
        }

        void ShowRealm(bool? online, string detail)
        {
            Brush brush = online == true ? (Brush)FindResource("Online")
                : online == false ? (Brush)FindResource("Offline") : (Brush)FindResource("Muted");
            RealmDot.Fill = brush;
            RealmHalo.Fill = brush;
            RealmState.Text = online == true ? "En ligne" : online == false ? "Hors ligne" : "Inconnu";
            RealmDetail.Text = detail;

            // A slow pulse while the realm is up
            RealmHalo.BeginAnimation(OpacityProperty, online == true
                ? new DoubleAnimation(0.45, 0.1, TimeSpan.FromSeconds(1.6))
                {
                    AutoReverse = true,
                    RepeatBehavior = RepeatBehavior.Forever,
                }
                : null);
        }

        // News -----------------------------------------------------------------------------------------------------

        void ShowNews(List<NewsItem> news)
        {
            List<NewsView> views = (news ?? new List<NewsItem>()).Select((item, index) => new NewsView(item, index))
                .ToList();
            if (views.Count == 0)
                views.Add(new NewsView(new NewsItem
                {
                    Title = "Bienvenue sur Evolutions",
                    Tag = "Serveur",
                    Image = "wintergrasp",
                    Body = "Le launcher installe et tient à jour tout ce dont le jeu a besoin. Cliquez sur Jouer, " +
                        "il s'occupe du reste.",
                }, 0));

            NewsView featured = views[0];
            FeaturedTagText.Text = featured.Tag;
            FeaturedTitle.Text = featured.Title;
            FeaturedBody.Text = featured.Body;
            NewsCards.ItemsSource = views.Take(3).ToList();
            NewsList.ItemsSource = views;
        }

        void ShowVersion(string version)
        {
            if (string.IsNullOrEmpty(version))
                return;
            VersionText.Text = "v" + version;
            VersionPill.Visibility = Visibility.Visible;
            AboutText.Text = "Client Evolutions v" + version;
        }

        sealed class NewsView
        {
            public NewsView(NewsItem item, int index)
            {
                Title = item.Title ?? "";
                Tag = (item.Tag ?? "Nouveauté").ToUpper(French);
                Body = item.Body ?? "";
                DateText = DateTime.TryParseExact(item.Date, "yyyy-MM-dd", CultureInfo.InvariantCulture,
                    DateTimeStyles.None, out DateTime date)
                    ? date.ToString("d MMMM yyyy", French)
                    : "";
                string key = NewsImages.Contains(item.Image) ? item.Image : NewsImages[index % NewsImages.Length];
                Image = new BitmapImage(new Uri("pack://application:,,,/Evolutions;component/Assets/news-" + key + ".jpg"));
            }

            public string Title { get; }
            public string Tag { get; }
            public string Body { get; }
            public string DateText { get; }
            public ImageSource Image { get; }
        }

        void OnNewsCardClick(object sender, RoutedEventArgs e) => NavNews.IsChecked = true;

        void OnOpenNews(object sender, RoutedEventArgs e) => NavNews.IsChecked = true;

        // Settings -------------------------------------------------------------------------------------------------

        void ShowSettings()
        {
            if (FolderText == null)
                return;
            FolderText.Text = HasGameFolder() ? settings.GameFolder : "Aucun dossier choisi";
        }

        void OnOpenFolder(object sender, RoutedEventArgs e)
        {
            if (HasGameFolder())
                Process.Start("explorer.exe", "\"" + settings.GameFolder + "\"");
        }

        async void OnChangeFolder(object sender, RoutedEventArgs e)
        {
            if (!busy && ChooseGameFolder())
                await CheckAsync();
        }

        async void OnRepair(object sender, RoutedEventArgs e)
        {
            if (busy || !HasGameFolder())
                return;
            settings.Hashes.Clear();
            settings.Save();
            repairRequested = true;
            NavHome.IsChecked = true;
            await CheckAsync();
        }

        void OnClearCache(object sender, RoutedEventArgs e)
        {
            if (busy || !HasGameFolder())
                return;
            if (IsGameRunning())
            {
                StatusText.Text = "Fermez World of Warcraft pour vider le cache";
                return;
            }
            string cache = Path.Combine(settings.GameFolder, "Cache");
            try
            {
                if (Directory.Exists(cache))
                    Directory.Delete(cache, true);
                StatusText.Text = "Cache vidé";
            }
            catch (Exception exception)
            {
                StatusText.Text = "Impossible de vider le cache : " + exception.Message;
            }
        }

        // Window ---------------------------------------------------------------------------------------------------

        void OnNavigate(object sender, RoutedEventArgs e)
        {
            if (HomePage == null)
                return;
            Grid page = sender == NavNews ? NewsPage : sender == NavSettings ? SettingsPage : HomePage;
            foreach (Grid candidate in new[] { HomePage, NewsPage, SettingsPage })
                candidate.Visibility = candidate == page ? Visibility.Visible : Visibility.Collapsed;

            bool home = page == HomePage;
            var fade = TimeSpan.FromMilliseconds(250);
            Veil.BeginAnimation(OpacityProperty, new DoubleAnimation(home ? 0 : 1, fade));
            Logo.BeginAnimation(OpacityProperty, new DoubleAnimation(home ? 1 : 0, fade));
            ((Storyboard)FindResource("PageIn")).Begin(page);
        }

        void OnDragWindow(object sender, MouseButtonEventArgs e)
        {
            if (e.ButtonState == MouseButtonState.Pressed)
                DragMove();
        }

        void OnMinimize(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;

        void OnClose(object sender, RoutedEventArgs e) => Close();

        // Windows 11 draws rounded corners and a dark border for a borderless window only when asked
        static class Dwm
        {
            const int UseImmersiveDarkMode = 20;
            const int WindowCornerPreference = 33;
            const int BorderColor = 34;

            [DllImport("dwmapi.dll")]
            static extern int DwmSetWindowAttribute(IntPtr window, int attribute, ref int value, int size);

            public static void Style(IntPtr window)
            {
                try
                {
                    int dark = 1, round = 2, border = 0x002A2622;
                    DwmSetWindowAttribute(window, UseImmersiveDarkMode, ref dark, sizeof(int));
                    DwmSetWindowAttribute(window, WindowCornerPreference, ref round, sizeof(int));
                    DwmSetWindowAttribute(window, BorderColor, ref border, sizeof(int));
                }
                catch (DllNotFoundException)
                {
                    // Older Windows: square corners
                }
            }
        }
    }
}
