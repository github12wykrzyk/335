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
        // Exact registered work TEST snapshot currently installed in the user's updater.
        // This experiment neither rebuilds nor overwrites its AutoLoot module.
        private const string EpochWorkAutoLootCommit = "149a2523f8068e407a8fc74e058b4f24c81d21e5";
        private const string EpochWorkAutoLootSha = "52bc5100d46c6edaa172ab5342d565454ea6629d7a0ec09bafc044f8f0f2b47e";
        private const string EpochWorkAutoLoot = "AutoLoot335.dll";
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
                !UpdaterSafety.IsGitCommitSha(GetString(state, "git_sha")) ||
                !UpdaterSafety.IsSha256Hex(GetString(state, "epoch_sha256")) ||
                !UpdaterSafety.IsSha256Hex(GetString(state, "loader_sha256")) ||
                !UpdaterSafety.IsSha256Hex(GetString(state, "dlls_sha256")))
                throw new InvalidOperationException("Epoch TEST: uszkodzony manifest lokalnej instalacji.");
            var id = GetString(state, "backup_id");
            if (id.Length != 32 || id.Any(c => "0123456789abcdef".IndexOf(c) < 0))
                throw new InvalidOperationException("Epoch TEST: niewłaściwy identyfikator kopii.");
            var entries = AsArray(GetValue(state, "module_load_order"));
            if (entries.Length > 1 || (entries.Length == 1 &&
                !string.Equals(Convert.ToString(entries[0]), EpochWorkAutoLoot,
                               StringComparison.Ordinal)))
                throw new InvalidOperationException("Epoch TEST: nieznany zestaw aktywnych DLL.");
            if (entries.Length == 1 &&
                (!string.Equals(GetString(state, "module_sha256"), EpochWorkAutoLootSha,
                    StringComparison.OrdinalIgnoreCase) ||
                 GetString(state, "source_commit") != EpochWorkAutoLootCommit))
                throw new InvalidOperationException("Epoch TEST: niezgodna proweniencja AutoLoot.");
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

        private string[] EpochExistingManagedOrder(string root, string listPath)
        {
            EpochNotLink(listPath);
            if (!File.Exists(listPath)) return new string[0];
            var text = File.ReadAllText(listPath, Encoding.UTF8);
            var order = text.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries)
                .Select(x => x.Trim()).Where(x => x.Length != 0).ToArray();
            if (order.Length == 0) return order;
            if (order.Length != 1 ||
                !string.Equals(order[0], EpochWorkAutoLoot, StringComparison.Ordinal))
                throw new InvalidOperationException(
                    "Epoch TEST: lista zawiera nieobsługiwane DLL. Nie nadpisuję jej ani nie uruchamiam.");
            var state = ReadInstalledState();
            if (state == null || GetString(state, "channel") != "test" ||
                GetString(state, "head_sha") != EpochWorkAutoLootCommit ||
                GetLong(state, "schema_version") != 2)
                throw new InvalidOperationException(
                    "Epoch TEST: AutoLoot musi pochodzić z dokładnego zweryfikowanego commita work.");
            var managed = AsArray(GetValue(state, "managed_files"))
                .Select(x => Convert.ToString(x)).ToArray();
            if (!managed.Contains(EpochWorkAutoLoot, StringComparer.OrdinalIgnoreCase) ||
                !managed.Contains("dlls.txt", StringComparer.OrdinalIgnoreCase))
                throw new InvalidOperationException("Epoch TEST: AutoLoot / dlls.txt nie są zarządzane przez updater.");
            // The first updater installations of this same pinned work run may
            // predate managed_sha256. Their file identities are reconstructed
            // from exact on-disk hashes, not from a guessed file name. Other
            // work versions or ambiguous/malformed manifests remain blocked.
            var hashValue = GetValue(state, "managed_sha256");
            var hashes = hashValue as Dictionary<string, object>;
            if (hashValue != null && hashes == null)
                throw new InvalidOperationException("Epoch TEST: nieprawidłowy format manifestu SHA256 work.");
            EpochCheckFile(Path.Combine(root, EpochWorkAutoLoot), EpochWorkAutoLootSha);
            if (hashes != null)
            {
                if (!string.Equals(GetString(hashes, EpochWorkAutoLoot), EpochWorkAutoLootSha,
                        StringComparison.OrdinalIgnoreCase) ||
                    !string.Equals(GetString(hashes, "dlls.txt"), Sha256File(listPath),
                        StringComparison.OrdinalIgnoreCase))
                    throw new InvalidOperationException("Epoch TEST: manifest work nie zgadza się z dlls.txt / AutoLoot.");
            }
            else
            {
                // Pin legacy acceptance to the precise, successful work run;
                // reject any other state that has no per-file SHA256 evidence.
                if (GetLong(state, "run_id") != 35855470746L ||
                    GetString(state, "artifact_name") !=
                        "WoW335-WORK-CANDIDATE-" + EpochWorkAutoLootCommit)
                    throw new InvalidOperationException(
                        "Epoch TEST: starszy stan updatera nie ma zaufanego manifestu SHA256.");
                Log("Epoch TEST: starszy manifest work bez managed_sha256; " +
                    "potwierdzono SHA256 AutoLoot335.dll i zachowano istniejący dlls.txt.");
            }
            return order;
        }

        // Same live-HEAD and latest-success gate used by installation, without
        // downloading or changing any game file. The three primary UI actions
        // share this source of truth.
        private async Task<Tuple<string, long>> EpochLatestAsync()
        {
            using (var client = CreateClient())
            {
                var branch = AsDictionary(json.DeserializeObject(
                    await GetStringAsync(client, ApiRoot + "/branches/" + EpochBranch)));
                var sha = GetString(AsDictionary(GetValue(branch, "commit")), "sha");
                if (!UpdaterSafety.IsGitCommitSha(sha))
                    throw new InvalidOperationException("Brak prawidłowego HEAD Epoch TEST.");
                var runs = AsArray(GetValue(AsDictionary(json.DeserializeObject(
                    await GetStringAsync(client, ApiRoot + "/actions/runs?branch=" +
                        EpochBranch + "&per_page=50"))), "workflow_runs"));
                var run = UpdaterSafety.RequireLatestSuccessfulRun(runs, EpochWorkflow, EpochBranch);
                if (!string.Equals(GetString(run, "head_sha"), sha, StringComparison.Ordinal))
                    throw new InvalidOperationException("CI Epoch TEST nie dotyczy aktualnego HEAD.");
                var id = GetLong(run, "id");
                var artifacts = AsArray(GetValue(AsDictionary(json.DeserializeObject(
                    await GetStringAsync(client, ApiRoot + "/actions/runs/" + id +
                        "/artifacts?per_page=100"))), "artifacts"));
                var found = artifacts.Select(AsDictionary).Any(a =>
                    !GetBool(a, "expired") &&
                    string.Equals(GetString(a, "name"),
                        "EpochConnection-TEST-PAIR-" + sha, StringComparison.Ordinal));
                if (!found)
                    throw new InvalidOperationException("Brak zweryfikowanej pary DLL dla bieżącego SHA.");
                return Tuple.Create(sha, id);
            }
        }

        private async Task<bool> EpochInstallAsync()
        {
            if (busy) return false;
            bool completed = false;
            string statusText = "Epoch TEST: nie zainstalowano.";
            string stage = "wstępna kontrola lokalnych plików";
            bool acquired = false;
            try
            {
                var root = Path.GetFullPath(gameDir.Text.Trim());
                if (!Directory.Exists(root)) throw new InvalidOperationException("Wybierz katalog WoW.");
                if (IsGameRunning(root)) throw new InvalidOperationException("Zamknij WoW przed zmianą DLL.");
                EpochCheckFile(Path.Combine(root, "Wow.exe"), UpdaterBuildInfo.PinnedClientSha256);
                var previousEpochState = EpochInstalled(root);
                if (previousEpochState == null)
                    EpochCheckFile(Path.Combine(root, EpochDll), EpochOriginalSha);
                else
                    EpochValidateLaunch(root);
                if (string.IsNullOrWhiteSpace(token.Text))
                    throw new InvalidOperationException("Wymagany token GitHub Contents: Read i Actions: Read.");
                var manager = EpochManager(root);
                var loaderPath = Path.Combine(root, EpochLoader);
                var listPath = Path.Combine(root, "dlls.txt");
                EpochNotLink(loaderPath); EpochNotLink(listPath);
                if (previousEpochState == null && File.Exists(loaderPath))
                    throw new InvalidOperationException("Epoch TEST: kolizja z istniejącym Wow335Loader.dll.");
                // Validate a previously installed *managed* work AutoLoot, not an
                // arbitrary user-provided filename. Leave its DLL and list unchanged.
                var activeOrder = EpochExistingManagedOrder(root, listPath);
                stage = "zapytanie o HEAD gałęzi Epoch na GitHubie";
                SetBusy(true, "Epoch TEST: sprawdzam aktualny HEAD i Windows CI...");
                acquired = true;
                string sha, archiveUrl;
                using (var client = CreateClient())
                {
                    var branch = AsDictionary(json.DeserializeObject(
                        await GetStringAsync(client, ApiRoot + "/branches/" + EpochBranch)));
                    sha = GetString(AsDictionary(GetValue(branch, "commit")), "sha");
                    if (!UpdaterSafety.IsGitCommitSha(sha))
                        throw new InvalidOperationException("Epoch TEST: brak aktualnego SHA brancha.");
                    stage = "pobieranie listy workflow Epoch";
                    var runs = AsArray(GetValue(AsDictionary(json.DeserializeObject(
                        await GetStringAsync(client, ApiRoot + "/actions/runs?branch=" + EpochBranch + "&per_page=50"))),
                        "workflow_runs"));
                    var run = UpdaterSafety.RequireLatestSuccessfulRun(runs, EpochWorkflow, EpochBranch);
                    if (!string.Equals(GetString(run, "head_sha"), sha, StringComparison.Ordinal))
                        throw new InvalidOperationException("Epoch TEST: udany CI jest dla starszego SHA.");
                    var runId = GetLong(run, "id");
                    stage = "odczyt artefaktów workflow Epoch";
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
                stage = "pobieranie ZIP testowej pary DLL";
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
                    stage = "manifest ZIP testowej pary DLL";
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

                if (previousEpochState != null &&
                    string.Equals(GetString(previousEpochState, "epoch_sha256"), epochSha,
                        StringComparison.OrdinalIgnoreCase) &&
                    string.Equals(GetString(previousEpochState, "loader_sha256"), loaderSha,
                        StringComparison.OrdinalIgnoreCase))
                {
                    EpochValidateLaunch(root);
                    if (GetString(previousEpochState, "git_sha") != sha)
                    {
                        previousEpochState["git_sha"] = sha;
                        UpdaterSafety.WriteUtf8Atomic(EpochStatePath(root), json.Serialize(previousEpochState),
                            ".stage", ".previous");
                        Log("Epoch TEST: zweryfikowano nowe CI; bajty DLL bez zmian.");
                    }
                    statusText = "Aktualne: Epoch TEST " + sha.Substring(0, 12) +
                        " • aktywne moduły: " + activeOrder.Length;
                    Log(statusText);
                    completed = true;
                    return true;
                }
                stage = "weryfikacja stanu przed instalacją";
                // Re-check after user confirmation and download, prior to any write.
                if (IsGameRunning(root)) throw new InvalidOperationException("Gra została uruchomiona; instalacja zablokowana.");
                EpochCheckFile(Path.Combine(root, "Wow.exe"), UpdaterBuildInfo.PinnedClientSha256);
                if (previousEpochState == null)
                    EpochCheckFile(Path.Combine(root, EpochDll), EpochOriginalSha);
                else
                    EpochValidateLaunch(root);
                EpochNotLink(loaderPath); EpochNotLink(listPath);
                if ((previousEpochState == null && File.Exists(loaderPath)) ||
                    (previousEpochState != null && EpochInstalled(root) == null))
                    throw new InvalidOperationException("Epoch TEST: pliki klienta zmieniły się w czasie pobierania.");
                var checkedOrder = EpochExistingManagedOrder(root, listPath);
                if (!activeOrder.SequenceEqual(checkedOrder, StringComparer.Ordinal))
                    throw new InvalidOperationException("Epoch TEST: kolejność DLL zmieniła się w czasie pobierania.");
                if (previousEpochState != null)
                {
                    // Update ONLY the verified TEST pair. Preserve original backup,
                    // managed work AutoLoot and exact dlls.txt; restore prior pair on failure.
                    var backupIdExisting = GetString(previousEpochState, "backup_id");
                    var backupOriginal = Path.Combine(manager, "backups", backupIdExisting, EpochDll);
                    EpochCheckFile(backupOriginal, EpochOriginalSha);
                    var transactional = Path.Combine(manager, "replace-" + Guid.NewGuid().ToString("N"));
                    Directory.CreateDirectory(transactional);
                    var oldEpoch = Path.Combine(transactional, "oldEpoch.dll");
                    var oldLoader = Path.Combine(transactional, "oldLoader.dll");
                    var nextEpoch = Path.Combine(transactional, EpochDll);
                    var nextLoader = Path.Combine(transactional, EpochLoader);
                    bool restoreCompleted = false;
                    bool loaderReplaced = false;
                    bool epochReplaced = false;
                    try
                    {
                        File.Copy(Path.Combine(root, EpochDll), oldEpoch);
                        File.Copy(loaderPath, oldLoader);
                        EpochCheckFile(oldEpoch, GetString(previousEpochState, "epoch_sha256"));
                        EpochCheckFile(oldLoader, GetString(previousEpochState, "loader_sha256"));
                        File.WriteAllBytes(nextEpoch, epoch);
                        File.WriteAllBytes(nextLoader, loader);
                        EpochCheckFile(nextEpoch, epochSha);
                        EpochCheckFile(nextLoader, loaderSha);
                        loaderReplaced = true;
                        EpochStageFile(nextLoader, loaderPath, loaderSha);
                        epochReplaced = true;
                        EpochStageFile(nextEpoch, Path.Combine(root, EpochDll), epochSha);
                        EpochCheckFile(Path.Combine(root, "dlls.txt"),
                            GetString(previousEpochState, "dlls_sha256"));
                        previousEpochState["git_sha"] = sha;
                        previousEpochState["epoch_sha256"] = epochSha;
                        previousEpochState["loader_sha256"] = loaderSha;
                        UpdaterSafety.WriteUtf8Atomic(EpochStatePath(root),
                            json.Serialize(previousEpochState), ".stage", ".previous");
                        statusText = "Aktualizacja Epoch TEST: " + sha.Substring(0, 12) +
                            " • moduły: " + activeOrder.Length;
                        Log(statusText);
                        completed = true;
                        restoreCompleted = true;
                    }
                    catch
                    {
                        if (epochReplaced)
                        {
                            var target = Path.Combine(root, EpochDll);
                            var oldHash = Sha256File(oldEpoch);
                            if (!File.Exists(target) ||
                                string.Equals(Sha256File(target), epochSha, StringComparison.OrdinalIgnoreCase))
                                EpochStageFile(oldEpoch, target, oldHash);
                            else
                                EpochCheckFile(target, oldHash);
                        }
                        if (loaderReplaced)
                        {
                            var oldHash = Sha256File(oldLoader);
                            if (!File.Exists(loaderPath) ||
                                string.Equals(Sha256File(loaderPath), loaderSha, StringComparison.OrdinalIgnoreCase))
                                EpochStageFile(oldLoader, loaderPath, oldHash);
                            else
                                EpochCheckFile(loaderPath, oldHash);
                        }
                        restoreCompleted = true;
                        throw;
                    }
                    finally
                    {
                        if (restoreCompleted) Directory.Delete(transactional, true);
                    }
                    return completed;
                }
                stage = "instalacja i backup plików zarządzanych";
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
                        { "dlls_existed", previousList }, { "module_load_order", activeOrder },
                        { "module_sha256", activeOrder.Length == 1 ? EpochWorkAutoLootSha : "" },
                        { "source_commit", activeOrder.Length == 1 ? EpochWorkAutoLootCommit : "" }
                    };
                    UpdaterSafety.WriteUtf8Atomic(EpochStatePath(root), json.Serialize(installed),
                        ".stage", ".previous");
                    statusText = "Epoch TEST: " + sha.Substring(0, 12) +
                        " • aktywne moduły: " + activeOrder.Length + " • wymagany test w grze.";
                    Log(statusText);
                    completed = true;
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
                statusText = "Aktualizacja nie powiodła się: Epoch TEST";
                Log("Epoch TEST: BŁĄD [" + stage + "] " + ex.Message);
                MessageBox.Show(this, "Etap: " + stage + "\n" + ex.Message,
                    "EpochConnection TEST", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally
            {
                if (acquired) SetBusy(false, statusText);
            }
            return completed;
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
                EpochValidateLaunch(root);
                MessageBox.Show(this,
                    "Aktywne moduły DLL: " + order.Length + ".\n\n" +
                    (order.Length == 1 ? "1. AutoLoot335.dll — zarejestrowany TEST work/" +
                        EpochWorkAutoLootCommit.Substring(0, 12) + ", SHA256 " + EpochWorkAutoLootSha +
                        ".\n\n" : "Lista dlls.txt jest pusta.\n\n") +
                    "EpochConnection.dll i Wow335Loader.dll są ładowane przez import PE. " +
                    "Inne moduły nie są włączane bez odrębnej kontroli SHA256 oraz zależności.",
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
            var entries = AsArray(GetValue(state, "module_load_order"));
            if (entries.Length == 1)
            {
                EpochCheckFile(Path.Combine(root, EpochWorkAutoLoot), EpochWorkAutoLootSha);
                var current = EpochExistingManagedOrder(root, Path.Combine(root, "dlls.txt"));
                if (current.Length != 1 ||
                    !string.Equals(current[0], EpochWorkAutoLoot, StringComparison.Ordinal))
                    throw new InvalidOperationException("Epoch TEST: zestaw AutoLoot zmienił się.");
            }
            else if (entries.Length != 0)
                throw new InvalidOperationException("Epoch TEST: nieznany zestaw DLL.");
        }
    }
}
