using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;

namespace WoW335Updater
{
    internal sealed partial class MainForm
    {
        private const string RegisteredAutoLootDll = "AutoLoot335.dll";
        private const string RegisteredAutoLootLoader = "AutoLoot335_Launcher.exe";
        private const string RegisteredAutoLootResource = "AutoLootRuntime.Launcher.exe";

        private static byte[] InstalledLoaderBytes()
        {
            using (var stream = Assembly.GetExecutingAssembly()
                .GetManifestResourceStream(RegisteredAutoLootResource))
            {
                if (stream == null || stream.Length < 512 || stream.Length > 1024 * 1024)
                    throw new InvalidOperationException(
                        "Updater nie zawiera zweryfikowanego x86 launchera AutoLoot.");
                using (var output = new MemoryStream())
                {
                    stream.CopyTo(output);
                    var bytes = output.ToArray();
                    NativeCheckX86(bytes, false);
                    return bytes;
                }
            }
        }

        /* A normal registered module is loaded ONLY if the complete game
         * candidate has been installed through this updater with exact
         * SHA256 evidence. Never silently fall back to launching Wow.exe
         * without its managed active DLLs. */
        private bool TryLaunchInstalledAutoLoot(string root, string exe,
                                                Dictionary<string, object> state)
        {
            if (state == null) return false;
            var names = AsArray(GetValue(state, "managed_files"))
                .Select(x => Convert.ToString(x)).ToArray();
            if (!names.Contains(RegisteredAutoLootDll, StringComparer.OrdinalIgnoreCase))
                return false;
            var installed = AsDictionary(GetValue(state, "managed_sha256"));
            if (installed == null || !names.Contains("dlls.txt", StringComparer.OrdinalIgnoreCase))
                throw new InvalidOperationException(
                    "Brak potwierdzonego manifestu DLL. W updaterze wykonaj Sprawdź / napraw.");
            root = Path.GetFullPath(root);
            exe = Path.GetFullPath(exe);
            if (!string.Equals(Path.GetFileName(exe), "Wow.exe", StringComparison.OrdinalIgnoreCase) ||
                !string.Equals(Sha256File(exe), UpdaterBuildInfo.PinnedClientSha256,
                               StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("Niewłaściwy Wow.exe dla AutoLoot build 12340.");
            if (IsGameRunning(root))
                throw new InvalidOperationException(
                    "WoW już działa. Zamknij grę przed uruchomieniem aktywnych DLL.");

            var dll = SafeDestination(root, RegisteredAutoLootDll);
            var order = SafeDestination(root, "dlls.txt");
            if (!File.Exists(dll) || !File.Exists(order) ||
                !UpdaterSafety.IsSha256Hex(GetString(installed, RegisteredAutoLootDll)) ||
                !string.Equals(Sha256File(dll), GetString(installed, RegisteredAutoLootDll),
                               StringComparison.OrdinalIgnoreCase) ||
                !string.Equals(Sha256File(order), GetString(installed, "dlls.txt"),
                               StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException(
                    "AutoLoot335.dll / dlls.txt różnią się od zainstalowanego manifestu. Użyj Sprawdź / napraw.");

            var listed = File.ReadAllLines(order).Where(x => !string.IsNullOrWhiteSpace(x))
                .Select(x => x.Trim()).ToArray();
            if (listed.Length != 1 || !string.Equals(listed[0], RegisteredAutoLootDll,
                                                   StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException(
                    "Inny aktywny zestaw DLL; launcher AutoLoot nie uruchomi niezgodnej konfiguracji.");

            var bytes = InstalledLoaderBytes();
            var loaderHash = Sha256(bytes);
            var manager = Path.Combine(root, ".wow335_updater");
            var loaderDir = Path.Combine(manager, "native_loader");
            NativeRejectReparse(manager);
            NativeRejectReparse(loaderDir);
            Directory.CreateDirectory(loaderDir);
            var loader = Path.Combine(loaderDir, RegisteredAutoLootLoader);
            NativeRejectReparse(loader);
            if (!File.Exists(loader) ||
                !string.Equals(Sha256File(loader), loaderHash, StringComparison.OrdinalIgnoreCase))
            {
                var stage = loader + "." + Guid.NewGuid().ToString("N") + ".tmp";
                try
                {
                    File.WriteAllBytes(stage, bytes);
                    if (!string.Equals(Sha256File(stage), loaderHash,
                                       StringComparison.OrdinalIgnoreCase))
                        throw new IOException("Launcher AutoLoot nie przeszedł weryfikacji SHA256.");
                    UpdaterSafety.ReplaceFile(stage, loader, ".native_loader.bak");
                }
                finally { if (File.Exists(stage)) File.Delete(stage); }
            }
            if (!string.Equals(Sha256File(loader), loaderHash, StringComparison.OrdinalIgnoreCase))
                throw new IOException("Launcher AutoLoot różni się od wbudowanego artefaktu.");

            var start = new ProcessStartInfo(loader)
            {
                Arguments = "--exe \"" + exe.Replace("\"", "") + "\"",
                WorkingDirectory = loaderDir,
                UseShellExecute = false
            };
            using (var running = System.Diagnostics.Process.Start(start))
            {
                if (running == null) throw new IOException("Nie uruchomiono game-thread launchera AutoLoot.");
                Log("Uruchomiono zarejestrowany AutoLoot335.dll z pakietu " +
                    ShortSha(GetString(state, "head_sha")) + " (launcher PID " + running.Id + ").");
            }
            return true;
        }
    }
}
