using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace Evolutions
{
    // The painted riveted plate (Assets/Forge/ironPlate.png) laid behind a panel at any size: cut in nine, its corners
    // (and their rivets) at a fixed size, its edges stretched along their length, its plain middle stretched to fill.
    // Placed first in a Grid, under the panel's content.
    public sealed class IronPlate : Grid
    {
        // The plate inside its painting, and how far in from each side its corners and bevel run
        static readonly Int32Rect Bounds = new Int32Rect(24, 75, 976, 361);
        const int Slice = 72;

        static BitmapSource source;

        public static readonly DependencyProperty CornerSizeProperty = DependencyProperty.Register(nameof(CornerSize),
            typeof(double), typeof(IronPlate), new FrameworkPropertyMetadata(26.0, (o, e) => ((IronPlate)o).Build()));

        public double CornerSize
        {
            get => (double)GetValue(CornerSizeProperty);
            set => SetValue(CornerSizeProperty, value);
        }

        public IronPlate()
        {
            IsHitTestVisible = false;
            Build();
        }

        // The pieces would ask for their painted size; the plate takes whatever its panel's content gives it
        protected override Size MeasureOverride(Size constraint)
        {
            base.MeasureOverride(new Size(0, 0));
            return new Size(0, 0);
        }

        static BitmapSource Source()
        {
            if (source == null)
            {
                var image = new BitmapImage(new Uri("pack://application:,,,/Evolutions;component/Assets/Forge/ironPlate.png"));
                source = new CroppedBitmap(image, Bounds);
                source.Freeze();
            }
            return source;
        }

        void Build()
        {
            Children.Clear();
            RowDefinitions.Clear();
            ColumnDefinitions.Clear();
            var corner = new GridLength(CornerSize);
            foreach (GridLength length in new[] { corner, new GridLength(1, GridUnitType.Star), corner })
            {
                RowDefinitions.Add(new RowDefinition { Height = length });
                ColumnDefinitions.Add(new ColumnDefinition { Width = length });
            }

            BitmapSource plate = Source();
            int[] xs = { 0, Slice, plate.PixelWidth - Slice, plate.PixelWidth };
            int[] ys = { 0, Slice, plate.PixelHeight - Slice, plate.PixelHeight };
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 3; ++column)
                {
                    var piece = new CroppedBitmap(plate, new Int32Rect(xs[column], ys[row], xs[column + 1] - xs[column],
                        ys[row + 1] - ys[row]));
                    piece.Freeze();
                    var image = new Image { Source = piece, Stretch = Stretch.Fill };
                    RenderOptions.SetBitmapScalingMode(image, BitmapScalingMode.HighQuality);
                    SetRow(image, row);
                    SetColumn(image, column);
                    Children.Add(image);
                }
        }
    }
}
