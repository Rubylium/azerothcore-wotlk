using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Shapes;

namespace Evolutions
{
    // The home backdrop's life: a very slow Ken Burns drift over the art, the braziers' firelight and the city's
    // beam breathing, and a few embers rising off the braziers. Everything is a storyboard over render transforms
    // and opacities - no per-frame work of our own - capped at 30 frames a second, and paused while the window is
    // minimised.
    internal sealed class Backdrop
    {
        // Where the lights sit on the 1672x941 art
        static readonly Point LeftBrazier = new Point(310, 552);
        static readonly Point RightBrazier = new Point(1362, 552);

        const int EmbersPerBrazier = 7;

        // The scene's RenderTransform is a TransformGroup of a ScaleTransform then a TranslateTransform
        const string Scale = "(UIElement.RenderTransform).(TransformGroup.Children)[0].";
        const string Shift = "(UIElement.RenderTransform).(TransformGroup.Children)[1].";

        readonly FrameworkElement owner;
        readonly Storyboard storyboard = new Storyboard();
        bool started;

        public Backdrop(FrameworkElement owner, UIElement scene, UIElement beaconHalo, UIElement beaconBeam,
            UIElement leftGlow, UIElement leftCore, UIElement rightGlow, UIElement rightCore, Canvas embers)
        {
            this.owner = owner;
            Timeline.SetDesiredFrameRate(storyboard, 30);

            // Ken Burns: 5% of zoom on a 70 s breath, and a drift on two other periods so the path never quite
            // repeats. The scale never drops under 1.03 around (0.5, 0.4) and the drift stays inside what that and
            // the cover crop leave spare, so the art's edges never show.
            Drift(scene, Scale + "(ScaleTransform.ScaleX)", 1.03, 1.08, 35);
            Drift(scene, Scale + "(ScaleTransform.ScaleY)", 1.03, 1.08, 35);
            Drift(scene, Shift + "(TranslateTransform.X)", -26, 26, 41);
            Drift(scene, Shift + "(TranslateTransform.Y)", -8, 8, 29);

            // Firelight and the beam: slow, slightly out of step with one another
            Pulse(beaconHalo, 0.4, 0.75, 4.6, 0);
            Pulse(beaconBeam, 0.45, 0.85, 3.3, 1.1);
            Pulse(leftGlow, 0.45, 0.8, 2.7, 0);
            Pulse(leftCore, 0.5, 0.95, 1.6, 0.4);
            Pulse(rightGlow, 0.45, 0.8, 3.1, 0.9);
            Pulse(rightCore, 0.5, 0.95, 1.8, 0.2);

            // Fixed seed: the same embers every run
            var random = new Random(1672);
            foreach (Point brazier in new[] { LeftBrazier, RightBrazier })
                for (int index = 0; index < EmbersPerBrazier; ++index)
                    AddEmber(embers, brazier, random);
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

        // A storyboard cannot aim at a transform on its own, only at an element and a path down to it
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

        void Pulse(UIElement target, double low, double high, double seconds, double delay)
        {
            var animation = new DoubleAnimation(low, high, TimeSpan.FromSeconds(seconds))
            {
                AutoReverse = true,
                RepeatBehavior = RepeatBehavior.Forever,
                BeginTime = TimeSpan.FromSeconds(-delay),
                EasingFunction = new SineEase { EasingMode = EasingMode.EaseInOut },
            };
            Add(animation, target, new PropertyPath(UIElement.OpacityProperty));
        }

        // A small soft spark that leaves the fire, rises with a little sway and fades out, then starts over
        void AddEmber(Canvas layer, Point brazier, Random random)
        {
            double size = 4 + random.NextDouble() * 5;
            double seconds = 3.6 + random.NextDouble() * 3.2;
            double rise = 150 + random.NextDouble() * 120;
            double sway = (random.NextDouble() - 0.5) * 60;

            var ember = new Ellipse
            {
                Width = size,
                Height = size,
                Opacity = 0,
                RenderTransform = new TranslateTransform(),
                Fill = new RadialGradientBrush(Color.FromArgb(0xFF, 0xFF, 0xE2, 0x9A),
                    Color.FromArgb(0, 0xFF, 0x9A, 0x3A)),
            };
            ember.Fill.Freeze();
            Canvas.SetLeft(ember, brazier.X - size / 2 + (random.NextDouble() - 0.5) * 80);
            Canvas.SetTop(ember, brazier.Y - 20 + (random.NextDouble() - 0.5) * 24);
            layer.Children.Add(ember);

            var duration = new Duration(TimeSpan.FromSeconds(seconds));
            // Spread over its own cycle so the embers never leave together
            TimeSpan phase = TimeSpan.FromSeconds(-random.NextDouble() * seconds);

            var up = new DoubleAnimation(0, -rise, duration)
            {
                RepeatBehavior = RepeatBehavior.Forever,
                BeginTime = phase,
                EasingFunction = new QuadraticEase { EasingMode = EasingMode.EaseOut },
            };
            Add(up, ember, new PropertyPath("(UIElement.RenderTransform).(TranslateTransform.Y)"));

            var across = new DoubleAnimation(0, sway, duration)
            {
                RepeatBehavior = RepeatBehavior.Forever,
                BeginTime = phase,
                EasingFunction = new SineEase { EasingMode = EasingMode.EaseInOut },
            };
            Add(across, ember, new PropertyPath("(UIElement.RenderTransform).(TranslateTransform.X)"));

            var fade = new DoubleAnimationUsingKeyFrames
            {
                Duration = duration,
                RepeatBehavior = RepeatBehavior.Forever,
                BeginTime = phase,
            };
            fade.KeyFrames.Add(new LinearDoubleKeyFrame(0, KeyTime.FromPercent(0)));
            fade.KeyFrames.Add(new LinearDoubleKeyFrame(0.9, KeyTime.FromPercent(0.15)));
            fade.KeyFrames.Add(new LinearDoubleKeyFrame(0.55, KeyTime.FromPercent(0.6)));
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
