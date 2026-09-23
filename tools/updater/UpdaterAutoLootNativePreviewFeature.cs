using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace WoW335Updater
{
    internal sealed partial class MainForm
    {
        private readonly Button autoLootNativePreviewButton = new Button();
        private const string NativeBranch = "feature/autoloot-12340";
        private const string NativeWorkflow = "Build isolated 12340 AutoLoot native host";
        private const string NativeHostFile = "AutoLoot335_Host_UNREGISTERED.dll";
        private const string NativeLauncherFile = "AutoLoot335_Launcher_PREVIEW.exe";

        private static void NativeRejectReparse(string path)
        {
            if ((Directory.Exists(path) || File.Exists(path)) &&
                (File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0)
                throw new InvalidOperationException(
                    "Native AutoLoot: odmowa użycia dowiązania " + Path.GetFileName(path));
        }

        private static ZipArchiveEntry NativeEntry(ZipArchive zip, string name)
        {
            var hits = zip.Entries.Where(e =>
                string.Equals(e.FullName, name, StringComparison.Ordinal)).ToArray();
            if (hits.Length != 1 || hits[0].Length > 3 * 1024 * 1024 ||
                !string.Equals(hits[0].Name, hits[0].FullName, StringComparison.Ordinal))
                throw new InvalidOperationException(
                    "Native AutoLoot: artefakt nie zawiera pojedynczego bezpiecznego pliku " + name);
            return hits[0];
        }

        private static void NativeCheckX86(byte[] payload, bool isDll)
        {
            if (payload.Length < 512 || payload[0] != 'M' || payload[1] != 'Z')
                throw new InvalidOperationException("Native AutoLoot: brak nagłówka MZ.");
            var pe = BitConverter.ToInt32(payload, 0x3c);
            if (pe < 64 || pe > payload.Length - 28 ||
                BitConverter.ToInt32(payload, pe) != 0x00004550 ||
                BitConverter.ToUInt16(payload, pe + 4) != 0x014c ||
                BitConverter.ToUInt16(payload, pe + 24) != 0x010b ||
                ((BitConverter.ToUInt16(payload, pe + 22) & 0x2000) != 0) != isDll)
                throw new InvalidOperationException("Native AutoLoot: moduł nie jest oczekiwanym PE32 x86.");
        }

        private async Task NativeAutoLootPreviewAsync()
        {
            if (busy) return;
            var root = gameDir.Text.Trim();
            var lastStatus = "Gotowy";
            try
            {
                if (!Directory.Exists(root))
                    throw new InvalidOperationException("Wybierz istniejący katalog WoW 3.3.5a.");
                root = Path.GetFullPath(root);
                var state = ReadInstalledState();
                if (state != null && AsArray(GetValue(state, "managed_files"))
                    .Any(x => Convert.ToString(x).EndsWith(".dll", StringComparison.OrdinalIgnoreCase)))
                    throw new InvalidOperationException(
                        "Zarejestrowane moduły DLL są już w paczce gry. " +
                        "Użyj «Uruchom grę», aby uniknąć podwójnego ładowania AutoLoot.");
                var wow = Path.Combine(root, "Wow.exe");
                if (!File.Exists(wow) ||
                    !string.Equals(Sha256File(wow), UpdaterBuildInfo.PinnedClientSha256,
                                   StringComparison.OrdinalIgnoreCase))
                    throw new InvalidOperationException("Native AutoLoot wymaga dokładnego, wybranego klienta Wow.exe build 12340.");
                if (IsGameRunning(root))
                    throw new InvalidOperationException("Zamknij grę przed uruchomieniem AutoLoot. Nowa sesja wymaga kontrolowanego startu.");
                if (string.IsNullOrWhiteSpace(token.Text))
                    throw new InvalidOperationException("Najpierw zapisz token GitHub z dostępem Contents/Actions do repo 335.");

                SetBusy(true, "Sprawdzanie najnowszego buildu natywnego AutoLoot na GitHubie...");
                string sha, downloadUrl;
                using (var client = CreateClient())
                {
                    var branchData = AsDictionary(json.DeserializeObject(
                        await GetStringAsync(client, ApiRoot + "/branches/" + NativeBranch)));
                    sha = GetString(AsDictionary(GetValue(branchData, "commit")), "sha");
                    if (sha.Length != 40 ||
                        sha.Any(c => "0123456789abcdef".IndexOf(c) < 0))
                        throw new InvalidOperationException("Nieznane SHA eksperymentalnej gałęzi AutoLoot.");
                    var runsRoot = AsDictionary(json.DeserializeObject(
                        await GetStringAsync(client, ApiRoot +
                            "/actions/runs?branch=" + NativeBranch + "&per_page=50")));
                    var run = UpdaterSafety.RequireLatestSuccessfulRun(
                        AsArray(GetValue(runsRoot, "workflow_runs")), NativeWorkflow, NativeBranch);
                    if (!string.Equals(GetString(run, "head_sha"), sha, StringComparison.Ordinal))
                        throw new InvalidOperationException(
                            "Najnowszy native AutoLoot nie pochodzi z aktualnego HEAD eksperymentu. Nie pobieram starszego artefaktu.");
                    var runId = GetLong(run, "id");
                    var artifactRoot = AsDictionary(json.DeserializeObject(
                        await GetStringAsync(client, ApiRoot + "/actions/runs/" + runId + "/artifacts?per_page=100")));
                    var artifactName = "AutoLoot12340-native-preview-TESTONLY-" + sha;
                    var artifact = AsArray(GetValue(artifactRoot, "artifacts"))
                        .Select(AsDictionary)
                        .SingleOrDefault(a => !GetBool(a, "expired") &&
                            string.Equals(GetString(a, "name"), artifactName, StringComparison.Ordinal));
                    if (artifact == null)
                        throw new InvalidOperationException("Najnowszy build nie opublikował zweryfikowanego artefaktu native AutoLoot.");
                    downloadUrl = GetString(artifact, "archive_download_url");
                }

                var outer = await DownloadBytesAsync(downloadUrl);
                byte[] hostBytes, launcherBytes;
                using (var stream = new MemoryStream(outer, false))
                using (var archive = new ZipArchive(stream, ZipArchiveMode.Read, false))
                {
                    if (archive.Entries.Count != 4 ||
                        archive.Entries.Any(e => e.FullName.IndexOfAny(new[] { '/', '\\' }) >= 0 ||
                            e.FullName == "." || e.FullName == ".."))
                        throw new InvalidOperationException("Artefakt AutoLoot ma nieoczekiwany zestaw lub ścieżki plików.");
                    var meta = AsDictionary(json.DeserializeObject(
                        Encoding.UTF8.GetString(ReadEntry(NativeEntry(archive, "native_preview_build.json")))));
                    if (GetLong(meta, "schema_version") != 1 ||
                        GetString(meta, "kind") != "NATIVE_AUTOLOOT_PREVIEW_NOT_GAME_PACKAGE" ||
                        GetString(meta, "branch") != NativeBranch ||
                        GetString(meta, "git_sha") != sha ||
                        GetLong(meta, "target_build") != 12340 ||
                        GetString(meta, "arch") != "x86" ||
                        GetString(meta, "final_package") != "NOT_RUN" ||
                        GetBool(meta, "in_game_verified") ||
                        !string.Equals(GetString(meta, "client_exe_sha256"),
                                       UpdaterBuildInfo.PinnedClientSha256, StringComparison.OrdinalIgnoreCase))
                        throw new InvalidOperationException("Proveniencja i ograniczenia native AutoLoot nie są zgodne.");
                    var files = AsDictionary(GetValue(meta, "files"));
                    if (files.Count != 2) throw new InvalidOperationException("Nieprawidłowy zestaw plików native AutoLoot.");
                    byte[] h = null, l = null;
                    foreach (var name in new[] { NativeHostFile, NativeLauncherFile })
                    {
                        var spec = AsDictionary(GetValue(files, name));
                        var bytes = ReadEntry(NativeEntry(archive, name));
                        if (!UpdaterSafety.IsSha256Hex(GetString(spec, "sha256")) ||
                            !string.Equals(GetString(spec, "sha256"), Sha256(bytes),
                                           StringComparison.OrdinalIgnoreCase) ||
                            GetLong(spec, "size") != bytes.Length)
                            throw new InvalidOperationException("Niezgodny SHA256 natywnego pliku " + name);
                        NativeCheckX86(bytes, name == NativeHostFile);
                        if (name == NativeHostFile) h = bytes; else l = bytes;
                    }
                    hostBytes = h;
                    launcherBytes = l;
                }

                var answer = MessageBox.Show(this,
                    "AutoLoot 3.3.5a — wstępny natywny TEST.\n\n" +
                    "Zostanie uruchomiony wybrany Wow.exe i odrębny moduł natywny z dokładnego SHA " +
                    sha.Substring(0, 12) + ". Moduł używa własnego hooka wątku okna klienta " +
                    "i wywołań 12340. Build x86 i sprawdzenie adresów nie dowodzą działania w grze. " +
                    "Mogą wystąpić błędy, zawieszenie gry lub odrzucenie hooka; " +
                    "testuj tylko na kliencie, który możesz bezpiecznie zamknąć.\n\n" +
                    "Nie zostanie zmieniony Wow.exe, dlls.txt, aktywne DLL ani kanał STABLE. " +
                    "Pliki testowe zostaną umieszczone w katalogu zarządzanym przez updater.\n\n" +
                    "Uruchomić eksperyment?",
                    "WoW335 — natywny test AutoLoot", MessageBoxButtons.YesNo,
                    MessageBoxIcon.Warning);
                if (answer != DialogResult.Yes)
                {
                    lastStatus = "Test natywnego AutoLoot anulowany.";
                    return;
                }

                var manager = Path.Combine(root, ".wow335_updater");
                var baseDir = Path.Combine(manager, "native_preview");
                var versionDir = Path.Combine(baseDir, sha);
                NativeRejectReparse(manager);
                NativeRejectReparse(baseDir);
                NativeRejectReparse(versionDir);
                Directory.CreateDirectory(baseDir);
                if (!Directory.Exists(versionDir))
                {
                    var stage = Path.Combine(baseDir, ".stage-" + Guid.NewGuid().ToString("N"));
                    Directory.CreateDirectory(stage);
                    try
                    {
                        File.WriteAllBytes(Path.Combine(stage, NativeHostFile), hostBytes);
                        File.WriteAllBytes(Path.Combine(stage, NativeLauncherFile), launcherBytes);
                        if (!string.Equals(Sha256File(Path.Combine(stage, NativeHostFile)), Sha256(hostBytes),
                                           StringComparison.OrdinalIgnoreCase) ||
                            !string.Equals(Sha256File(Path.Combine(stage, NativeLauncherFile)), Sha256(launcherBytes),
                                           StringComparison.OrdinalIgnoreCase))
                            throw new InvalidOperationException("Native AutoLoot: weryfikacja plików po zapisie nie powiodła się.");
                        Directory.Move(stage, versionDir);
                    }
                    finally
                    {
                        if (Directory.Exists(stage)) Directory.Delete(stage, true);
                    }
                }
                else
                {
                    foreach (var name in Directory.GetFiles(versionDir)) NativeRejectReparse(name);
                    if (Directory.GetFiles(versionDir).Length != 2 ||
                        Directory.GetDirectories(versionDir).Length != 0)
                        throw new InvalidOperationException("Katalog natywnego eksperymentu został zmodyfikowany — odmowa wykonania.");
                }
                var launcher = Path.Combine(versionDir, NativeLauncherFile);
                var host = Path.Combine(versionDir, NativeHostFile);
                if (!string.Equals(Sha256File(launcher), Sha256(launcherBytes), StringComparison.OrdinalIgnoreCase) ||
                    !string.Equals(Sha256File(host), Sha256(hostBytes), StringComparison.OrdinalIgnoreCase))
                    throw new InvalidOperationException("Native AutoLoot: pliki nie zgadzają się z bieżącym manifestem SHA.");
                var start = new ProcessStartInfo(launcher) {
                    Arguments = "--exe \"" + wow.Replace("\"", "") + "\"",
                    WorkingDirectory = versionDir,
                    UseShellExecute = false
                };
                using (var launched = System.Diagnostics.Process.Start(start))
                {
                    if (launched == null)
                        throw new InvalidOperationException("Nie udało się uruchomić natywnego launchera.");
                    Log("Natywny TEST AutoLoot uruchomiony; branch " + NativeBranch +
                        "; SHA " + sha + "; launcher pid " + launched.Id);
                }
                lastStatus = "Uruchomiono natywny TEST AutoLoot; sprawdź zachowanie w grze.";
            }
            catch (Exception ex)
            {
                lastStatus = "Natywny TEST AutoLoot nie został uruchomiony.";
                Log("AutoLoot TEST: " + ex.Message);
                MessageBox.Show(this, ex.Message, "WoW335 — AutoLoot", MessageBoxButtons.OK,
                                MessageBoxIcon.Error);
            }
            finally
            {
                SetBusy(false, lastStatus);
            }
        }
    }
}
