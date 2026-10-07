using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;

namespace Evolutions
{
    // The forge's life (Assets/Forge, .agents/plans/launcher-forge): its heat - the hot paintings cross-fading over
    // the cold ones as the realm opens or closes - the fire's light flickering, smoke drifting across, embers rising
    // off the blade while it is hot, three depths following the mouse a little, and a very slow drift over it all.
    // Storyboards over render transforms and opacities, capped at 30 frames a second, paused while minimised.
    internal sealed class Backdrop
    {
        // Where the embers leave from on the 1920 x 1140 scene: the blade on the anvil and the furnace's mouth
        static readonly Rect EmberSource = new Rect(1240, 470, 470, 110);
        const int EmberCount = 16;
        // How far each depth moves with the mouse, in scene pixels, at the window's edge
        const double BackTravel = 10, MidTravel = 22, FrontTravel = 44;

        const string Scale = "(UIElement.RenderTransform).(TransformGroup.Children)[0].";
        const string Shift = "(UIElement.RenderTransform).(TransformGroup.Children)[1].";

        readonly FrameworkElement owner;
        readonly UIElement wallHot, hotMid, embers;
        readonly TranslateTransform back, mid, front;
        readonly Storyboard storyboard = new Storyboard();
        bool started;
        bool? hot;

        public Backdrop(FrameworkElement owner, UIElement scene, UIElement wallHot, UIElement hotMid,
            UIElement fireLight, UIElement smoke, Canvas embers, TranslateTransform back, TranslateTransform mid,
            TranslateTransform front)
        {
            this.owner = owner;
            this.wallHot = wallHot;
            this.hotMid = hotMid;
            this.embers = embers;
            this.back = back;
            this.mid = mid;
            this.front = front;
            Timeline.SetDesiredFrameRate(storyboard, 30);

            // A breath of zoom and a drift, on periods that never quite line up; the edges never show
            Drift(scene, Scale + "(ScaleTransform.ScaleX)", 1.04, 1.07, 37);
            Drift(scene, Scale + "(ScaleTransform.ScaleY)", 1.04, 1.07, 37);
            Drift(scene, Shift + "(TranslateTransform.X)", -14, 14, 43);
            Drift(scene, Shift + "(TranslateTransform.Y)", -6, 6, 31);

            // The fire breathes: a slow swell and a quicker flicker on top of it
            var flicker = new DoubleAnimationUsingKeyFrames { RepeatBehavior = RepeatBehavior.Forever };
            double[] levels = { 0.86, 1.0, 0.9, 0.97, 0.82, 1.0, 0.92, 0.88, 1.0, 0.86 };
            for (int index = 0; index < levels.Length; ++index)
                flicker.KeyFrames.Add(new EasingDoubleKeyFrame(levels[index],
                    KeyTime.FromTimeSpan(TimeSpan.FromMilliseconds(index * 260)),
                    new SineEase { EasingMode = EasingMode.EaseInOut }));
            Add(flicker, fireLight, new PropertyPath(UIElement.OpacityProperty));

            // The smoke: two copies of the tileable band, sliding one width over a minute and a half
            var slide = new DoubleAnimation(0, -1920, TimeSpan.FromSeconds(90)) { RepeatBehavior = RepeatBehavior.Forever };
            Add(slide, smoke, new PropertyPath("(UIElement.RenderTransform).(TranslateTransform.X)"));

            var random = new Random(1920);
            for (int index = 0; index < EmberCount; ++index)
                AddEmber(embers, index % 16, random);
        }

        public void Start()
        {
            if (!started)
            {
                storyboard.Begin(owner, true);
                started = true;
            }
            else
            {
                storyboard.Resume(owner);
            }
        }

        public void Pause()
        {
            if (started)
                storyboard.Pause(owner);
        }

        public void Stop()
        {
            if (started)
                storyboard.Stop(owner);
            started = false;
        }

        // The forge lit or cold: the hot paintings (the wall, the anvil, the fire's light, the embers) fade over the
        // cold ones. The first call sets it at once; later ones take their time, as metal heats and cools.
        public void SetHeat(bool lit)
        {
            if (hot == lit)
                return;
            TimeSpan duration = hot == null ? TimeSpan.Zero : TimeSpan.FromSeconds(lit ? 1.8 : 3.0);
            hot = lit;
            foreach (UIElement layer in new[] { wallHot, hotMid, embers })
                layer.BeginAnimation(UIElement.OpacityProperty, new DoubleAnimation(lit ? 1 : 0, duration)
                {
                    EasingFunction = new SineEase { EasingMode = EasingMode.EaseInOut },
                });
        }

