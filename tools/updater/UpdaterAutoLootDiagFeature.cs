using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Windows.Forms;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;

namespace WoW335Updater
{
    // Independent diagnostic addon. Deliberately NOT part of the native DLL
    // manifest, game candidate or updater-managed Wow.exe replacement.
    internal static class AutoLootDiagSupport
    {
        internal const string AddonName = "WoW335AutoLootDiag";
        private const string ScriptName = "WoW335AutoLootDiag.lua";
        private const string TocName = "WoW335AutoLootDiag.toc";
        private const string MarkerName = ".wow335_diag_managed.txt";
        private const int MaxLuaBytes = 512 * 1024;
        private const int MaxEntries = 120;
        private const int MaxReportChars = 14500;

        private static byte[] Resource(Assembly assembly, string name)
        {
            using (var input = assembly.GetManifestResourceStream("AutoLootDiag." + name))
            {
                if (input == null) throw new InvalidOperationException(
                    "Updater nie zawiera zasobu AutoLoot: " + name +
                    ". Pobierz pełny artefakt eksperymentalnego updatera.");
                using (var output = new MemoryStream())
                {
                    input.CopyTo(output);
                    if (output.Length == 0 || output.Length > 100 * 1024)
                        throw new InvalidOperationException("Nieprawidłowy rozmiar zasobu AutoLoot.");
                    return output.ToArray();
                }
            }
        }

        private static string Hash(byte[] bytes)
        {
            using (var sha = SHA256.Create())
                return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "").ToLowerInvariant();
        }

        private static string HashFile(string file)
        {
            using (var stream = File.OpenRead(file))
            using (var sha = SHA256.Create())
                return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
        }

        private static void RejectLink(string path)
        {
            if ((File.Exists(path) || Directory.Exists(path)) &&
                (File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0)
                throw new InvalidOperationException("AutoLoot: ścieżka jest dowiązaniem: " +
                                                    Path.GetFileName(path));
        }

        private static bool IsManagedDirectory(string folder, byte[] script, byte[] toc)
        {
            var mark = Path.Combine(folder, MarkerName);
            if (!File.Exists(mark)) return false;
            var marker = File.ReadAllText(mark, Encoding.UTF8);
            return marker.Contains("WOW335_AUTOLOOT_DIAG_MANAGED_V1") &&
                   marker.Contains(HashFile(Path.Combine(folder, ScriptName))) &&
                   marker.Contains(HashFile(Path.Combine(folder, TocName))) &&
                   HashFile(Path.Combine(folder, ScriptName)) == Hash(script) &&
                   HashFile(Path.Combine(folder, TocName)) == Hash(toc);
        }

        // Refresh only an untouched addon previously installed by this updater.
        // Never adopt or overwrite manually edited addons during game updates.
        internal static string RefreshManagedIfPresent(string root, Assembly assembly)
        {
            if (string.IsNullOrWhiteSpace(root)) return null;
            var addonDir = Path.Combine(root, "Interface", "AddOns", AddonName);
            if (!Directory.Exists(addonDir)) return null;
            if ((File.GetAttributes(addonDir) & FileAttributes.ReparsePoint) != 0 ||
                Directory.GetDirectories(addonDir).Length != 0) return null;
            var files = Directory.GetFiles(addonDir).Select(Path.GetFileName)
                .OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToArray();
            var expected = new[] { ScriptName, TocName, MarkerName }
                .OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToArray();
            if (!files.SequenceEqual(expected, StringComparer.OrdinalIgnoreCase))
                return null;
            foreach (var file in files) RejectLink(Path.Combine(addonDir, file));
            var marker = File.ReadAllText(Path.Combine(addonDir, MarkerName), Encoding.UTF8)
                .Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);
            if (marker.Length != 4 || marker[0] != "WOW335_AUTOLOOT_DIAG_MANAGED_V1" ||
                !Regex.IsMatch(marker[1], "^[0-9a-f]{40}$") ||
                !Regex.IsMatch(marker[2], "^[0-9a-f]{64}$") ||
                !Regex.IsMatch(marker[3], "^[0-9a-f]{64}$") ||
                !string.Equals(marker[2], HashFile(Path.Combine(addonDir, ScriptName)),
                    StringComparison.OrdinalIgnoreCase) ||
                !string.Equals(marker[3], HashFile(Path.Combine(addonDir, TocName)),
                    StringComparison.OrdinalIgnoreCase))
                return null;
            return Install(root, assembly, true); // backs up the exact previous managed addon
        }

