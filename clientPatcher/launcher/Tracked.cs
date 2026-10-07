using System.Globalization;
using System.Windows;
using System.Windows.Documents;
using System.Windows.Media;

namespace Evolutions
{
    // A line of text with letter spacing, which WPF's TextBlock cannot do: the stamped capitals of the forge (its
    // labels, tabs and the ingot's word). Spacing is in em, as CSS's letter-spacing; one line, no wrapping.
    public sealed class Tracked : FrameworkElement
    {
        public static readonly DependencyProperty TextProperty = DependencyProperty.Register(nameof(Text),
            typeof(string), typeof(Tracked), new FrameworkPropertyMetadata("",
                FrameworkPropertyMetadataOptions.AffectsMeasure | FrameworkPropertyMetadataOptions.AffectsRender));

        public static readonly DependencyProperty SpacingProperty = DependencyProperty.Register(nameof(Spacing),
            typeof(double), typeof(Tracked), new FrameworkPropertyMetadata(0.14,
                FrameworkPropertyMetadataOptions.AffectsMeasure | FrameworkPropertyMetadataOptions.AffectsRender));

        public static readonly DependencyProperty FontFamilyProperty = TextElement.FontFamilyProperty.AddOwner(
            typeof(Tracked), new FrameworkPropertyMetadata(SystemFonts.MessageFontFamily,
                FrameworkPropertyMetadataOptions.Inherits | FrameworkPropertyMetadataOptions.AffectsMeasure |
                FrameworkPropertyMetadataOptions.AffectsRender));

        public static readonly DependencyProperty FontSizeProperty = TextElement.FontSizeProperty.AddOwner(
            typeof(Tracked), new FrameworkPropertyMetadata(13.0,
                FrameworkPropertyMetadataOptions.Inherits | FrameworkPropertyMetadataOptions.AffectsMeasure |
                FrameworkPropertyMetadataOptions.AffectsRender));

        public static readonly DependencyProperty ForegroundProperty = TextElement.ForegroundProperty.AddOwner(
            typeof(Tracked), new FrameworkPropertyMetadata(Brushes.White,
                FrameworkPropertyMetadataOptions.Inherits | FrameworkPropertyMetadataOptions.AffectsRender));

        public string Text
        {
            get => (string)GetValue(TextProperty);
            set => SetValue(TextProperty, value);
        }

        public double Spacing
        {
            get => (double)GetValue(SpacingProperty);
            set => SetValue(SpacingProperty, value);
        }

        public FontFamily FontFamily
        {
            get => (FontFamily)GetValue(FontFamilyProperty);
            set => SetValue(FontFamilyProperty, value);
        }

        public double FontSize
        {
            get => (double)GetValue(FontSizeProperty);
            set => SetValue(FontSizeProperty, value);
        }

        public Brush Foreground
        {
            get => (Brush)GetValue(ForegroundProperty);
            set => SetValue(ForegroundProperty, value);
        }

        FormattedText Glyph(string text) => new FormattedText(text, CultureInfo.CurrentUICulture,
            FlowDirection.LeftToRight, new Typeface(FontFamily, FontStyles.Normal, FontWeights.Normal,
                FontStretches.Normal), FontSize, Foreground, VisualTreeHelper.GetDpi(this).PixelsPerDip);

        protected override Size MeasureOverride(Size constraint)
        {
            string text = Text ?? "";
            double width = 0;
            foreach (char letter in text)
                width += Glyph(letter.ToString()).WidthIncludingTrailingWhitespace;
            width += Spacing * FontSize * System.Math.Max(0, text.Length - 1);
            return new Size(width, Glyph(text.Length > 0 ? text : " ").Height);
        }

        protected override void OnRender(DrawingContext drawing)
        {
            string text = Text ?? "";
            double x = 0;
            foreach (char letter in text)
            {
                FormattedText glyph = Glyph(letter.ToString());
                drawing.DrawText(glyph, new Point(x, 0));
                x += glyph.WidthIncludingTrailingWhitespace + Spacing * FontSize;
            }
        }
    }
}
