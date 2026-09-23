using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace WoW335Updater
{
    // Separate, explicit TEST transaction. Never installed by the normal work/main package path.
    internal sealed partial class MainForm
    {
        private const string EpochBranch = "feature/loader-12340";
        private const string EpochWorkflow = "Build EpochConnection 12340 isolated TEST";
        private const string EpochOriginalSha = "9af04f7afd21b0bc93860d66e6ccdc96ccf1b039ea31871119a3deaa271352c3";
        private const string EpochDll = "EpochConnection.dll";
        private const string EpochLoader = "Wow335Loader.dll";
        private readonly Button epochTestInstallButton = new Button();
        private readonly Button epochModulesButton = new Button();
        private readonly Button epochTestRollbackButton = new Button();

        private static string EpochStatePath(string root)
        {
            return Path.Combine(root, ".wow335_updater", "epoch_test", "installed.json");
        }

        private static void EpochNotLink(string path)
        {
            if ((File.Exists(path) || Directory.Exists(path)) &&
                (File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0)
                throw new InvalidOperationException("Epoch TEST: odmowa użycia dowiązania: " + path);
        }

        private static string EpochManager(string root)
        {
            var top = Path.Combine(root, ".wow335_updater");
            EpochNotLink(root);
            EpochNotLink(top);
            var sub = Path.Combine(top, "epoch_test");
            EpochNotLink(sub);
            return sub;
        }

        private static ZipArchiveEntry EpochZipEntry(ZipArchive archive, string file)
        {
            var found = archive.Entries.Where(entry =>
                string.Equals(entry.FullName, file, StringComparison.Ordinal)).ToArray();
            if (found.Length != 1 || found[0].Length > 4 * 1024 * 1024 ||
                found[0].CompressedLength > 4 * 1024 * 1024 ||
                found[0].Name != found[0].FullName)
                throw new InvalidOperationException("Epoch TEST: nieprawidłowy wpis artefaktu: " + file);
            return found[0];
        }

        private static void EpochPE32(byte[] bytes)
        {
            if (bytes.Length < 512 || bytes[0] != 'M' || bytes[1] != 'Z')
                throw new InvalidOperationException("Epoch TEST: plik nie jest PE.");
            int pe = BitConverter.ToInt32(bytes, 0x3c);
            if (pe < 64 || pe > bytes.Length - 28 ||
                BitConverter.ToInt32(bytes, pe) != 0x4550 ||
                BitConverter.ToUInt16(bytes, pe + 4) != 0x14c ||
                BitConverter.ToUInt16(bytes, pe + 24) != 0x10b ||
                (BitConverter.ToUInt16(bytes, pe + 22) & 0x2000) == 0)
                throw new InvalidOperationException("Epoch TEST: wymagane prawdziwe DLL PE32 x86.");
        }

        private Dictionary<string, object> EpochInstalled(string root)
        {
            var path = EpochStatePath(root);
            EpochNotLink(path);
            if (!File.Exists(path)) return null;
            var state = AsDictionary(json.DeserializeObject(File.ReadAllText(path, Encoding.UTF8)));
            if (GetString(state, "kind") != "EPOCH_CONNECTION_ISOLATED_TEST_PAIR" ||
                GetString(state, "branch") != EpochBranch ||
                !UpdaterSafety.IsSha256Hex(GetString(state, "git_sha")) ||
                !UpdaterSafety.IsSha256Hex(GetString(state, "epoch_sha256")) ||
                !UpdaterSafety.IsSha256Hex(GetString(state, "loader_sha256")) ||
                !UpdaterSafety.IsSha256Hex(GetString(state, "dlls_sha256")))
                throw new InvalidOperationException("Epoch TEST: uszkodzony manifest lokalnej instalacji.");
            var id = GetString(state, "backup_id");
            if (id.Length != 32 || id.Any(c => "0123456789abcdef".IndexOf(c) < 0))
                throw new InvalidOperationException("Epoch TEST: niewłaściwy identyfikator kopii.");
            return state;
        }

        private static void EpochCheckFile(string path, string hash)
        {
            EpochNotLink(path);
            if (!File.Exists(path) ||
                !string.Equals(Sha256File(path), hash, StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("Epoch TEST: plik zmienił się lub go brakuje: " +
                    Path.GetFileName(path) + ". Nie wykonuję destrukcyjnej operacji.");
        }

        private static void EpochStageFile(string source, string target, string sha)
        {
            EpochCheckFile(source, sha);
            EpochNotLink(target);
            var temp = target + ".epochnew";
            EpochNotLink(temp);
            if (File.Exists(temp))
                throw new InvalidOperationException("Epoch TEST: istnieje obcy plik tymczasowy.");
            File.Copy(source, temp);
            try
            {
                EpochCheckFile(temp, sha);
                UpdaterSafety.ReplaceFile(temp, target, ".epochreplace");
                EpochCheckFile(target, sha);
            }
            finally
            {
                if (File.Exists(temp)) File.Delete(temp);
            }
        }

        private async Task EpochInstallAsync()
        {
            if (busy) return;
            string statusText = "Epoch TEST: nie zainstalowano.";
            bool acquired = false;
            try
            {
                var root = Path.GetFullPath(gameDir.Text.Trim());
                if (!Directory.Exists(root)) throw new InvalidOperationException("Wybierz katalog WoW.");
                if (IsGameRunning(root)) throw new InvalidOperationException("Zamknij WoW przed zmianą DLL.");
                EpochCheckFile(Path.Combine(root, "Wow.exe"), UpdaterBuildInfo.PinnedClientSha256);
                EpochCheckFile(Path.Combine(root, EpochDll), EpochOriginalSha);
                if (EpochInstalled(root) != null)
                    throw new InvalidOperationException("Epoch TEST jest zainstalowany; użyj przycisku rollback.");
                if (string.IsNullOrWhiteSpace(token.Text))
                    throw new InvalidOperationException("Wymagany token GitHub Contents: Read i Actions: Read.");
                var manager = EpochManager(root);
                var loaderPath = Path.Combine(root, EpochLoader);
                var listPath = Path.Combine(root, "dlls.txt");
                EpochNotLink(loaderPath); EpochNotLink(listPath);
                if (File.Exists(loaderPath))
                    throw new InvalidOperationException("Epoch TEST: kolizja z istniejącym Wow335Loader.dll.");
                if (File.Exists(listPath) &&
                    !string.IsNullOrWhiteSpace(File.ReadAllText(listPath, Encoding.UTF8)))
                    throw new InvalidOperationException("Epoch TEST: istniejący dlls.txt zawiera niezarządzane moduły. Nie nadpisuję go.");
                SetBusy(true, "Epoch TEST: sprawdzam aktualny HEAD i Windows CI...");
                acquired = true;
                string sha, archiveUrl;
                using (var client = CreateClient())
                {
                    var branch = AsDictionary(json.DeserializeObject(
                        await GetStringAsync(client, ApiRoot + "/branches/" + EpochBranch)));
                    sha = GetString(AsDictionary(GetValue(branch, "commit")), "sha");
                    if (!UpdaterSafety.IsSha256Hex(sha))
                        throw new InvalidOperationException("Epoch TEST: brak aktualnego SHA brancha.");
                    var runs = AsArray(GetValue(AsDictionary(json.DeserializeObject(
                        await GetStringAsync(client, ApiRoot + "/actions/runs?branch=" + EpochBranch + "&per_page=50"))),
                        "workflow_runs"));
                    var run = UpdaterSafety.RequireLatestSuccessfulRun(runs, EpochWorkflow, EpochBranch);
                    if (!string.Equals(GetString(run, "head_sha"), sha, StringComparison.Ordinal))
                        throw new InvalidOperationException("Epoch TEST: udany CI jest dla starszego SHA.");
                    var runId = GetLong(run, "id");
                    var artifacts = AsArray(GetValue(AsDictionary(json.DeserializeObject(
                        await GetStringAsync(client, ApiRoot + "/actions/runs/" + runId +
                            "/artifacts?per_page=100"))), "artifacts"));
                    var named = "EpochConnection-TEST-PAIR-" + sha;
                    var artifact = artifacts.Select(AsDictionary).SingleOrDefault(a =>
                        !GetBool(a, "expired") &&
                        string.Equals(GetString(a, "name"), named, StringComparison.Ordinal));
                    if (artifact == null)
                        throw new InvalidOperationException("Epoch TEST: bieżący SHA nie ma artefaktu z pozytywnym wynikiem weryfikacji.");
                    archiveUrl = GetString(artifact, "archive_download_url");
                }
                var downloaded = await DownloadBytesAsync(archiveUrl);
                byte[] epoch, loader;
                string epochSha, loaderSha;
                using (var mem = new MemoryStream(downloaded, false))
                using (var zip = new ZipArchive(mem, ZipArchiveMode.Read, false))
                {
                    var names = new[] { EpochDll, EpochLoader, "epoch_patch.json", "epoch_test_pair.json" };
                    if (zip.Entries.Count != names.Length ||
                        zip.Entries.Any(e => !names.Contains(e.FullName, StringComparer.Ordinal)))
                        throw new InvalidOperationException("Epoch TEST: artefakt zawiera obce pliki.");
                    var manifest = AsDictionary(json.DeserializeObject(
                        Encoding.UTF8.GetString(ReadEntry(EpochZipEntry(zip, "epoch_test_pair.json")))));
                    if (GetLong(manifest, "schema_version") != 1 ||
                        GetString(manifest, "kind") != "EPOCH_CONNECTION_ISOLATED_TEST_PAIR" ||
                        GetString(manifest, "branch") != EpochBranch ||
                        GetString(manifest, "git_sha") != sha ||
                        GetLong(manifest, "target_build") != 12340 ||
                        GetString(manifest, "arch") != "x86" ||
                        GetString(manifest, "test_pair_gate") != "PASS" ||
                        GetString(manifest, "pe_integrity") != "PASS" ||
                        GetString(manifest, "final_package") != "NOT_RUN" ||
                        GetBool(manifest, "in_game_verified") ||
                        GetBool(manifest, "network_behavior_verified") ||
                        GetString(manifest, "startup_import") != "Wow335Loader.dll:#1" ||
                        GetString(manifest, "original_epoch_sha256") != EpochOriginalSha ||
                        GetLong(manifest, "original_epoch_size") != 160768 ||
                        GetString(manifest, "client_exe_sha256") != UpdaterBuildInfo.PinnedClientSha256)
                        throw new InvalidOperationException("Epoch TEST: niespójny manifest Windows CI.");
                    var order = AsArray(GetValue(manifest, "module_load_order"));
                    if (order.Length != 0)
                        throw new InvalidOperationException("Epoch TEST: brak certyfikowanych zależnych modułów; odrzucam niepustą listę.");
                    var files = AsDictionary(GetValue(manifest, "files"));
                    if (files.Count != 2)
                        throw new InvalidOperationException("Epoch TEST: oczekiwano dokładnie dwóch DLL.");
                    epoch = ReadEntry(EpochZipEntry(zip, EpochDll));
                    loader = ReadEntry(EpochZipEntry(zip, EpochLoader));
                    epochSha = GetString(AsDictionary(GetValue(files, EpochDll)), "sha256");
                    loaderSha = GetString(AsDictionary(GetValue(files, EpochLoader)), "sha256");
                    if (!UpdaterSafety.IsSha256Hex(epochSha) || !UpdaterSafety.IsSha256Hex(loaderSha) ||
                        GetLong(AsDictionary(GetValue(files, EpochDll)), "size") != epoch.Length ||
                        GetLong(AsDictionary(GetValue(files, EpochLoader)), "size") != loader.Length ||
                        !string.Equals(Sha256(epoch), epochSha, StringComparison.OrdinalIgnoreCase) ||
                        !string.Equals(Sha256(loader), loaderSha, StringComparison.OrdinalIgnoreCase))
                        throw new InvalidOperationException("Epoch TEST: niezgodny SHA256 pobranych DLL.");
                    EpochPE32(epoch); EpochPE32(loader);
                }
                if (MessageBox.Show(this,
                    "Izolowany eksperyment loadera. Windows CI sprawdził parę PE32 x86, ale działanie tunelu logowania i świata nie zostało zweryfikowane w grze.\n\n" +
                    "Podmiana wyłącznie EpochConnection.dll po wykonaniu kopii oryginału, instalacja Wow335Loader.dll i pustego dlls.txt. Wow.exe pozostaje bez zmian.\n\n" +
                    "SHA: " + sha + "\nZainstalować wariant TEST?",
                    "EpochConnection TEST", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes)
                {
                    statusText = "Epoch TEST: anulowano.";
                    return;
                }
                // Re-check after user confirmation and download, prior to any write.
                if (IsGameRunning(root)) throw new InvalidOperationException("Gra została uruchomiona; instalacja zablokowana.");
                EpochCheckFile(Path.Combine(root, "Wow.exe"), UpdaterBuildInfo.PinnedClientSha256);
                EpochCheckFile(Path.Combine(root, EpochDll), EpochOriginalSha);
                EpochNotLink(loaderPath); EpochNotLink(listPath);
                if (File.Exists(loaderPath) || EpochInstalled(root) != null ||
                    (File.Exists(listPath) && !string.IsNullOrWhiteSpace(File.ReadAllText(listPath, Encoding.UTF8))))
                    throw new InvalidOperationException("Epoch TEST: pliki klienta zmieniły się w czasie pobierania.");
                Directory.CreateDirectory(manager);
                var backupId = Guid.NewGuid().ToString("N");
                var backups = Path.Combine(manager, "backups");
                EpochNotLink(backups);
                Directory.CreateDirectory(backups);
                var backup = Path.Combine(backups, backupId);
                Directory.CreateDirectory(backup);
                var staged = Path.Combine(manager, "stage-" + backupId);
                Directory.CreateDirectory(staged);
                var backupEpoch = Path.Combine(backup, EpochDll);
                var previousList = File.Exists(listPath);
                var dllSha = previousList ? Sha256File(listPath) : Sha256(new byte[0]);
                bool touched = false;
                try
                {
                    File.Copy(Path.Combine(root, EpochDll), backupEpoch);
                    EpochCheckFile(backupEpoch, EpochOriginalSha);
                    if (previousList) File.Copy(listPath, Path.Combine(backup, "dlls.txt"));
                    File.WriteAllBytes(Path.Combine(staged, EpochDll), epoch);
                    File.WriteAllBytes(Path.Combine(staged, EpochLoader), loader);
                    EpochCheckFile(Path.Combine(staged, EpochDll), epochSha);
                    EpochCheckFile(Path.Combine(staged, EpochLoader), loaderSha);
                    File.Copy(Path.Combine(staged, EpochLoader), loaderPath);
                    touched = true;
                    EpochCheckFile(loaderPath, loaderSha);
                    EpochStageFile(Path.Combine(staged, EpochDll), Path.Combine(root, EpochDll), epochSha);
                    if (!previousList) File.WriteAllBytes(listPath, new byte[0]);
                    EpochCheckFile(listPath, dllSha);
                    var installed = new Dictionary<string, object> {
                        { "kind", "EPOCH_CONNECTION_ISOLATED_TEST_PAIR" },
                        { "branch", EpochBranch }, { "git_sha", sha },
                        { "backup_id", backupId }, { "epoch_sha256", epochSha },
                        { "loader_sha256", loaderSha }, { "dlls_sha256", dllSha },
                        { "dlls_existed", previousList }, { "module_load_order", new string[0] }
                    };
                    UpdaterSafety.WriteUtf8Atomic(EpochStatePath(root), json.Serialize(installed),
                        ".stage", ".previous");
                    statusText = "Epoch TEST zainstalowany: " + sha.Substring(0, 12) +
                        " • brak aktywnych DLL gry • wymagany test w grze.";
                    Log(statusText);
                }
                catch
                {
                    // Roll back this transaction only. Never touch unrelated game files.
                    if (touched)
                    {
                        var installedEpoch = Path.Combine(root, EpochDll);
                        if (File.Exists(backupEpoch) &&
                            (!File.Exists(installedEpoch) ||
                            string.Equals(Sha256File(installedEpoch), epochSha, StringComparison.OrdinalIgnoreCase)))
                            File.Copy(backupEpoch, installedEpoch, true);
                        if (File.Exists(loaderPath) &&
                            string.Equals(Sha256File(loaderPath), loaderSha, StringComparison.OrdinalIgnoreCase))
                            File.Delete(loaderPath);
                        if (!previousList && File.Exists(listPath) &&
                            string.Equals(Sha256File(listPath), dllSha, StringComparison.OrdinalIgnoreCase))
                            File.Delete(listPath);
                    }
                    throw;
                }
                finally
                {
                    if (Directory.Exists(staged)) Directory.Delete(staged, true);
                }
            }
            catch (Exception ex)
            {
                Log("Epoch TEST: BŁĄD " + ex.Message);
                MessageBox.Show(this, ex.Message, "EpochConnection TEST", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally
            {
                if (acquired) SetBusy(false, statusText);
            }
        }

        private void EpochRollback()
        {
            if (busy) return;
            try
            {
                var root = Path.GetFullPath(gameDir.Text.Trim());
                if (IsGameRunning(root)) throw new InvalidOperationException("Zamknij grę przed rollbackiem.");
                var state = EpochInstalled(root);
                if (state == null) throw new InvalidOperationException("Nie ma zarządzanej instalacji Epoch TEST.");
                var manager = EpochManager(root);
                var backup = Path.Combine(manager, "backups", GetString(state, "backup_id"));
                EpochNotLink(Path.Combine(manager, "backups"));
                EpochNotLink(backup);
                var orig = Path.Combine(backup, EpochDll);
                var epoch = Path.Combine(root, EpochDll);
                var loader = Path.Combine(root, EpochLoader);
                var list = Path.Combine(root, "dlls.txt");
                EpochCheckFile(orig, EpochOriginalSha);
                EpochCheckFile(epoch, GetString(state, "epoch_sha256"));
                EpochCheckFile(loader, GetString(state, "loader_sha256"));
                EpochCheckFile(list, GetString(state, "dlls_sha256"));
                bool existed = GetBool(state, "dlls_existed");
                if (existed) EpochCheckFile(Path.Combine(backup, "dlls.txt"),
                    GetString(state, "dlls_sha256"));
                // Preserve any local modifications instead of overwriting them.
                EpochStageFile(orig, epoch, EpochOriginalSha);
                File.Delete(loader);
                if (existed)
                    EpochStageFile(Path.Combine(backup, "dlls.txt"), list,
                        GetString(state, "dlls_sha256"));
                else File.Delete(list);
                File.Delete(EpochStatePath(root));
                Log("Epoch TEST: przywrócono oryginalną DLL " + EpochOriginalSha);
                status.Text = "Epoch TEST: rollback zakończony.";
            }
            catch (Exception ex)
            {
                Log("Epoch TEST rollback: BŁĄD " + ex.Message);
                MessageBox.Show(this, ex.Message, "EpochConnection rollback",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void EpochModules()
        {
            try
            {
                var root = Path.GetFullPath(gameDir.Text.Trim());
                var state = EpochInstalled(root);
                if (state == null)
                    throw new InvalidOperationException("Zainstaluj najpierw zweryfikowany wariant Epoch TEST.");
                var order = AsArray(GetValue(state, "module_load_order"));
                // Only registry-backed, CI-verified modules may be exposed for enabling.
                // Current runtime/current.json is empty: arbitrary local DLLs are prohibited.
                if (order.Length != 0)
                    throw new InvalidOperationException("Nieobsługiwany zestaw modułów: wymagana nowa kontrola zależności.");
                EpochCheckFile(Path.Combine(root, "dlls.txt"), GetString(state, "dlls_sha256"));
                MessageBox.Show(this,
                    "Aktywne moduły DLL: 0.\n\n" +
                    "runtime/current.json nie deklaruje jeszcze modułów gry. " +
                    "dlls.txt jest pusty; moduł sieciowy EpochConnection.dll i bootstrap " +
                    "Wow335Loader.dll są ładowane przez import PE, nie przez dlls.txt.\n\n" +
                    "Dowolnych lokalnych DLL nie wolno aktywować bez manifestu SHA256 i weryfikacji zależności.",
                    "Epoch TEST — moduły", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, ex.Message, "Epoch TEST — moduły",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void EpochValidateLaunch(string root)
        {
            var state = EpochInstalled(root);
            if (state == null) return;
            EpochCheckFile(Path.Combine(root, "Wow.exe"), UpdaterBuildInfo.PinnedClientSha256);
            EpochCheckFile(Path.Combine(root, EpochDll), GetString(state, "epoch_sha256"));
            EpochCheckFile(Path.Combine(root, EpochLoader), GetString(state, "loader_sha256"));
            EpochCheckFile(Path.Combine(root, "dlls.txt"), GetString(state, "dlls_sha256"));
            if (AsArray(GetValue(state, "module_load_order")).Length != 0)
                throw new InvalidOperationException("Epoch TEST: moduły nie mają zatwierdzonego zestawu zależności.");
        }
    }
}