        internal static string Install(string root, Assembly assembly, bool replaceExisting)
        {
            if (string.IsNullOrWhiteSpace(root) || !Directory.Exists(root))
                throw new InvalidOperationException("Wybierz poprawny katalog WoW 3.3.5a.");
            root = Path.GetFullPath(root);
            var exe = Path.Combine(root, "Wow.exe");
            if (!File.Exists(exe) ||
                !string.Equals(HashFile(exe), UpdaterBuildInfo.PinnedClientSha256,
                               StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("Wow.exe różni się od wybranego klienta build 12340; instalacja dodatku przerwana.");

            var script = Resource(assembly, ScriptName);
            var toc = Resource(assembly, TocName);
            var sourceSha = Encoding.UTF8.GetString(Resource(assembly, "SourceCommit.txt")).Trim();
            if (!Regex.IsMatch(sourceSha, "^[0-9a-f]{40}$") ||
                !Encoding.UTF8.GetString(toc).StartsWith("## Interface: 30300", StringComparison.Ordinal) ||
                !Encoding.UTF8.GetString(script).Contains("local enabled = false"))
                throw new InvalidOperationException("Nieprawidłowa wersja lub proweniencja dodatku.");

            var interfaceDir = Path.Combine(root, "Interface");
            var addonsDir = Path.Combine(interfaceDir, "AddOns");
            var addonDir = Path.Combine(addonsDir, AddonName);
            RejectLink(interfaceDir);
            RejectLink(addonsDir);
            RejectLink(addonDir);
            Directory.CreateDirectory(addonsDir);

            if (Directory.Exists(addonDir))
            {
                foreach (var existingFile in Directory.GetFiles(addonDir)) RejectLink(existingFile);
                foreach (var existingDir in Directory.GetDirectories(addonDir)) RejectLink(existingDir);
                if (IsManagedDirectory(addonDir, script, toc))
                    return "Dodatek AutoLoot DIAG jest już zainstalowany i zgodny z tym buildem (" +
                           sourceSha.Substring(0, 10) + ").";
                // An exact manual install may be adopted non-destructively.
                var files = Directory.GetFiles(addonDir).Select(Path.GetFileName)
                    .OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToArray();
                var expectedFiles = new[] { ScriptName, TocName }
                    .OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToArray();
                var same = files.SequenceEqual(expectedFiles, StringComparer.OrdinalIgnoreCase) &&
                           Directory.GetDirectories(addonDir).Length == 0 &&
                           HashFile(Path.Combine(addonDir, ScriptName)) == Hash(script) &&
                           HashFile(Path.Combine(addonDir, TocName)) == Hash(toc);
                if (same)
                {
                    File.WriteAllText(Path.Combine(addonDir, MarkerName),
                        "WOW335_AUTOLOOT_DIAG_MANAGED_V1\n" + sourceSha + "\n" +
                        Hash(script) + "\n" + Hash(toc) + "\n", Encoding.UTF8);
                    return "Wykryto ten sam dodatek; przejęto zarządzanie bez nadpisywania plików. Commit " + sourceSha.Substring(0, 10);
                }
                if (!replaceExisting)
                    throw new InvalidOperationException("Katalog dodatku już istnieje. Wymagana jawna zgoda na kopię i zastąpienie.");
            }

            var stage = Path.Combine(addonsDir, ".wow335-autoloot-stage-" + Guid.NewGuid().ToString("N"));
            string backup = null;
            Directory.CreateDirectory(stage);
            try
            {
                File.WriteAllBytes(Path.Combine(stage, ScriptName), script);
                File.WriteAllBytes(Path.Combine(stage, TocName), toc);
                File.WriteAllText(Path.Combine(stage, MarkerName),
                    "WOW335_AUTOLOOT_DIAG_MANAGED_V1\n" + sourceSha + "\n" +
                    Hash(script) + "\n" + Hash(toc) + "\n", Encoding.UTF8);
                if (HashFile(Path.Combine(stage, ScriptName)) != Hash(script) ||
                    HashFile(Path.Combine(stage, TocName)) != Hash(toc))
                    throw new IOException("Weryfikacja plików dodatku nie powiodła się.");

                if (Directory.Exists(addonDir))
                {
                    var backupParent = Path.Combine(root, ".wow335_updater", "autoloot_diag_backups");
                    Directory.CreateDirectory(backupParent);
                    backup = Path.Combine(backupParent, DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_") +
                                                       Guid.NewGuid().ToString("N"));
                    Directory.Move(addonDir, backup); // preserve ALL pre-existing files
                }
                try { Directory.Move(stage, addonDir); }
                catch
                {
                    if (backup != null && !Directory.Exists(addonDir))
                        Directory.Move(backup, addonDir);
                    throw;
                }
                return "Dodatek AutoLoot DIAG zainstalowany (" + sourceSha.Substring(0, 10) +
                       ")." + (backup == null ? "" : " Kopia poprzedniej wersji: " + backup);
            }
            finally
            {
                if (Directory.Exists(stage)) Directory.Delete(stage, true);
            }
        }

        // Never send the raw SavedVariables file, account-directory names or
        // arbitrary addon data. Only fixed event types with bounded text.
        private static readonly Regex Entry = new Regex(
            @"^\s*(?:\[\s*(?:\d+|""\d+"")\s*\]\s*=\s*)?""((?:\\.|[^""\\])*)""\s*,?\s*(?:--\s*\[\d+\]\s*)?$",
            RegexOptions.Compiled | RegexOptions.CultureInvariant);
        private static readonly Regex EntriesStart = new Regex(
            @"^\s*(?:\[\s*""entries""\s*\]|entries)\s*=\s*\{",
            RegexOptions.Compiled | RegexOptions.CultureInvariant);
        private static readonly Regex Event = new Regex(
            @"^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} " +
            @"(?:LOGIN|LOOT_OPENED|LOOT_SLOT_CLEARED|LOOT_CLOSED|LOOT_CALL_FAILED|" +
            @"DRAIN|UI_ERROR|LOGOUT|ENABLE|DISABLE|LOG_CLEARED)\b",
            RegexOptions.Compiled | RegexOptions.CultureInvariant);

        private static IEnumerable<string> ReadSavedEntries(string path)
        {
            var info = new FileInfo(path);
            if (info.Length < 1 || info.Length > MaxLuaBytes) yield break;
            var inEntries = false;
            foreach (var line in File.ReadLines(path, Encoding.UTF8))
            {
                if (!inEntries)
                {
                    if (EntriesStart.IsMatch(line)) inEntries = true;
                    continue;
                }
                if (Regex.IsMatch(line, @"^\s*\},?\s*$")) yield break;
                var match = Entry.Match(line);
                if (!match.Success) continue;
                var value = match.Groups[1].Value;
                value = value.Replace("\\\\", "\\").Replace("\\\"", "\"");
                if (!Event.IsMatch(value)) continue;
                if (value.Length > 240) value = value.Substring(0, 240) + " [TRUNCATED]";
                value = Regex.Replace(value, @"[\x00-\x1F\x7F]", " ");
                yield return value;
            }
        }

        internal static string EmbeddedCommit(Assembly assembly)
        {
            var sha = Encoding.UTF8.GetString(Resource(assembly, "SourceCommit.txt")).Trim();
            if (!Regex.IsMatch(sha, "^[0-9a-f]{40}$"))
                throw new InvalidOperationException("Brak dokładnego SHA źródeł dodatku.");
            return sha;
        }

        internal static string[] FindSavedVariableFiles(string root)
        {
            if (string.IsNullOrWhiteSpace(root) || !Directory.Exists(root)) return new string[0];
            var accountRoot = Path.Combine(Path.GetFullPath(root), "WTF", "Account");
            if (!Directory.Exists(accountRoot)) return new string[0];
            RejectLink(accountRoot);
            var found = new List<string>();
            foreach (var dir in Directory.GetDirectories(accountRoot).Take(64))
            {
                RejectLink(dir);
                var saved = Path.Combine(dir, "SavedVariables");
                if (!Directory.Exists(saved)) continue;
                RejectLink(saved);
                foreach (var filename in new[] { "WoW335AutoLootDiag.lua",
                                                  "WoW335AutoLootDiag.lua.bak",
                                                  "WoW335AutoLootDiagLog.lua",
                                                  "WoW335AutoLootDiagLog.lua.bak" })
                {
                    var file = Path.Combine(saved, filename);
                    if (!File.Exists(file)) continue;
                    RejectLink(file);
                    found.Add(file);
                }
            }
            return found.OrderByDescending(File.GetLastWriteTimeUtc).Take(64).ToArray();
        }

        private static bool IsSavedVariableName(string file)
        {
            var name = Path.GetFileName(file);
            return Regex.IsMatch(name,
                @"^WoW335AutoLootDiag(?:Log)?(?:\(\d{1,3}\))?\.lua(?:\.bak)?$",
                RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);
        }

        internal static string CollectLogFile(string file)
        {
            if (string.IsNullOrWhiteSpace(file) || !File.Exists(file) ||
                !IsSavedVariableName(file))
                throw new InvalidOperationException(
                    "Wskaż plik WoW335AutoLootDiag.lua lub jego kopię .lua.bak.");
            RejectLink(file);
            var sb = new StringBuilder();
            foreach (var entry in ReadSavedEntries(file))
            {
                if (sb.Length + entry.Length + 1 > MaxReportChars) break;
                sb.AppendLine(entry);
                if (sb.Length >= MaxReportChars) break;
            }
            return sb.ToString();
        }

        internal static string CollectLog(string root)
        {
            var sb = new StringBuilder();
            var found = 0;
            foreach (var path in FindSavedVariableFiles(root))
            {
                foreach (var entry in ReadSavedEntries(path))
                {
                    if (sb.Length + entry.Length + 1 > MaxReportChars) return sb.ToString();
                    sb.AppendLine(entry);
                    if (++found >= MaxEntries) return sb.ToString();
                }
            }
            return sb.ToString();
        }

        internal static string ExplainMissingLog(string root)
        {
            var full = Path.GetFullPath(root);
            var expected = Path.Combine(full, "WTF", "Account");
            var addon = Path.Combine(full, "Interface", "AddOns", AddonName);
            var files = FindSavedVariableFiles(root);
            var sb = new StringBuilder();
            sb.AppendLine("Updater nie znalazł zdarzeń AutoLoot DIAG w wybranym katalogu gry.");
            sb.AppendLine("Dodatek: " + (File.Exists(Path.Combine(addon, TocName)) &&
                                            File.Exists(Path.Combine(addon, ScriptName))
                ? "pliki dodatku są obecne" : "BRAK plików dodatku; kliknij «Instaluj test AutoLoot»"));
            if (!Directory.Exists(expected))
                sb.AppendLine("Brak katalogu WTF\\Account w wybranej lokalizacji: " + expected);
            else if (files.Length == 0)
                sb.AppendLine("Brak WoW335AutoLootDiag.lua w: " +
                    Path.Combine(expected, "<konto>", "SavedVariables"));
            else
            {
                sb.AppendLine("Znaleziono " + files.Length + " plik(i), ale nie odczytano poprawnych wpisów.");
                foreach (var file in files.Take(4))
                    sb.AppendLine("Lokalny plik: " + file +
                        " (" + new FileInfo(file).Length + " B, " +
                        File.GetLastWriteTime(file).ToString("yyyy-MM-dd HH:mm:ss") + ")");
                sb.AppendLine("Możliwy inny format tabeli lub dodatek nie zapisał zdarzeń.");
            }
            sb.AppendLine("W grze sprawdź /al335 status oraz /al335 log, potem /reload i zamknij WoW.");
            sb.AppendLine("Możesz też wskazać konkretny lokalny plik .lua / .lua.bak.");
            return sb.ToString();
        }

        internal static string SanitizeLog(string entries)
        {
            // ReadSavedEntries already limits input to a small event whitelist.
            var text = entries ?? "";
            text = Regex.Replace(text, @"(?i)\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}\b", "<EMAIL>");
            text = Regex.Replace(text, @"\b(?:\d{1,3}\.){3}\d{1,3}\b", "<IP>");
            text = Regex.Replace(text, @"(?i)(?:[A-Z]:\\|/Users/|/home/)[^\s]{1,140}", "<PATH>");
            text = Regex.Replace(text, @"(?i)\b(?:ghp_|github_pat_|gho_|ghu_|ghs_)[A-Za-z0-9_]{8,}\b", "<TOKEN>");
            return text.Length > MaxReportChars ? text.Substring(0, MaxReportChars) : text;
        }
    }

    internal sealed partial class MainForm
    {
        private readonly Button autoLootDiagInstallButton = new Button();

        private void InstallAutoLootDiagnostic()
        {
            if (busy) return;
            var root = gameDir.Text.Trim();
            try
            {
                if (string.IsNullOrWhiteSpace(root) || !Directory.Exists(root))
                    throw new InvalidOperationException("Wybierz katalog klienta 3.3.5a.");
                if (IsGameRunning(root))
                    throw new InvalidOperationException("Zamknij WoW przed instalacją dodatku. /reload zapisuje aktualny log.");
                var existing = Path.Combine(root, "Interface", "AddOns", AutoLootDiagSupport.AddonName);
                bool allowReplacement = false;
                if (Directory.Exists(existing))
                {
                    var answer = MessageBox.Show(this,
                        "Folder dodatku AutoLoot już istnieje. Jeżeli pliki są inne niż w nowej wersji, updater zachowa CAŁY dotychczasowy folder w kopii zapasowej, po czym zainstaluje tylko ten dodatek. Inne dodatki, Wow.exe i DLL pozostaną nietknięte.\n\nKontynuować?",
                        "WoW335 — instalacja dodatku diagnostycznego",
                        MessageBoxButtons.YesNo, MessageBoxIcon.Question);
                    if (answer != DialogResult.Yes) return;
                    allowReplacement = true;
                }
                SetBusy(true, "Instalowanie niezależnego dodatku AutoLoot DIAG...");
                var result = AutoLootDiagSupport.Install(root,
                    System.Reflection.Assembly.GetExecutingAssembly(), allowReplacement);
                Log("AutoLoot: " + result);
                MessageBox.Show(this, result +
                    "\n\nPo uruchomieniu WoW: /al335 on, ręcznie otwórz zwłoki NPC, potem /reload. W updaterze wybierz «Wyślij log AutoLoot». To NIE jest działający natywny AutoLoot.",
                    "WoW335 — dodatek testowy", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                Log("BŁĄD instalacji AutoLoot DIAG: " + ex.Message);
                MessageBox.Show(this, ex.Message, "WoW335 — AutoLoot DIAG",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally { SetBusy(false, "Gotowy — dodatek AutoLoot DIAG jest niezależny od paczki gry"); }
        }
    }
}
