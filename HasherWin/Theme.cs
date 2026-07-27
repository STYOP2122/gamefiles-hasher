namespace HasherWin;

internal static class Theme
{
    public static readonly Color Background = Color.FromArgb(30, 30, 34);
    public static readonly Color Panel = Color.FromArgb(42, 42, 48);
    public static readonly Color Text = Color.FromArgb(230, 230, 235);
    public static readonly Color Muted = Color.FromArgb(150, 150, 160);
    public static readonly Color Accent = Color.FromArgb(72, 149, 239);
    public static readonly Color EditBackground = Color.FromArgb(24, 24, 28);
    public static readonly Color LogBackground = Color.FromArgb(20, 20, 24);

    public static void ApplyDark(Control control)
    {
        control.BackColor = Background;
        control.ForeColor = Text;

        foreach (Control child in control.Controls)
            StyleControl(child);
    }

    private static void StyleControl(Control control)
    {
        switch (control)
        {
            case Label label:
                label.BackColor = Background;
                label.ForeColor = Text;
                break;

            case TextBox textBox:
                textBox.BackColor = control.Parent?.Name == "logPanel" ? LogBackground : EditBackground;
                textBox.ForeColor = control.Parent?.Name == "logPanel" ? Muted : Text;
                textBox.BorderStyle = BorderStyle.FixedSingle;
                break;

            case Button button:
                button.FlatStyle = FlatStyle.System;
                button.BackColor = Panel;
                button.ForeColor = Text;
                break;

            case ProgressBar:
                control.BackColor = EditBackground;
                control.ForeColor = Accent;
                break;

            default:
                control.BackColor = Background;
                control.ForeColor = Text;
                break;
        }

        foreach (Control child in control.Controls)
            StyleControl(child);
    }
}