        // The mouse, from -1 to 1 across the window: the far wall barely moves, the foreground the most
        public void Parallax(double x, double y)
        {
            Ease(back, -x * BackTravel, -y * BackTravel * 0.5);
            Ease(mid, -x * MidTravel, -y * MidTravel * 0.5);
            Ease(front, -x * FrontTravel, -y * FrontTravel * 0.5);
        }

        static void Ease(TranslateTransform transform, double x, double y)
        {
            var easing = new CubicEase { EasingMode = EasingMode.EaseOut };
            transform.BeginAnimation(TranslateTransform.XProperty,
                new DoubleAnimation(x, TimeSpan.FromMilliseconds(600)) { EasingFunction = easing });
            transform.BeginAnimation(TranslateTransform.YProperty,
                new DoubleAnimation(y, TimeSpan.FromMilliseconds(600)) { EasingFunction = easing });
        }

        void Drift(UIElement target, string path, double from, double to, double seconds)
        {
            var animation = new DoubleAnimation(from, to, TimeSpan.FromSeconds(seconds))
            {
                AutoReverse = true,
                RepeatBehavior = RepeatBehavior.Forever,
                EasingFunction = new SineEase { EasingMode = EasingMode.EaseInOut },
            };
            Add(animation, target, new PropertyPath(path));
        }

        // One painted ember (Assets/Forge/emberNN.png) that leaves the fire, rises with a sway and fades, then
        // starts over
        void AddEmber(Canvas layer, int sprite, Random random)
        {
            double size = 14 + random.NextDouble() * 22;
            double seconds = 3.4 + random.NextDouble() * 3.4;
            double rise = 220 + random.NextDouble() * 260;
            double sway = (random.NextDouble() - 0.5) * 140;

            var ember = new Image
            {
                Width = size,
                Height = size,
                Opacity = 0,
                RenderTransform = new TranslateTransform(),
                Source = new BitmapImage(new Uri(
                    $"pack://application:,,,/Evolutions;component/Assets/Forge/ember{sprite:00}.png")),
            };
            Canvas.SetLeft(ember, EmberSource.X + random.NextDouble() * EmberSource.Width);
            Canvas.SetTop(ember, EmberSource.Y + random.NextDouble() * EmberSource.Height);
            layer.Children.Add(ember);

            var duration = new Duration(TimeSpan.FromSeconds(seconds));
            TimeSpan phase = TimeSpan.FromSeconds(-random.NextDouble() * seconds);

            Add(new DoubleAnimation(0, -rise, duration)
            {
                RepeatBehavior = RepeatBehavior.Forever,
                BeginTime = phase,
                EasingFunction = new QuadraticEase { EasingMode = EasingMode.EaseOut },
            }, ember, new PropertyPath("(UIElement.RenderTransform).(TranslateTransform.Y)"));
            Add(new DoubleAnimation(0, sway, duration)
            {
                RepeatBehavior = RepeatBehavior.Forever,
                BeginTime = phase,
                EasingFunction = new SineEase { EasingMode = EasingMode.EaseInOut },
            }, ember, new PropertyPath("(UIElement.RenderTransform).(TranslateTransform.X)"));

            var fade = new DoubleAnimationUsingKeyFrames { Duration = duration, RepeatBehavior = RepeatBehavior.Forever,
                BeginTime = phase };
            fade.KeyFrames.Add(new LinearDoubleKeyFrame(0, KeyTime.FromPercent(0)));
            fade.KeyFrames.Add(new LinearDoubleKeyFrame(1, KeyTime.FromPercent(0.12)));
            fade.KeyFrames.Add(new LinearDoubleKeyFrame(0.6, KeyTime.FromPercent(0.6)));
            fade.KeyFrames.Add(new LinearDoubleKeyFrame(0, KeyTime.FromPercent(1)));
            Add(fade, ember, new PropertyPath(UIElement.OpacityProperty));
        }

        void Add(Timeline animation, DependencyObject target, PropertyPath path)
        {
            Storyboard.SetTarget(animation, target);
            Storyboard.SetTargetProperty(animation, path);
            storyboard.Children.Add(animation);
        }
    }
}
