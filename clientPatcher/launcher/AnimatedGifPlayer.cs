using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media.Imaging;
using System.Windows.Resources;
using System.Windows.Threading;

namespace Evolutions
{
    internal sealed class AnimatedGifPlayer : IDisposable
    {
        readonly Canvas target;
        readonly Uri resource;
        readonly DispatcherTimer timer = new DispatcherTimer(DispatcherPriority.Render);

        IReadOnlyList<GifFrame> frames;
        int frameIndex;
        bool wantsPlayback;

        public AnimatedGifPlayer(Canvas target, Uri resource)
        {
            this.target = target;
            this.resource = resource;
            timer.Tick += OnTick;
        }

        public async Task LoadAsync()
        {
            StreamResourceInfo resourceInfo = Application.GetResourceStream(resource);
            if (resourceInfo == null)
                throw new FileNotFoundException("Animated launcher background is missing.", resource.ToString());

            byte[] gif;
            using (resourceInfo.Stream)
            using (var buffer = new MemoryStream())
            {
                await resourceInfo.Stream.CopyToAsync(buffer);
                gif = buffer.ToArray();
            }

            GifSequence sequence = await Task.Run(() => Decode(gif));
            frames = sequence.Frames;
            frameIndex = 0;
            DrawFrame(frames[0], true);

            if (wantsPlayback)
                ScheduleNextFrame();
        }

        public void Start()
        {
            wantsPlayback = true;
            if (frames != null && !timer.IsEnabled)
                ScheduleNextFrame();
        }

        public void Stop()
        {
            wantsPlayback = false;
            timer.Stop();
        }

        void OnTick(object sender, EventArgs eventArgs)
        {
            timer.Stop();
            if (!wantsPlayback || frames == null)
                return;

            frameIndex = (frameIndex + 1) % frames.Count;
            DrawFrame(frames[frameIndex], frameIndex == 0);
            ScheduleNextFrame();
        }

        void ScheduleNextFrame()
        {
            timer.Interval = frames[frameIndex].Delay;
            timer.Start();
        }

        void DrawFrame(GifFrame frame, bool startsLoop)
        {
            if (startsLoop)
                target.Children.Clear();

            var image = new Image
            {
                Source = frame.Source,
                Width = frame.Source.PixelWidth,
                Height = frame.Source.PixelHeight,
                IsHitTestVisible = false,
            };
            Canvas.SetLeft(image, frame.Left);
            Canvas.SetTop(image, frame.Top);
            target.Children.Add(image);
        }

        static GifSequence Decode(byte[] gif)
        {
            using (var stream = new MemoryStream(gif, false))
            {
                var decoder = new GifBitmapDecoder(stream, BitmapCreateOptions.PreservePixelFormat,
                    BitmapCacheOption.OnLoad);
                var decodedFrames = new List<GifFrame>(decoder.Frames.Count);

                foreach (BitmapFrame frame in decoder.Frames)
                {
                    int left = ReadMetadata(frame, "/imgdesc/Left");
                    int top = ReadMetadata(frame, "/imgdesc/Top");
                    TimeSpan delay = ReadDelay(frame);
                    frame.Freeze();
                    decodedFrames.Add(new GifFrame(frame, left, top, delay));
                }

                if (decodedFrames.Count == 0)
                    throw new InvalidDataException("Animated launcher background has no frames.");

                return new GifSequence(decodedFrames);
            }
        }

        static int ReadMetadata(BitmapFrame frame, string query)
        {
            try
            {
                if (frame.Metadata is BitmapMetadata metadata)
                    return Convert.ToInt32(metadata.GetQuery(query));
            }
            catch (NotSupportedException)
            {
            }

            return 0;
        }

        static TimeSpan ReadDelay(BitmapFrame frame)
        {
            try
            {
                if (frame.Metadata is BitmapMetadata metadata && metadata.GetQuery("/grctlext/Delay") is ushort delay)
                    return TimeSpan.FromMilliseconds(Math.Max(20, delay * 10));
            }
            catch (NotSupportedException)
            {
                // Some GIF encoders omit per-frame metadata.
            }

            return TimeSpan.FromMilliseconds(42);
        }

        public void Dispose()
        {
            timer.Stop();
            timer.Tick -= OnTick;
            frames = null;
        }

        sealed class GifSequence
        {
            public GifSequence(IReadOnlyList<GifFrame> frames) => Frames = frames;

            public IReadOnlyList<GifFrame> Frames { get; }
        }

        sealed class GifFrame
        {
            public GifFrame(BitmapSource source, int left, int top, TimeSpan delay)
            {
                Source = source;
                Left = left;
                Top = top;
                Delay = delay;
            }

            public BitmapSource Source { get; }
            public int Left { get; }
            public int Top { get; }
            public TimeSpan Delay { get; }
        }
    }
}
