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
        private readonly Timer monitorTimer = new Timer { Interval = 20000 };
        private FlowLayoutPanel monitorStrips;
        private bool monitorResizing;
        private string monitorLayoutKey = "";
        private readonly Button monitorButton = new Button();
        private readonly Dictionary<string, Label> monitorBadges = new Dictionary<string, Label>
        {
            { "work", new Label() }, { "main", new Label() }
        };
        private readonly Dictionary<string, Panel> monitorBadgeFrames = new Dictionary<string, Panel>
        {
            { "work", new Panel() }, { "main", new Panel() }
        };
        // Initial placeholders only; after GitHub discovery ALL branch names
        // compete equally for the four visible slots, sorted by live CI state.
        private static readonly string[] FeaturedBranches = {
            "work", "main", "feature/autopickpocket-12340",
            "feature/autopickpocket-packets-12340"
        };
        private readonly Button monitorMoreBranchesButton = new Button();
        private readonly ContextMenuStrip monitorMoreBranches = new ContextMenuStrip();
        private readonly Dictionary<string, string> monitorBadgeStates =
            new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, string> monitorBadgeDetails =
            new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        private readonly List<string> monitorKnownBranches = new List<string>();
        private bool monitorBusy;
        private Form monitorWindow;
        private RichTextBox monitorOutput;
        private string monitorReport = "Nie pobrano jeszcze informacji z GitHub.";

        private FlowLayoutPanel Build335MonitorHeader()
        {
            monitorStrips = new FlowLayoutPanel
            {
                Dock = DockStyle.Fill,
                FlowDirection = FlowDirection.LeftToRight,
                WrapContents = false,
                AutoScroll = true,
                BackColor = Color.Transparent,
                Margin = Padding.Empty,
                Padding = new Padding(0, 1, 0, 0)
            };
            monitorStrips.Resize += delegate { Resize335MonitorBadges(); };
            monitorMoreBranches.BackColor = UiSurface;
            monitorMoreBranches.ForeColor = UiInk;
            monitorMoreBranches.ShowImageMargin = false;
            UiButton(monitorMoreBranchesButton, "Pozostałe (0) ▾");
            monitorMoreBranchesButton.Dock = DockStyle.None;
            monitorMoreBranchesButton.Height = 25;
            monitorMoreBranchesButton.Width = 140;
            monitorMoreBranchesButton.Margin = new Padding(3, 1, 3, 1);
            monitorMoreBranchesButton.ContextMenuStrip = monitorMoreBranches;
            monitorMoreBranchesButton.Click += delegate {
                if (monitorMoreBranches.Items.Count > 0)
                    monitorMoreBranches.Show(monitorMoreBranchesButton,
                        new Point(0, monitorMoreBranchesButton.Height));
            };
            Arrange335MonitorBadges(FeaturedBranches);
            monitorButton.Click += delegate { Open335Monitor(); };
            return monitorStrips;
        }

        private static int Badge335Priority(string state)
        {
            // Unavailable/unknown data is neutral and comes after measured CI states.
            return state == "FAIL" ? 0 : state == "RUNNING" || state == "PENDING" ? 1
                : state == "SUCCESS" ? 2 : 3;
        }

        private int Badge335PriorityFor(string branch)
        {
            string state;
            return Badge335Priority(monitorBadgeStates.TryGetValue(branch, out state) ? state : "UNKNOWN");
        }

        private static string Compact335Status(string state)
        {
            return state == "SUCCESS" ? "OK" : state == "RUNNING" ? "RUN" :
                state == "PENDING" ? "WAIT" : state == "UNKNOWN" ? "?" : state;
        }

        private void Resize335MonitorBadges()
        {
            if (monitorStrips == null || monitorResizing) return;
            monitorResizing = true;
            try
            {
                // Up to four highest-priority branch frames and the 140px menu
                // fit without scrolling at the supported 960px minimum width.
                int count = 4;
                int available = Math.Max(1, monitorStrips.ClientSize.Width -
                    monitorStrips.Padding.Horizontal - monitorMoreBranchesButton.Width);
                int width = Math.Max(138, Math.Min(218, (available - (count + 1) * 6 - 8) / count));
                monitorStrips.SuspendLayout();
                foreach (Control item in monitorStrips.Controls)
                    if (item != monitorMoreBranchesButton && item.Width != width) item.Width = width;
                monitorStrips.ResumeLayout(true);
            }
            finally { monitorResizing = false; }
        }

        private static int Badge335TieBreak(string branch)
        {
            int index = Array.IndexOf(FeaturedBranches, branch);
            return index < 0 ? FeaturedBranches.Length : index;
        }

        private static string Compact335Name(string branch)
        {
            return branch == "feature/autopickpocket-12340" ? "PP natywny" :
                branch == "feature/autopickpocket-packets-12340" ? "PP pakiety" :
                branch.Replace("feature/", "").Replace("promote/", "p/");
        }

        private void Arrange335MonitorBadges(IList<string> branches)
        {
            if (monitorStrips == null) return;
            var known = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            if (branches != null)
                foreach (var name in branches)
                    if (ValidGameBranch(name)) known.Add(name);
            monitorKnownBranches.Clear();
            monitorKnownBranches.AddRange(known);
            foreach (var name in monitorKnownBranches)
            {
                if (!monitorBadges.ContainsKey(name))
                {
                    monitorBadges[name] = new Label();
                    monitorBadgeFrames[name] = new Panel();
                }
                if (!monitorBadgeStates.ContainsKey(name))
                    Set335Badge(name, "UNKNOWN", "", "Oczekiwanie na bieżący HEAD.");
            }

            // Globally sort every discovered branch BEFORE choosing four visible
            // badges: newly created red/yellow feature/* displace green badges.
            var ordered = new List<string>(monitorKnownBranches);
            ordered.Sort(delegate(string a, string b) {
                int cmp = Badge335PriorityFor(a).CompareTo(Badge335PriorityFor(b));
                if (cmp != 0) return cmp;
                cmp = Badge335TieBreak(a).CompareTo(Badge335TieBreak(b));
                return cmp != 0 ? cmp : StringComparer.OrdinalIgnoreCase.Compare(a, b);
            });
            int visibleCount = Math.Min(4, ordered.Count);
            var visible = ordered.GetRange(0, visibleCount);
            var remaining = ordered.GetRange(visibleCount, ordered.Count - visibleCount);

            var layoutKey = string.Join("|", visible);
            if (layoutKey != monitorLayoutKey)
            {
                monitorLayoutKey = layoutKey;
                monitorStrips.SuspendLayout();
                try
                {
                    monitorStrips.Controls.Clear();
                    foreach (var name in visible)
                    {
                        var badge = monitorBadges[name];
                        var outline = monitorBadgeFrames[name];
                        badge.Dock = DockStyle.Fill;
                        badge.AutoEllipsis = true;
                        badge.Margin = Padding.Empty;
                        badge.Padding = new Padding(5, 0, 3, 0);
                        badge.Font = new Font("Segoe UI", 8.25f, FontStyle.Regular);
                        badge.TextAlign = ContentAlignment.MiddleLeft;
                        outline.Height = 25;
                        outline.Margin = new Padding(3, 1, 3, 1);
                        outline.Padding = new Padding(1);
                        if (!outline.Controls.Contains(badge)) outline.Controls.Add(badge);
                        monitorStrips.Controls.Add(outline);
                    }
                    monitorStrips.Controls.Add(monitorMoreBranchesButton);
                }
                finally { monitorStrips.ResumeLayout(true); }
                Resize335MonitorBadges();
            }
            monitorMoreBranches.Items.Clear();
            foreach (var name in remaining)
            {
                string state;
                if (!monitorBadgeStates.TryGetValue(name, out state)) state = "UNKNOWN";
                var item = new ToolStripMenuItem(name + "  •  " + Compact335Status(state));
                item.Tag = name;
                item.ForeColor = state == "FAIL" ? Color.FromArgb(255, 178, 188) :
                    state == "RUNNING" || state == "PENDING" ? Color.FromArgb(255, 221, 153) :
                    state == "SUCCESS" ? Color.FromArgb(183, 241, 231) : UiMuted;
                string detail;
                item.ToolTipText = monitorBadgeDetails.TryGetValue(name, out detail) ? detail : name;
                item.Click += delegate { Open335Monitor(); };
                monitorMoreBranches.Items.Add(item);
            }
            monitorMoreBranchesButton.Text = "Pozostałe (" + remaining.Count + ") ▾";
            monitorMoreBranchesButton.Enabled = remaining.Count > 0;
            dashboardTips.SetToolTip(monitorMoreBranchesButton,
                "Pozostałe branche GitHub; wybór brancha gry pozostaje w Konfiguracji.");
        }

        private void Set335Badge(string branch, string state, string head, string detail)
        {
            monitorBadgeStates[branch] = state;
            monitorBadgeDetails[branch] = detail;
            var badge = monitorBadges[branch];
            var outline = monitorBadgeFrames[branch];
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
            var compactName = Compact335Name(branch);
            var compactState = Compact335Status(state);
            badge.Text = compactName + "  •  " + compactState;
            var tooltip = detail + "\nOdczyt: " + DateTime.Now.ToString("HH:mm:ss");
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
                monitorMoreBranches.Dispose();
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
                output.AppendLine("Odczyt co 20 sekund, tylko github12wykrzyk/335.");
                if (string.IsNullOrWhiteSpace(token.Text))
                {
                    output.AppendLine("Brak tokenu (Contents: Read, Actions: Read).");
                    foreach (var b in new List<string>(monitorKnownBranches))
                        Set335Badge(b, "UNKNOWN", "", "Brak tokenu GitHub.");
                    Arrange335MonitorBadges(new List<string>(monitorKnownBranches));
                    monitorReport = output.ToString();
                    return;
                }
                bool hasErrors = false;
                using (var client = CreateClient())
                {
                    client.Timeout = TimeSpan.FromSeconds(18);
                    var rows = new Dictionary<string, Dictionary<string, object>>(StringComparer.OrdinalIgnoreCase);
                    try
                    {
                        // GitHub paginates at 100 items. Load subsequent pages
                        // instead of silently hiding new branches from the bar.
                        for (var page = 1; page <= 100; page++)
                        {
                            var refs = AsArray(json.DeserializeObject(await GetStringAsync(
                                client, ApiRoot + "/branches?per_page=100&page=" + page)));
                            foreach (var item in refs)
                            {
                                var row = AsDictionary(item);
                                var name = GetString(row, "name");
                                if (ValidGameBranch(name) && !rows.ContainsKey(name)) rows[name] = row;
                            }
                            if (refs.Length < 100) break;
                            if (page == 100)
                                throw new InvalidOperationException("Repo ma ponad 10000 branchy; lista niekompletna.");
                        }
                    }
                    catch (Exception ex)
                    {
                        hasErrors = true;
                        output.AppendLine("Lista branchy: " + ex.Message);
                    }
                    if (rows.Count == 0 && hasErrors)
                        throw new InvalidOperationException("Nie udało się odczytać listy branchy.");
                    var branches = new List<string>(rows.Keys);
                    branches.Sort(StringComparer.OrdinalIgnoreCase);
                    Arrange335MonitorBadges(branches);
                    foreach (var b in branches)
                    {
                        try
                        {
                            Dictionary<string, object> row;
                            if (!rows.TryGetValue(b, out row))
                                throw new InvalidOperationException("Branch nie został zwrócony przez GitHub.");
                            var runsData = await GetStringAsync(client, ApiRoot +
                                "/actions/runs?branch=" + Uri.EscapeDataString(b) + "&per_page=40");
                            var badge = Read335Badge(b, json.Serialize(row), runsData);
                            if (monitorBadges.ContainsKey(b)) Set335Badge(b, badge.State, badge.Head, badge.Info);
                            output.AppendLine(b + "  " + badge.State + "  " + badge.Head);
                            output.AppendLine(badge.Info);
                        }
                        catch (Exception ex)
                        {
                            hasErrors = true;
                            if (monitorBadges.ContainsKey(b)) Set335Badge(b, "UNKNOWN", "", ex.Message);
                            output.AppendLine(b + ": błąd odczytu " + ex.Message);
                        }
                        output.AppendLine();
                    }
                }
                Arrange335MonitorBadges(new List<string>(monitorKnownBranches)); // globally sort all current-HEAD states
                monitorReport = output.ToString();
                monitorButton.Text = hasErrors ? "GH: błąd" : "GH: " + DateTime.Now.ToString("HH:mm:ss");
                connectionBadge.Text = hasErrors ? "GitHub: częściowy odczyt" : "GitHub: połączono";
                connectionBadge.ForeColor = hasErrors ? UiMuted : Color.FromArgb(151, 235, 190);
            }
            catch (Exception ex)
            {
                monitorReport = "Monitor GitHub: " + ex.Message;
                foreach (var b in new List<string>(monitorKnownBranches))
                    Set335Badge(b, "UNKNOWN", "", monitorReport);
                Arrange335MonitorBadges(new List<string>(monitorKnownBranches));
            }
            finally
            {
                if (monitorOutput != null && !monitorOutput.IsDisposed) monitorOutput.Text = monitorReport;
                monitorBusy = false;
            }
        }

    }
}
