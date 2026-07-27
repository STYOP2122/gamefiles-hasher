#nullable enable
namespace HasherWin;

internal sealed partial class MainForm
{
    private System.ComponentModel.IContainer? components = null;

    private Label lblTitle = null!;
    private Label lblSubtitle = null!;
    private Label lblInput = null!;
    private TextBox txtInput = null!;
    private Button btnBrowseInput = null!;
    private Label lblOutput = null!;
    private TextBox txtOutput = null!;
    private Button btnBrowseOutput = null!;
    private Label lblProgress = null!;
    private ProgressBar progressBar = null!;
    private Label lblStatus = null!;
    private Label lblCounter = null!;
    private Button btnStart = null!;
    private Button btnCancel = null!;
    private Button btnOpenOutput = null!;
    private Label lblLog = null!;
    private TextBox txtLog = null!;

    protected override void Dispose(bool disposing)
    {
        if (disposing)
            components?.Dispose();
        base.Dispose(disposing);
    }

    private void InitializeComponent()
    {
        components = new System.ComponentModel.Container();

        lblTitle = new Label();
        lblSubtitle = new Label();
        lblInput = new Label();
        txtInput = new TextBox();
        btnBrowseInput = new Button();
        lblOutput = new Label();
        txtOutput = new TextBox();
        btnBrowseOutput = new Button();
        lblProgress = new Label();
        progressBar = new ProgressBar();
        lblStatus = new Label();
        lblCounter = new Label();
        btnStart = new Button();
        btnCancel = new Button();
        btnOpenOutput = new Button();
        lblLog = new Label();
        txtLog = new TextBox();

        SuspendLayout();

        const int margin = 20;
        const int browseWidth = 110;
        const int editWidth = 520;
        const int rowHeight = 28;
        var y = margin;

        Text = "Hasher - SHA-256 Manifest Generator";
        ClientSize = new Size(680, 480);
        MinimumSize = new Size(680, 480);
        StartPosition = FormStartPosition.CenterScreen;
        Font = new Font("Segoe UI", 9F);
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;

        lblTitle.AutoSize = true;
        lblTitle.Location = new Point(margin, y);
        lblTitle.Text = "FILE HASHER";
        y += 30;

        lblSubtitle.AutoSize = true;
        lblSubtitle.Location = new Point(margin, y);
        lblSubtitle.Text = "SHA-256 directory scanner";
        y += 34;

        lblInput.AutoSize = true;
        lblInput.Location = new Point(margin, y);
        lblInput.Text = "Input folder";
        y += 22;

        txtInput.Location = new Point(margin, y);
        txtInput.Size = new Size(editWidth, rowHeight);
        txtInput.Text = "Client";

        btnBrowseInput.Location = new Point(margin + editWidth + 8, y - 1);
        btnBrowseInput.Size = new Size(browseWidth, rowHeight + 2);
        btnBrowseInput.Text = "Browse...";
        btnBrowseInput.Click += btnBrowseInput_Click;
        y += rowHeight + 12;

        lblOutput.AutoSize = true;
        lblOutput.Location = new Point(margin, y);
        lblOutput.Text = "Output JSON";
        y += 22;

        txtOutput.Location = new Point(margin, y);
        txtOutput.Size = new Size(editWidth, rowHeight);
        txtOutput.Text = "client.json";

        btnBrowseOutput.Location = new Point(margin + editWidth + 8, y - 1);
        btnBrowseOutput.Size = new Size(browseWidth, rowHeight + 2);
        btnBrowseOutput.Text = "Browse...";
        btnBrowseOutput.Click += btnBrowseOutput_Click;
        y += rowHeight + 20;

        lblProgress.AutoSize = true;
        lblProgress.Location = new Point(margin, y);
        lblProgress.Text = "Progress";
        y += 22;

        progressBar.Location = new Point(margin, y);
        progressBar.Size = new Size(editWidth + browseWidth + 8, 22);
        progressBar.Style = ProgressBarStyle.Continuous;
        y += 30;

        lblStatus.AutoSize = true;
        lblStatus.Location = new Point(margin, y);
        lblStatus.Text = "Ready";

        lblCounter.AutoSize = true;
        lblCounter.Location = new Point(margin + 420, y);
        lblCounter.Text = "";
        y += 30;

        btnStart.Location = new Point(margin, y);
        btnStart.Size = new Size(130, 34);
        btnStart.Text = "Start";
        btnStart.Click += btnStart_Click;

        btnCancel.Location = new Point(margin + 140, y);
        btnCancel.Size = new Size(130, 34);
        btnCancel.Text = "Cancel";
        btnCancel.Click += btnCancel_Click;

        btnOpenOutput.Location = new Point(margin + 280, y);
        btnOpenOutput.Size = new Size(130, 34);
        btnOpenOutput.Text = "Open Output";
        btnOpenOutput.Click += btnOpenOutput_Click;
        y += 46;

        lblLog.AutoSize = true;
        lblLog.Location = new Point(margin, y);
        lblLog.Text = "Log";
        y += 22;

        txtLog.Location = new Point(margin, y);
        txtLog.Size = new Size(editWidth + browseWidth + 8, 130);
        txtLog.Multiline = true;
        txtLog.ReadOnly = true;
        txtLog.ScrollBars = ScrollBars.Vertical;
        txtLog.Name = "logPanel";
        txtLog.WordWrap = false;

        Controls.Add(lblTitle);
        Controls.Add(lblSubtitle);
        Controls.Add(lblInput);
        Controls.Add(txtInput);
        Controls.Add(btnBrowseInput);
        Controls.Add(lblOutput);
        Controls.Add(txtOutput);
        Controls.Add(btnBrowseOutput);
        Controls.Add(lblProgress);
        Controls.Add(progressBar);
        Controls.Add(lblStatus);
        Controls.Add(lblCounter);
        Controls.Add(btnStart);
        Controls.Add(btnCancel);
        Controls.Add(btnOpenOutput);
        Controls.Add(lblLog);
        Controls.Add(txtLog);

        ResumeLayout(false);
        PerformLayout();
    }
}
