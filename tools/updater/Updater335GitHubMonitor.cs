using System;
using System.Collections.Generic;
using System.Linq;
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
        private readonly Dictionary<string, Label> monitorBadges =
            new Dictionary<string, Label>(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, Panel> monitorBadgeFrames =
            new Dictionary<string, Panel>(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, string> monitorHeads =
            new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        private readonly FlowLayoutPanel monitorLayout = new FlowLayoutPanel();
        private RowStyle monitorStripRow;
        private const int MonitorBadgeHeight = 38;
        private DateTime monitorBranchesFetchedUtc = DateTime.MinValue;
        private bool monitorBranchListTruncated;
        private bool monitorBusy;
        private Form monitorWindow;
        private RichTextBox monitorOutput;
        private string monitorReport = "Nie pobrano jeszcze informacji z GitHub.";

        private void Bind335MonitorRow(RowStyle row)
        {
            monitorStripRow = row;
        }

        private Control Build335MonitorHeader()
        {
            // All discovered branches live in one full-width ribbon. When there
            // are more branches than fit horizontally, expand the ribbon vertically.
            monitorLayout.Dock = DockStyle.Fill;
            monitorLayout.AutoScroll = false;
            monitorLayout.WrapContents = true;
            monitorLayout.FlowDirection = FlowDirection.LeftToRight;
            monitorLayout.Margin = Padding.Empty;
            monitorLayout.Padding = new Padding(4);
            monitorLayout.BackColor = UiSurface;
            monitorLayout.Resize += delegate { Resize335MonitorBadges(); };
            Rebuild335MonitorBadges(new[] { "work", "main" });
            return monitorLayout;
        }

        private static string Compact335Branch(string branch)
        {
            if (branch.StartsWith("feature/", StringComparison.OrdinalIgnoreCase))
                return branch.Substring("feature/".Length);
            if (branch.StartsWith("promote/", StringComparison.OrdinalIgnoreCase))
                return "P/" + branch.Substring("promote/".Length);
            return branch;
        }

        private void Resize335MonitorBadges()
        {
            int available = monitorLayout.ClientSize.Width - monitorLayout.Padding.Horizontal;
            if (available <= 0 || monitorBadgeFrames.Count == 0) return;
            // At the standard 1060 px window, all five current branches fit
            // in one row; future branches wrap without a scrollbar.
            int columns = Math.Max(1, Math.Min(monitorBadgeFrames.Count, available / 185));
            int slot = available / columns;
            int rows = (monitorBadgeFrames.Count + columns - 1) / columns;
            int height = rows * (MonitorBadgeHeight + 2) + monitorLayout.Padding.Vertical + 2;
            if (monitorStripRow != null && monitorStripRow.Height != height)
                monitorStripRow.Height = height;
            foreach (var panel in monitorBadgeFrames.Values)
            {
                int panelWidth = Math.Max(80, slot - panel.Margin.Horizontal);
                if (panel.Width != panelWidth) panel.Width = panelWidth;
            }
        }

        private void Rebuild335MonitorBadges(IList<string> branches)
        {
            if (branches.SequenceEqual(monitorBadges.Keys, StringComparer.OrdinalIgnoreCase))
                return;
            monitorLayout.SuspendLayout();
            try
            {
                monitorLayout.Controls.Clear();
                monitorBadges.Clear();
                monitorBadgeFrames.Clear();
                foreach (var branch in branches)
                {
                    var badge = new Label {
                        Dock = DockStyle.Fill, AutoEllipsis = true,
                        Margin = Padding.Empty, Padding = new Padding(5, 0, 2, 0),
                        Font = new Font("Segoe UI", 8f, FontStyle.Regular),
                        TextAlign = ContentAlignment.MiddleLeft
                    };
                    var outline = new Panel {
                        Height = MonitorBadgeHeight, Width = 175, Margin = new Padding(2, 1, 2, 1),
                        Padding = new Padding(1), Cursor = Cursors.Hand
                    };
                    outline.Controls.Add(badge);
                    // The badge opens a detailed, scrollable report for long names.
                    badge.Cursor = Cursors.Hand;
                    badge.Click += delegate { Open335Monitor(); };
                    outline.Click += delegate { Open335Monitor(); };
                    monitorBadges.Add(branch, badge);
                    monitorBadgeFrames.Add(branch, outline);
                    monitorLayout.Controls.Add(outline);
                    Set335Badge(branch, "UNKNOWN", "", "Oczekiwanie na GitHub.");
                }
                Resize335MonitorBadges();
            }
            finally { monitorLayout.ResumeLayout(true); }
        }

        private void Set335Badge(string branch, string state, string head, string detail)
        {
            Label badge;
            Panel outline;
            if (!monitorBadges.TryGetValue(branch, out badge) ||
                !monitorBadgeFrames.TryGetValue(branch, out outline)) return;
            bool green = state == "SUCCESS", yellow = state == "RUNNING" || state == "PENDING", red = state == "FAIL";
            outline.BackColor = green ? Color.FromArgb(52, 194, 199)
                : yellow ? Color.FromArgb(191, 153, 74)
                : red ? Color.FromArgb(209, 97, 111) : Color.FromArgb(69, 96, 116);
            badge.BackColor = green ? Color.FromArgb(19, 52, 66)
                : yellow ? Color.FromArgb(56, 49, 41)
                : red ? Color.FromArgb(58, 39, 51) : Color.FromArgb(33, 51, 69);
            badge.ForeColor = green ? Color.FromArgb(183, 241, 231)
                : yellow ? Color.FromArgb(255, 221, 153)
                : red ? Color.FromArgb(255, 178, 188) : UiMuted;
            var shortHead = string.IsNullOrEmpty(head) ? "HEAD ?" : head.Substring(0, Math.Min(8, head.Length));
            badge.Text = Compact335Branch(branch).ToUpperInvariant() +
                "\n" + shortHead + "  |  " + state;
            var tooltip = "Branch: " + branch + "\n" + detail +
                "\nOdczyt: " + DateTime.Now.ToString("HH:mm:ss") +
                "\nKliknij, aby otworzyć pełny monitor.";
            dashboardTips.SetToolTip(outline, tooltip);
            dashboardTips.SetToolTip(badge, tooltip);
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

        private BadgeData Read335Badge(string branch, string sha, object[] runs)
        {
            if (!UpdaterSafety.IsGitCommitSha(sha))
                return new BadgeData { State = "UNKNOWN", Head = "",
                    Info = "Nieprawidłowy HEAD gałęzi " + branch + "." };
            var seenWorkflows = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            Dictionary<string, object> pending = null, failed = null, success = null;
            foreach (var item in runs)
            {
                var row = item as Dictionary<string, object>;
                if (row == null ||
                    !string.Equals(GetString(row, "head_branch"), branch, StringComparison.OrdinalIgnoreCase) ||
                    !string.Equals(GetString(row, "head_sha"), sha, StringComparison.OrdinalIgnoreCase)) continue;
                var workflow = GetString(row, "name");
                if (!seenWorkflows.Add(workflow)) continue; // newest attempt for each workflow
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
                Info = "Nie znaleziono wyniku dla aktualnego HEAD w ostatnich 300 runach. " +
                    "Nie utożsamiaj starego sukcesu z bieżącym HEAD." };
            var statusValue = GetString(selected, "status");
            var conclusionValue = GetString(selected, "conclusion");
            var display = pending != null ?
                (statusValue == "in_progress" ? "RUNNING" : "PENDING") : failed != null ? "FAIL" : "SUCCESS";
            return new BadgeData { State = display, Head = sha,
                Info = "HEAD " + sha + "\nWorkflow: " + GetString(selected, "name") +
                "\nRun: " + GetLong(selected, "id") +
                "\nStatus: " + statusValue + " / " + conclusionValue +
                "\nStatus CI nie oznacza gotowej paczki ani uprawnienia do instalacji." };
        }

        private async Task Refresh335BranchListAsync(HttpClient client)
        {
            if (monitorHeads.Count != 0 &&
                DateTime.UtcNow - monitorBranchesFetchedUtc < TimeSpan.FromSeconds(60)) return;
            var fetched = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            bool truncated = true;
            // GitHub paginates at 100 branches. Discover more pages instead of
            // hard-coding known branches. A bound protects the UI and API quota.
            for (int page = 1; page <= 10; ++page)
            {
                var entries = AsArray(json.DeserializeObject(
                    await GetStringAsync(client, ApiRoot + "/branches?per_page=100&page=" + page)));
                foreach (var item in entries)
                {
                    var row = item as Dictionary<string, object>;
                    if (row == null) continue;
                    var branch = GetString(row, "name");
                    var sha = GetString(AsDictionary(GetValue(row, "commit")), "sha");
                    if (string.IsNullOrWhiteSpace(branch) ||
                        !UpdaterSafety.IsGitCommitSha(sha) || fetched.ContainsKey(branch)) continue;
                    fetched.Add(branch, sha);
                }
                if (entries.Length < 100) { truncated = false; break; }
            }
            if (fetched.Count == 0)
                throw new InvalidOperationException("GitHub nie zwrócił listy branchy.");
            var names = fetched.Keys.OrderBy(n => n == "work" ? 0 : n == "main" ? 1 : 2)
                .ThenBy(n => n, StringComparer.OrdinalIgnoreCase).ToArray();
            monitorHeads.Clear();
            foreach (var branch in names) monitorHeads.Add(branch, fetched[branch]);
            monitorBranchListTruncated = truncated;
            monitorBranchesFetchedUtc = DateTime.UtcNow;
            Rebuild335MonitorBadges(names);
        }

        private async Task<object[]> Recent335RunsAsync(HttpClient client)
        {
            var result = new List<object>();
            for (int page = 1; page <= 3; ++page)
            {
                var response = AsDictionary(json.DeserializeObject(
                    await GetStringAsync(client, ApiRoot + "/actions/runs?per_page=100&page=" + page)));
                var rows = AsArray(GetValue(response, "workflow_runs"));
                result.AddRange(rows);
                if (rows.Length < 100) break;
            }
            return result.ToArray();
        }

        private async Task Refresh335MonitorAsync()
        {
            if (monitorBusy || IsDisposed || Disposing) return;
            monitorBusy = true;
            try
            {
                var output = new StringBuilder();
                output.AppendLine("WOW 335 / GITHUB   " + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"));
                output.AppendLine("Lista branchy: GitHub co 60 s; Actions: co 10 s. Podgląd tylko z github12wykrzyk/335.");
                output.AppendLine("Status CI nie zezwala automatycznie na instalację eksperymentalnych DLL.");
                if (string.IsNullOrWhiteSpace(token.Text))
                {
                    output.AppendLine("Brak tokenu (Contents: Read, Actions: Read).");
                    foreach (var branch in monitorBadges.Keys)
                        Set335Badge(branch, "UNKNOWN", "", "Brak tokenu GitHub.");
                    monitorReport = output.ToString();
                    return;
                }
                using (var client = CreateClient())
                {
                    client.Timeout = TimeSpan.FromSeconds(18);
                    // A branch lookup failure must not silently hide old badges.
                    await Refresh335BranchListAsync(client);
                    var runs = await Recent335RunsAsync(client);
                    output.AppendLine("Odkryte branche: " + monitorHeads.Count +
                        (monitorBranchListTruncated ? " (ponad 1000, lista niepełna)" : ""));
                    output.AppendLine();
                    foreach (var entry in monitorHeads)
                    {
                        var badge = Read335Badge(entry.Key, entry.Value, runs);
                        Set335Badge(entry.Key, badge.State, badge.Head, badge.Info);
                        output.AppendLine(entry.Key + "   " + badge.State + "   " + entry.Value);
                        output.AppendLine(badge.Info);
                        output.AppendLine();
                    }
                }
                monitorReport = output.ToString();
                monitorButton.Text = "GH: " + DateTime.Now.ToString("HH:mm:ss");
                connectionBadge.Text = "GitHub: połączono • " + monitorHeads.Count + " branchy";
                connectionBadge.ForeColor = Color.FromArgb(151, 235, 190);
            }
            catch (Exception ex)
            {
                monitorReport = "Monitor GitHub: błąd odczytu: " + ex.Message +
                    "\nDotychczasowe branche zachowano, status niepotwierdzony.";
                foreach (var branch in monitorBadges.Keys)
                    Set335Badge(branch, "UNKNOWN", "", monitorReport);
                connectionBadge.Text = "GitHub: błąd odczytu branchy/Actions";
                connectionBadge.ForeColor = UiMuted;
            }
            finally
            {
                if (monitorOutput != null && !monitorOutput.IsDisposed)
                    monitorOutput.Text = monitorReport;
                monitorBusy = false;
            }
        }
    }
}
