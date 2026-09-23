using System;
using System.Collections;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace WoW335Updater
{
    internal static class IssueReportFeature
    {
        public static void Attach(Form form)
        {
            if (form == null) return;
            new Controller(form).Attach();
        }

        private sealed class Controller
        {
            private const string Owner = "github12wykrzyk";
            private const string Repo = "335";
            private const string ApiRoot = "https://api.github.com/repos/" + Owner + "/" + Repo;
            private const int AddedHeight = 50;
            private readonly Form form;
            private readonly TextBox gameDir;
            private readonly RichTextBox log;
            private readonly Label status;
            private readonly Button sendButton = new Button();
            private readonly Button autoLootDiagSendButton = new Button();
            private readonly Button tokenButton = new Button();
            private readonly JavaScriptSerializer json = new JavaScriptSerializer();
            private bool busy;

            public Controller(Form form)
            {
                this.form = form;
                gameDir = GetPrivateField<TextBox>(form, "gameDir");
                log = GetPrivateField<RichTextBox>(form, "log");
                status = GetPrivateField<Label>(form, "status");
            }

            public void Attach()
            {
                if (gameDir == null) return;
                var oldHeight = form.ClientSize.Height;
                form.ClientSize = new Size(form.ClientSize.Width, oldHeight + AddedHeight);
                form.MinimumSize = new Size(form.MinimumSize.Width, form.MinimumSize.Height + AddedHeight);

                sendButton.Text = "WYŚLIJ RAPORT";
                sendButton.SetBounds(20, oldHeight + 8, 210, 32);
                sendButton.Font = new Font("Segoe UI Semibold", 9F);
                sendButton.Click += async delegate { await SendReportAsync(false); };
                autoLootDiagSendButton.Text = "WYŚLIJ LOG AUTOLOOT";
                autoLootDiagSendButton.Click += async delegate { await SendReportAsync(true); };
                form.Controls.Add(autoLootDiagSendButton);
                form.Controls.Add(sendButton);

                tokenButton.Text = "TOKEN RAPORTU";
                tokenButton.SetBounds(240, oldHeight + 8, 145, 32);
                tokenButton.Click += delegate { ChangeReportToken(); };
                form.Controls.Add(tokenButton);

                var note = new Label
                {
                    Text = "GitHub Issues R/W, osobny token DPAPI; raport sanityzowany.",
                    AutoSize = true,
                    Left = 400,
                    Top = oldHeight + 16,
                    ForeColor = Color.DimGray
                };
                form.Controls.Add(note);
            }

            private async Task SendReportAsync(bool onlyAutoLoot)
            {
                string finalStatus = "Gotowy";
                try
                {
                    if (busy) return;
                    var root = gameDir.Text.Trim();
                    if (string.IsNullOrWhiteSpace(root) || !Directory.Exists(root))
                        throw new InvalidOperationException("Wybierz istniejący katalog gry.");
                    root = Path.GetFullPath(root);
                    string autoLootLog = "";
                    if (onlyAutoLoot)
                    {
                        autoLootLog = AutoLootDiagSupport.SanitizeLog(
                            AutoLootDiagSupport.CollectLog(root));
                        if (string.IsNullOrWhiteSpace(autoLootLog))
                        {
                            var detail = AutoLootDiagSupport.ExplainMissingLog(root);
                            Log("AutoLoot DIAG: brak rozpoznanych zdarzeń; szczegóły i ścieżki pokazano tylko lokalnie.");
                            var choose = MessageBox.Show(form,
                                detail + "\n\nWskazać zapisany plik AutoLoot DIAG z dysku?",
                                "WoW335 — lokalizacja logu AutoLoot", MessageBoxButtons.YesNo,
                                MessageBoxIcon.Information);
                            if (choose != DialogResult.Yes)
                            {
                                finalStatus = "Nie odnaleziono logu AutoLoot; sprawdź instalację i lokalizację.";
                                return;
                            }
                            using (var picker = new OpenFileDialog())
                            {
                                picker.Title = "Wskaż zapisany log WoW335AutoLootDiag.lua";
                                picker.Filter = "Zapis AutoLoot (*.lua;*.lua.bak)|*.lua;*.lua.bak|Wszystkie pliki (*.*)|*.*";
                                var accountRoot = Path.Combine(root, "WTF", "Account");
                                picker.InitialDirectory = Directory.Exists(accountRoot) ? accountRoot : root;
                                picker.FileName = "WoW335AutoLootDiag.lua";
                                picker.CheckFileExists = true;
                                picker.Multiselect = false;
                                if (picker.ShowDialog(form) != DialogResult.OK)
                                {
                                    finalStatus = "Wskazywanie pliku AutoLoot anulowane.";
                                    return;
                                }
                                autoLootLog = AutoLootDiagSupport.SanitizeLog(
                                    AutoLootDiagSupport.CollectLogFile(picker.FileName));
                                if (string.IsNullOrWhiteSpace(autoLootLog))
                                    throw new InvalidOperationException(
                                        "Wybrany plik nie zawiera poprawnych zdarzeń AutoLoot. " +
                                        "W grze sprawdź /al335 log i zapisz sesję przez /reload.");
                                Log("AutoLoot DIAG: odczytano zdarzenia ze wskazanego pliku; surowe dane nie będą wysłane.");
                            }
                        }
                        if (!ConfirmAutoLootReport(autoLootLog))
                        {
                            finalStatus = "Wysyłanie logu AutoLoot anulowane.";
                            return;
                        }
                    }

                    var reportToken = LoadReportToken();
                    if (string.IsNullOrWhiteSpace(reportToken))
                    {
                        reportToken = PromptForToken();
                        if (string.IsNullOrWhiteSpace(reportToken))
                        {
                            finalStatus = "Wysyłanie raportu anulowane.";
                            return;
                        }
                        SaveReportToken(reportToken);
                    }

                    SetBusy(true, "Tworzenie raportu diagnostycznego...");
                    string headSha;
                    long runId;
                    var bodyCore = onlyAutoLoot
                        ? BuildAutoLootReport(autoLootLog, out headSha, out runId)
                        : BuildReportBody(root, out headSha, out runId);
                    var signatureSeed = onlyAutoLoot
                        ? "AUTOLOOT335-DIAG-V1\n" + headSha + "\n" + autoLootLog
                        : BuildSignatureSeed(root, headSha, runId);
                    var signature = Sha256Text(signatureSeed).Substring(0, 12);
                    var marker = "[diag:" + signature + "]";
                    var title = onlyAutoLoot
                        ? "[AUTOLOOT-DIAG 335] " + ShortSha(headSha) + " " + marker
                        : "[AUTO-DIAG] " + ShortSha(headSha) + " run " + runId + " " + marker;
                    var body = bodyCore + "\n\n---\nDiagnostic signature: `" + signature + "`\nGenerated by WoW335Updater " + UpdaterBuildInfo.Version + ".";

                    using (var client = CreateClient(reportToken))
                    {
                        var existing = await FindExistingIssueAsync(client, marker);
                        if (existing > 0)
                        {
                            finalStatus = "Raport już istnieje jako GitHub Issue #" + existing + ".";
                            Log(finalStatus);
                            MessageBox.Show(form, finalStatus, "WoW335 Updater", MessageBoxButtons.OK, MessageBoxIcon.Information);
                            return;
                        }

                        var payload = new Dictionary<string, object>();
                        payload["title"] = title;
                        payload["body"] = body;
                        using (var request = new HttpRequestMessage(HttpMethod.Post, ApiRoot + "/issues"))
                        {
                            request.Content = new StringContent(json.Serialize(payload), Encoding.UTF8, "application/json");
                            using (var response = await client.SendAsync(request))
                            {
                                var text = await response.Content.ReadAsStringAsync();
                                if (!response.IsSuccessStatusCode)
                                {
                                    HandleAuthenticationFailure(response.StatusCode);
                                    throw new InvalidOperationException("GitHub Issues HTTP " + (int)response.StatusCode + ": " + TrimForError(text) + "\n\nToken raportowy musi mieć Issues: Read and write tylko dla repo 335.");
                                }
                                var created = AsDictionary(json.DeserializeObject(text));
                                var number = GetLong(created, "number");
                                finalStatus = "Raport wysłany jako GitHub Issue #" + number + ".";
                                Log(finalStatus);
                                MessageBox.Show(form, finalStatus, "WoW335 Updater", MessageBoxButtons.OK, MessageBoxIcon.Information);
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    finalStatus = "Wysyłanie raportu nie powiodło się";
                    Log("BŁĄD raportu GitHub: " + ex.Message);
                    MessageBox.Show(form, ex.Message, "WoW335 Updater", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
                finally
                {
                    SetBusy(false, finalStatus);
                }
            }

            private void ChangeReportToken()
            {
                try
                {
                    var value = PromptForToken();
                    if (string.IsNullOrWhiteSpace(value)) return;
                    SaveReportToken(value);
                    Log("Token raportowy został zastąpiony i zapisany przez DPAPI.");
                    MessageBox.Show(form, "Token raportowy zapisany.", "WoW335 Updater", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
                catch (Exception ex)
                {
                    Log("BŁĄD zapisu tokenu raportowego: " + ex.Message);
                    MessageBox.Show(form, ex.Message, "WoW335 Updater", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }

            private string BuildReportBody(string root, out string headSha, out long runId)
            {
                headSha = string.Empty;
                runId = 0;
                var sb = new StringBuilder();
                sb.AppendLine("## WoW335 automatic diagnostic report");
                sb.AppendLine();
                sb.AppendLine("Target: World of Warcraft 3.3.5a build 12340, Windows x86");
                sb.AppendLine("Updater: " + UpdaterBuildInfo.Version);
                sb.AppendLine("Generated UTC: " + DateTime.UtcNow.ToString("o"));

                var installedPath = Path.Combine(root, ".wow335_updater", "installed.json");
                Dictionary<string, object> installed = null;
                if (File.Exists(installedPath))
                {
                    installed = AsDictionary(json.DeserializeObject(File.ReadAllText(installedPath, Encoding.UTF8)));
                    headSha = GetString(installed, "head_sha");
                    runId = GetLong(installed, "run_id");
                    sb.AppendLine("Channel: " + GetString(installed, "channel"));
                    sb.AppendLine("Run ID: " + runId);
                    sb.AppendLine("Head SHA: `" + headSha + "`");
                    sb.AppendLine("Artifact: " + GetString(installed, "artifact_name"));
                    sb.AppendLine("Installed UTC: " + GetString(installed, "installed_utc"));
                    sb.AppendLine("Integrity: " + GetString(installed, "integrity_status"));
                }
                else
                {
                    sb.AppendLine("Installed-state metadata: missing");
                }

                var dlls = Path.Combine(root, "dlls.txt");
                if (File.Exists(dlls))
                {
                    sb.AppendLine();
                    sb.AppendLine("### dlls.txt");
                    sb.AppendLine("```text");
                    sb.AppendLine(Sanitize(File.ReadAllText(dlls, Encoding.ASCII), root, 12000));
                    sb.AppendLine("```");
                }

                sb.AppendLine();
                sb.AppendLine("### WoWDiagHub recent logs");
                var files = GetRecentDiagFiles(root);
                if (files.Length == 0)
                {
                    sb.AppendLine("No WoWDiagHub JSONL diagnostic files found.");
                }
                else
                {
                    foreach (var file in files)
                    {
                        sb.AppendLine();
                        sb.AppendLine("#### " + Path.GetFileName(file));
                        sb.AppendLine("```json");
                        sb.AppendLine(Sanitize(TailFile(file, 12000), root, 12000));
                        sb.AppendLine("```");
                    }
                }

                var autoLootEvents = AutoLootDiagSupport.SanitizeLog(
                    AutoLootDiagSupport.CollectLog(root));
                if (!string.IsNullOrWhiteSpace(autoLootEvents))
                {
                    sb.AppendLine();
                    sb.AppendLine("### AutoLoot 12340 addon diagnostic (SavedVariables; curated event lines)");
                    sb.AppendLine("```text");
                    sb.AppendLine(autoLootEvents);
                    sb.AppendLine("```");
                }

                if (log != null && !string.IsNullOrWhiteSpace(log.Text))
                {
                    sb.AppendLine();
                    sb.AppendLine("### Updater session log (tail)");
                    sb.AppendLine("```text");
                    sb.AppendLine(Sanitize(TailText(log.Text, 10000), root, 10000));
                    sb.AppendLine("```");
                }
                return sb.ToString();
            }

            private string BuildSignatureSeed(string root, string headSha, long runId)
            {
                var sb = new StringBuilder();
                sb.AppendLine("W112-DIAG-SIGNATURE-V2");
                sb.AppendLine(headSha ?? string.Empty);
                sb.AppendLine(runId.ToString());

                var dlls = Path.Combine(root, "dlls.txt");
                if (File.Exists(dlls))
                    sb.AppendLine(Sanitize(File.ReadAllText(dlls, Encoding.ASCII), root, 12000));
                else
                    sb.AppendLine("<no-dlls>");

                var files = GetRecentDiagFiles(root);
                if (files.Length == 0)
                {
                    sb.AppendLine("<no-diag-files>");
                }
                else
                {
                    foreach (var file in files)
                    {
                        sb.AppendLine(Path.GetFileName(file));
                        sb.AppendLine(Sanitize(TailFile(file, 12000), root, 12000));
                    }
                }
                sb.AppendLine("AUTOLOOT_DIAG_LOG:");
                sb.AppendLine(AutoLootDiagSupport.SanitizeLog(AutoLootDiagSupport.CollectLog(root)));
                return sb.ToString();
            }

            private string BuildAutoLootReport(string autoLootLog, out string headSha, out long runId)
            {
                runId = 0;
                headSha = AutoLootDiagSupport.EmbeddedCommit(Assembly.GetExecutingAssembly());
                var sb = new StringBuilder();
                sb.AppendLine("## WoW335 AutoLoot diagnostic addon report (not native AutoLoot)");
                sb.AppendLine("Game: WoW 3.3.5a build 12340 / Windows x86");
                sb.AppendLine("Diagnostic addon build SHA: `" + headSha + "`");
                sb.AppendLine("Updater: " + UpdaterBuildInfo.Version);
                sb.AppendLine("Only whitelisted, bounded loot events extracted from SavedVariables:");
                sb.AppendLine("```text");
                sb.AppendLine(autoLootLog);
                sb.AppendLine("```");
                sb.AppendLine("No raw SavedVariables files, account-folder names or credentials attached.");
                return sb.ToString();
            }

            private bool ConfirmAutoLootReport(string report)
            {
                using (var dialog = new Form())
                using (var preview = new TextBox())
                using (var yes = new Button())
                using (var no = new Button())
                {
                    dialog.Text = "Potwierdź wysłanie logu AutoLoot do GitHub Issues";
                    dialog.StartPosition = FormStartPosition.CenterParent;
                    dialog.ClientSize = new Size(720, 460);
                    dialog.MinimumSize = new Size(600, 370);
                    var caption = new Label {
                        Text = "Podgląd dokładnych zdarzeń wysyłanych do repo github12wykrzyk/335. " +
                            "Nie wysyłamy surowych plików WTF ani nazw katalogów kont.",
                        Dock = DockStyle.Top, Height = 45,
                        Padding = new Padding(10, 8, 10, 3)
                    };
                    preview.Multiline = true;
                    preview.ReadOnly = true;
                    preview.ScrollBars = ScrollBars.Both;
                    preview.WordWrap = false;
                    preview.Font = new Font("Consolas", 9f);
                    preview.Dock = DockStyle.Fill;
                    preview.Text = report;
                    var footer = new Panel { Dock = DockStyle.Bottom, Height = 49 };
                    yes.Text = "WYŚLIJ DO GITHUB";
                    yes.SetBounds(405, 9, 165, 32);
                    yes.DialogResult = DialogResult.Yes;
                    no.Text = "ANULUJ";
                    no.SetBounds(575, 9, 130, 32);
                    no.DialogResult = DialogResult.No;
                    footer.Controls.Add(yes);
                    footer.Controls.Add(no);
                    dialog.Controls.Add(preview);
                    dialog.Controls.Add(caption);
                    dialog.Controls.Add(footer);
                    dialog.AcceptButton = yes;
                    dialog.CancelButton = no;
                    return dialog.ShowDialog(form) == DialogResult.Yes;
                }
            }

            private static string[] GetRecentDiagFiles(string root)
            {
                var debugDir = Path.Combine(root, ".wow335_debug");
                if (!Directory.Exists(debugDir)) return new string[0];
                return Directory.GetFiles(debugDir, "*.jsonl")
                    .OrderByDescending(File.GetLastWriteTimeUtc)
                    .Take(3)
                    .ToArray();
            }

            private async Task<long> FindExistingIssueAsync(HttpClient client, string marker)
            {
                using (var response = await client.GetAsync(ApiRoot + "/issues?state=open&per_page=100"))
                {
                    var text = await response.Content.ReadAsStringAsync();
                    if (!response.IsSuccessStatusCode)
                    {
                        HandleAuthenticationFailure(response.StatusCode);
                        throw new InvalidOperationException("GitHub Issues HTTP " + (int)response.StatusCode + ": " + TrimForError(text));
                    }
                    foreach (var item in AsArray(json.DeserializeObject(text)))
                    {
                        var row = item as Dictionary<string, object>;
                        if (row == null) continue;
                        if (GetString(row, "title").IndexOf(marker, StringComparison.OrdinalIgnoreCase) >= 0)
                            return GetLong(row, "number");
                    }
                }
                return 0;
            }

            private static HttpClient CreateClient(string reportToken)
            {
                var client = new HttpClient(new HttpClientHandler { AllowAutoRedirect = true });
                client.Timeout = TimeSpan.FromMinutes(2);
                client.DefaultRequestHeaders.UserAgent.ParseAdd("WoW335Updater/" + UpdaterBuildInfo.Version);
                client.DefaultRequestHeaders.Accept.Add(new MediaTypeWithQualityHeaderValue("application/vnd.github+json"));
                client.DefaultRequestHeaders.Add("X-GitHub-Api-Version", "2022-11-28");
                client.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", reportToken.Trim());
                return client;
            }

            private string PromptForToken()
            {
                using (var dialog = new Form())
                using (var box = new TextBox())
                using (var ok = new Button())
                using (var cancel = new Button())
                {
                    dialog.Text = "GitHub report token";
                    dialog.ClientSize = new Size(620, 150);
                    dialog.StartPosition = FormStartPosition.CenterParent;
                    dialog.FormBorderStyle = FormBorderStyle.FixedDialog;
                    dialog.MaximizeBox = false;
                    dialog.MinimizeBox = false;
                    dialog.Controls.Add(new Label { Text = "Osobny fine-grained token dla repo 335: Issues = Read and write", Left = 16, Top = 16, AutoSize = true });
                    box.Left = 16;
                    box.Top = 45;
                    box.Width = 588;
                    box.UseSystemPasswordChar = true;
                    dialog.Controls.Add(box);
                    ok.Text = "ZAPISZ";
                    ok.SetBounds(378, 92, 108, 32);
                    ok.DialogResult = DialogResult.OK;
                    cancel.Text = "ANULUJ";
                    cancel.SetBounds(496, 92, 108, 32);
                    cancel.DialogResult = DialogResult.Cancel;
                    dialog.Controls.Add(ok);
                    dialog.Controls.Add(cancel);
                    dialog.AcceptButton = ok;
                    dialog.CancelButton = cancel;
                    return dialog.ShowDialog(form) == DialogResult.OK ? box.Text.Trim() : string.Empty;
                }
            }

            private static string ReportTokenPath()
            {
                return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "WoW335Updater", "report_token.dpapi");
            }

            private static byte[] ReportEntropy()
            {
                return Encoding.UTF8.GetBytes("WoW335Updater-report-token-v1");
            }

            private string LoadReportToken()
            {
                try
                {
                    var path = ReportTokenPath();
                    if (!File.Exists(path)) return string.Empty;
                    var protectedBytes = Convert.FromBase64String(File.ReadAllText(path, Encoding.ASCII));
                    return Encoding.UTF8.GetString(ProtectedData.Unprotect(protectedBytes, ReportEntropy(), DataProtectionScope.CurrentUser));
                }
                catch (Exception ex)
                {
                    Log("Ostrzeżenie: nie udało się odczytać tokenu raportowego: " + ex.Message);
                    DeleteReportToken();
                    return string.Empty;
                }
            }

            private static void SaveReportToken(string value)
            {
                var path = ReportTokenPath();
                Directory.CreateDirectory(Path.GetDirectoryName(path));
                var protectedBytes = ProtectedData.Protect(Encoding.UTF8.GetBytes(value.Trim()), ReportEntropy(), DataProtectionScope.CurrentUser);
                File.WriteAllText(path, Convert.ToBase64String(protectedBytes), Encoding.ASCII);
            }

            private static void DeleteReportToken()
            {
                try
                {
                    var path = ReportTokenPath();
                    if (File.Exists(path)) File.Delete(path);
                }
                catch
                {
                }
            }

            private void HandleAuthenticationFailure(System.Net.HttpStatusCode statusCode)
            {
                var code = (int)statusCode;
                if (code != 401 && code != 403) return;
                DeleteReportToken();
                Log("Token raportowy został odrzucony przez GitHub i usunięty z lokalnego magazynu. Przy następnej próbie updater poprosi o nowy token.");
            }

            private void SetBusy(bool value, string text)
            {
                busy = value;
                sendButton.Enabled = !value;
                tokenButton.Enabled = !value;
                autoLootDiagSendButton.Enabled = !value;
                if (status != null) status.Text = text;
                form.Cursor = value ? Cursors.WaitCursor : Cursors.Default;
            }

            private void Log(string message)
            {
                if (log == null) return;
                log.AppendText("[" + DateTime.Now.ToString("HH:mm:ss") + "] " + message + Environment.NewLine);
                log.SelectionStart = log.TextLength;
                log.ScrollToCaret();
            }

            private static string TailFile(string path, int maxChars)
            {
                try
                {
                    using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
                    {
                        if (stream.Length == 0) return string.Empty;
                        var maxBytes = Math.Max(4096L, (long)maxChars * 4L);
                        var count = (int)Math.Min(stream.Length, maxBytes);
                        stream.Seek(-count, SeekOrigin.End);
                        var buffer = new byte[count];
                        var total = 0;
                        while (total < count)
                        {
                            var read = stream.Read(buffer, total, count - total);
                            if (read <= 0) break;
                            total += read;
                        }
                        var text = Encoding.UTF8.GetString(buffer, 0, total);
                        return TailText(text, maxChars);
                    }
                }
                catch (Exception ex)
                {
                    return "<read error: " + ex.Message + ">";
                }
            }

            private static string TailText(string text, int maxChars)
            {
                if (string.IsNullOrEmpty(text) || text.Length <= maxChars) return text ?? string.Empty;
                return "<truncated>\n" + text.Substring(text.Length - maxChars);
            }

            private static string Sanitize(string text, string root, int maxChars)
            {
                text = TailText(text, maxChars);
                if (!string.IsNullOrWhiteSpace(root)) text = ReplaceInsensitive(text, root, "<GAME_DIR>");
                var profile = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
                if (!string.IsNullOrWhiteSpace(profile)) text = ReplaceInsensitive(text, profile, "<USER_PROFILE>");
                return text;
            }

            private static string ReplaceInsensitive(string text, string value, string replacement)
            {
                if (string.IsNullOrEmpty(text) || string.IsNullOrEmpty(value)) return text ?? string.Empty;
                var sb = new StringBuilder();
                var start = 0;
                while (start < text.Length)
                {
                    var index = text.IndexOf(value, start, StringComparison.OrdinalIgnoreCase);
                    if (index < 0)
                    {
                        sb.Append(text, start, text.Length - start);
                        break;
                    }
                    sb.Append(text, start, index - start);
                    sb.Append(replacement);
                    start = index + value.Length;
                }
                return sb.ToString();
            }

            private static string Sha256Text(string text)
            {
                using (var sha = SHA256.Create())
                {
                    var bytes = sha.ComputeHash(Encoding.UTF8.GetBytes(text ?? string.Empty));
                    var sb = new StringBuilder(bytes.Length * 2);
                    foreach (var b in bytes) sb.Append(b.ToString("x2"));
                    return sb.ToString();
                }
            }

            private static T GetPrivateField<T>(object instance, string name) where T : class
            {
                var field = instance.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic);
                return field == null ? null : field.GetValue(instance) as T;
            }

            private static Dictionary<string, object> AsDictionary(object value)
            {
                var dict = value as Dictionary<string, object>;
                if (dict == null) throw new InvalidOperationException("Nieoczekiwany JSON.");
                return dict;
            }

            private static object[] AsArray(object value)
            {
                if (value == null) return new object[0];
                var array = value as object[];
                if (array != null) return array;
                var list = value as ArrayList;
                return list == null ? new object[0] : list.ToArray();
            }

            private static object GetValue(Dictionary<string, object> dict, string key)
            {
                object value;
                return dict != null && dict.TryGetValue(key, out value) ? value : null;
            }

            private static string GetString(Dictionary<string, object> dict, string key)
            {
                var value = GetValue(dict, key);
                return value == null ? string.Empty : Convert.ToString(value);
            }

            private static long GetLong(Dictionary<string, object> dict, string key)
            {
                var value = GetValue(dict, key);
                return value == null ? 0L : Convert.ToInt64(value);
            }

            private static string ShortSha(string sha)
            {
                return string.IsNullOrWhiteSpace(sha) ? "unknown" : sha.Substring(0, Math.Min(8, sha.Length));
            }

            private static string TrimForError(string text)
            {
                if (string.IsNullOrWhiteSpace(text)) return "brak treści odpowiedzi";
                text = text.Replace("\r", " ").Replace("\n", " ").Trim();
                return text.Length <= 300 ? text : text.Substring(0, 300) + "...";
            }
        }
    }
}
