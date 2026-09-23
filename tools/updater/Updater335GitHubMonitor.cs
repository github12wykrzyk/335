using System;
using System.Collections.Generic;
using System.Drawing;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace WoW335Updater
{
    // Read-only Actions monitor: only github12wykrzyk/335, never an artifact selector.
    internal sealed partial class MainForm
    {
        private readonly Timer monitorTimer = new Timer { Interval = 10000 };
        private readonly Button monitorButton = new Button();
        private readonly Dictionary<string, Label> monitorBadges = new Dictionary<string, Label>
        {
            { "work", new Label() }, { "main", new Label() }
        };
        private bool monitorBusy;
        private Form monitorWindow;
        private RichTextBox monitorOutput;
        private string monitorReport = "Nie pobrano jeszcze informacji z GitHub.";

        private TableLayoutPanel Build335MonitorHeader()
        {
            // One horizontal row: two stacked badges were taller than the header on
            // scaled Windows desktops and the configuration card covered MAIN.
            var panel = UiGrid(2, 1);
            panel.ColumnStyles.Clear();
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50f));
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50f));
            panel.RowStyles.Add(new RowStyle(SizeType.Percent, 100f));
            int index = 0;
            foreach (var branch in new[] { "work", "main" })
            {
                var badge = monitorBadges[branch];
                badge.Dock = DockStyle.Fill;
                badge.AutoEllipsis = true;
                badge.Margin = new Padding(4, 9, 4, 9);
                badge.Padding = new Padding(8, 0, 2, 0);
                badge.Font = new Font("Segoe UI", 9f, FontStyle.Bold);
                badge.TextAlign = ContentAlignment.MiddleLeft;
                Set335Badge(branch, "UNKNOWN", "", "Oczekiwanie na pierwsze sprawdzenie.");
                panel.Controls.Add(badge, index++, 0);
            }
            monitorButton.Click += delegate { Open335Monitor(); };
            return panel;
        }

        private void Set335Badge(string branch, string state, string head, string detail)
        {
            var badge = monitorBadges[branch];
            bool green = state == "SUCCESS", yellow = state == "RUNNING" || state == "PENDING", red = state == "FAIL";
            badge.BackColor = green ? Color.FromArgb(27, 83, 65)
                : yellow ? Color.FromArgb(105, 77, 33)
                : red ? Color.FromArgb(107, 40, 50) : Color.FromArgb(44, 62, 79);
            badge.ForeColor = green ? Color.FromArgb(169, 247, 202)
                : yellow ? Color.FromArgb(255, 221, 145)
                : red ? Color.FromArgb(255, 166, 177) : UiMuted;
            badge.Text = branch.ToUpperInvariant() + "  " + state + "  " +
                (string.IsNullOrEmpty(head) ? "HEAD ?" : head.Substring(0, Math.Min(8, head.Length)));
            dashboardTips.SetToolTip(badge, detail + "\nOdczyt: " + DateTime.Now.ToString("HH:mm:ss"));
        }

        private void Start335Monitor()
        {
            if (Array.Exists(Environment.GetCommandLineArgs(), a => a == "--ui-smoke")) return;
            monitorTimer.Tick += async delegate { await Refresh335MonitorAsync(); };
            monitorTimer.Start();
            var pending = Refresh335MonitorAsync();
            FormClosed += delegate
            {
                monitorTimer.Stop();
                monitorTimer.Dispose();
                if (monitorWindow != null && !monitorWindow.IsDisposed) monitorWindow.Close();
            };
        }

        private void Open335Monitor()
        {
            if (monitorWindow != null && !monitorWindow.IsDisposed) { monitorWindow.Activate(); return; }
            var window = new Form { Text = "Monitor GitHub • 335", ClientSize = new Size(760, 480),
                MinimumSize = new Size(560, 340), StartPosition = FormStartPosition.CenterParent,
                BackColor = UiCanvas, ForeColor = UiInk, Font = Font };
            var grid = UiGrid(1, 2);
            grid.Padding = new Padding(12);
            grid.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            grid.RowStyles.Add(new RowStyle(SizeType.Absolute, 43));
            var output = new RichTextBox { Dock = DockStyle.Fill, ReadOnly = true, Text = monitorReport,
                BackColor = UiSurface, ForeColor = UiInk, Font = new Font("Consolas", 9f) };
            var refresh = UiButton(new Button(), "Odśwież teraz");
            refresh.Click += async delegate { await Refresh335MonitorAsync(); };
            grid.Controls.Add(output, 0, 0);
            grid.Controls.Add(refresh, 0, 1);
            window.Controls.Add(grid);
            monitorOutput = output;
            monitorWindow = window;
            window.FormClosed += delegate { monitorOutput = null; monitorWindow = null; };
            window.Show(this);
        }

        private sealed class BadgeData
        {
            public string State;
            public string Head;
            public string Info;
        }

        private BadgeData Read335Badge(string branch, string branchText, string runsText)
        {
            var branchRoot = AsDictionary(json.DeserializeObject(branchText));
            var sha = GetString(AsDictionary(GetValue(branchRoot, "commit")), "sha");
            if (string.IsNullOrWhiteSpace(sha)) throw new InvalidOperationException("Brak HEAD brancha " + branch);
            var runs = AsDictionary(json.DeserializeObject(runsText));
            var seenWorkflows = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            Dictionary<string, object> pending = null, failed = null, success = null;
            foreach (var item in AsArray(GetValue(runs, "workflow_runs")))
            {
                var row = item as Dictionary<string, object>;
                if (row == null || !string.Equals(GetString(row, "head_sha"), sha, StringComparison.OrdinalIgnoreCase)) continue;
                var workflow = GetString(row, "name");
                if (!seenWorkflows.Add(workflow)) continue; // only newest attempt for each workflow
                var state = GetString(row, "status");
                var conclusion = GetString(row, "conclusion");
                if (!string.Equals(state, "completed", StringComparison.OrdinalIgnoreCase))
                {
                    if (pending == null) pending = row;
                }
                else if (string.Equals(conclusion, "success", StringComparison.OrdinalIgnoreCase))
                {
                    if (success == null) success = row;
                }
                else if (failed == null) failed = row;
            }
            var selected = pending ?? failed ?? success;
            if (selected == null) return new BadgeData { State = "UNKNOWN", Head = sha,
                Info = "Brak wyniku Actions dla aktualnego HEAD. Starsze sukcesy są pomijane." };
            var statusValue = GetString(selected, "status");
            var conclusionValue = GetString(selected, "conclusion");
            var display = pending != null ?
                (statusValue == "in_progress" ? "RUNNING" : "PENDING") : failed != null ? "FAIL" : "SUCCESS";
            return new BadgeData { State = display, Head = sha,
                Info = "HEAD " + sha + "\nWorkflow: " + GetString(selected, "name") +
                "\nRun: " + GetLong(selected, "id") +
                "\nStatus: " + statusValue + " / " + conclusionValue +
                "\nStatus CI nie oznacza gotowej paczki." };
        }

        private async Task Refresh335MonitorAsync()
        {
            if (monitorBusy || IsDisposed || Disposing) return;
            monitorBusy = true;
            try
            {
                var output = new StringBuilder();
                output.AppendLine("WOW 335 / GITHUB   " + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"));
                output.AppendLine("Odczyt co 10 sekund, tylko repozytorium github12wykrzyk/335.");
                if (string.IsNullOrWhiteSpace(token.Text))
                {
                    output.AppendLine("Brak tokenu (Contents: Read, Actions: Read).");
                    foreach (var b in monitorBadges.Keys) Set335Badge(b, "UNKNOWN", "", "Brak tokenu GitHub.");
                    monitorReport = output.ToString();
                    return;
                }
                bool hasErrors = false;
                using (var client = CreateClient())
                {
                    client.Timeout = TimeSpan.FromSeconds(18);
                    foreach (var b in new[] { "work", "main" })
                    {
                        try
                        {
                            var branchData = await GetStringAsync(client, ApiRoot + "/branches/" + b);
                            var runsData = await GetStringAsync(client, ApiRoot + "/actions/runs?branch=" + b + "&per_page=40");
                            var badge = Read335Badge(b, branchData, runsData);
                            Set335Badge(b, badge.State, badge.Head, badge.Info);
                            output.AppendLine(b.ToUpperInvariant() + "  " + badge.State + "  " + badge.Head);
                            output.AppendLine(badge.Info);
                        }
                        catch (Exception ex)
                        {
                            hasErrors = true;
                            Set335Badge(b, "UNKNOWN", "", ex.Message);
                            output.AppendLine(b.ToUpperInvariant() + ": błąd odczytu " + ex.Message);
                        }
                        output.AppendLine();
                    }
                    try
                    {
                        var refs = AsArray(json.DeserializeObject(await GetStringAsync(client, ApiRoot + "/branches?per_page=100")));
                        output.AppendLine("FEATURE / PROMOTE • tylko podgląd, bez instalacji:");
                        int count = 0;
                        foreach (var item in refs)
                        {
                            var row = AsDictionary(item);
                            var name = GetString(row, "name");
                            if (!name.StartsWith("feature/", StringComparison.OrdinalIgnoreCase) &&
                                !name.StartsWith("promote/", StringComparison.OrdinalIgnoreCase)) continue;
                            var sha = GetString(AsDictionary(GetValue(row, "commit")), "sha");
                            output.AppendLine("  " + name + "   " + sha.Substring(0, Math.Min(8, sha.Length)));
                            count++;
                        }
                        if (count == 0) output.AppendLine("  Brak.");
                        if (refs.Length >= 100) output.AppendLine("  Uwaga: lista branchy może być ucięta.");
                    }
                    catch (Exception ex)
                    {
                        hasErrors = true;
                        output.AppendLine("Eksperymenty: błąd odczytu " + ex.Message);
                    }
                }
                monitorReport = output.ToString();
                monitorButton.Text = hasErrors ? "GH: błąd" : "GH: " + DateTime.Now.ToString("HH:mm:ss");
                if (string.IsNullOrEmpty(connectionBadge.Text) || connectionBadge.Text != "GitHub: połączono")
                {
                    connectionBadge.Text = hasErrors ? "GitHub: częściowy odczyt" : "GitHub: połączono";
                    connectionBadge.ForeColor = hasErrors ? UiMuted : Color.FromArgb(151, 235, 190);
                }
            }
            catch (Exception ex)
            {
                monitorReport = "Monitor GitHub: " + ex.Message;
                foreach (var b in monitorBadges.Keys) Set335Badge(b, "UNKNOWN", "", monitorReport);
            }
            finally
            {
                if (monitorOutput != null && !monitorOutput.IsDisposed) monitorOutput.Text = monitorReport;
                monitorBusy = false;
            }
        }
    }
}
