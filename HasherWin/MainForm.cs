using System.Diagnostics;

namespace HasherWin;

internal sealed partial class MainForm : Form
{
    private CancellationTokenSource? _cts;

    public MainForm()
    {
        InitializeComponent();
        Theme.ApplyDark(this);
        lblTitle.Font = new Font("Segoe UI", 14F, FontStyle.Bold);
        lblSubtitle.ForeColor = Theme.Muted;
        lblCounter.Font = new Font("Segoe UI", 9F, FontStyle.Bold);
        txtLog.BackColor = Theme.LogBackground;
        txtLog.ForeColor = Theme.Muted;
        progressBar.ForeColor = Theme.Accent;
        SetWorkingState(false);
        AppendLog("Ready.");
    }

    private void btnBrowseInput_Click(object? sender, EventArgs e)
    {
        using var dialog = new FolderBrowserDialog
        {
            Description = "Select folder to hash",
            UseDescriptionForTitle = true,
            ShowNewFolderButton = false
        };

        if (!string.IsNullOrWhiteSpace(txtInput.Text) && Directory.Exists(txtInput.Text))
            dialog.InitialDirectory = Path.GetFullPath(txtInput.Text);

        if (dialog.ShowDialog(this) == DialogResult.OK)
            txtInput.Text = dialog.SelectedPath;
    }

    private void btnBrowseOutput_Click(object? sender, EventArgs e)
    {
        using var dialog = new SaveFileDialog
        {
            Title = "Save hash manifest",
            Filter = "JSON files (*.json)|*.json|All files (*.*)|*.*",
            DefaultExt = "json",
            FileName = string.IsNullOrWhiteSpace(txtOutput.Text) ? "client.json" : Path.GetFileName(txtOutput.Text)
        };

        if (!string.IsNullOrWhiteSpace(txtOutput.Text))
        {
            var dir = Path.GetDirectoryName(txtOutput.Text);
            if (!string.IsNullOrEmpty(dir) && Directory.Exists(dir))
                dialog.InitialDirectory = dir;
        }

        if (dialog.ShowDialog(this) == DialogResult.OK)
            txtOutput.Text = dialog.FileName;
    }

    private async void btnStart_Click(object? sender, EventArgs e)
    {
        var inputDir = txtInput.Text.Trim();
        var outputFile = txtOutput.Text.Trim();

        if (string.IsNullOrWhiteSpace(inputDir))
        {
            MessageBox.Show(this, "Please select an input folder.", "Hasher", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        if (!Directory.Exists(inputDir))
        {
            MessageBox.Show(this, "Input folder does not exist.", "Hasher", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return;
        }

        if (string.IsNullOrWhiteSpace(outputFile))
        {
            MessageBox.Show(this, "Please specify an output JSON file.", "Hasher", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        txtLog.Clear();
        AppendLog("Starting scan...");
        progressBar.Value = 0;
        lblStatus.Text = "Preparing...";
        lblCounter.Text = "0 / 0";

        _cts = new CancellationTokenSource();
        SetWorkingState(true);

        try
        {
            var progress = new Progress<(int Current, int Total, string RelativePath)>(OnProgress);
            var token = _cts.Token;

            var files = await Task.Run(() => HashEngine.BuildManifest(inputDir, progress, token), token);

            if (files.Count == 0)
            {
                lblStatus.Text = "No files found in the selected folder.";
                AppendLog(lblStatus.Text);
                MessageBox.Show(this, lblStatus.Text, "Hasher", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            lblStatus.Text = "Writing JSON...";
            await Task.Run(() => HashEngine.WriteJsonManifest(files, outputFile), token);

            progressBar.Value = 100;
            lblStatus.Text = $"Done! {files.Count} files hashed successfully.";
            AppendLog(lblStatus.Text);
            MessageBox.Show(this, lblStatus.Text, "Hasher - Complete", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        catch (OperationCanceledException)
        {
            progressBar.Value = 0;
            lblStatus.Text = "Cancelled.";
            AppendLog(lblStatus.Text);
        }
        catch (Exception ex)
        {
            progressBar.Value = 0;
            lblStatus.Text = ex.Message;
            AppendLog($"Error: {ex.Message}");
            MessageBox.Show(this, ex.Message, "Hasher - Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        finally
        {
            _cts?.Dispose();
            _cts = null;
            SetWorkingState(false);
        }
    }

    private void btnCancel_Click(object? sender, EventArgs e)
    {
        _cts?.Cancel();
        lblStatus.Text = "Cancelling...";
        AppendLog("Cancelling...");
    }

    private void btnOpenOutput_Click(object? sender, EventArgs e)
    {
        var output = txtOutput.Text.Trim();
        if (string.IsNullOrWhiteSpace(output))
            return;

        if (File.Exists(output))
        {
            Process.Start(new ProcessStartInfo(output) { UseShellExecute = true });
            return;
        }

        var dir = Path.GetDirectoryName(output);
        if (!string.IsNullOrEmpty(dir) && Directory.Exists(dir))
            Process.Start(new ProcessStartInfo("explorer.exe", dir) { UseShellExecute = true });
    }

    private void OnProgress((int Current, int Total, string RelativePath) state)
    {
        var percent = state.Total > 0 ? (int)((double)state.Current / state.Total * 100) : 0;
        progressBar.Value = Math.Clamp(percent, 0, 100);
        lblStatus.Text = $"Hashing: {state.RelativePath}";
        lblCounter.Text = $"{state.Current} / {state.Total}";

        if (state.Current == 1 || state.Current == state.Total || state.Current % 10 == 0)
            AppendLog($"[{state.Current}/{state.Total}] {state.RelativePath}");
    }

    private void SetWorkingState(bool working)
    {
        txtInput.Enabled = !working;
        txtOutput.Enabled = !working;
        btnBrowseInput.Enabled = !working;
        btnBrowseOutput.Enabled = !working;
        btnStart.Enabled = !working;
        btnCancel.Enabled = working;
        btnOpenOutput.Enabled = !working;
    }

    private void AppendLog(string line)
    {
        if (txtLog.TextLength > 0)
            txtLog.AppendText(Environment.NewLine);
        txtLog.AppendText(line);
    }
}
