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
        private const string RegisteredRuntimeLoader = "WoW335RuntimeLoader.exe";
        private const string RegisteredRuntimeResource = "WoW335Runtime.Loader.exe";

        // Shared preflight for the only updater-managed Windows x86 launcher.
        private static void NativeRejectReparse(string path)
        {
            if ((Directory.Exists(path) || File.Exists(path)) &&
                (File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0)
                throw new InvalidOperationException(
                    "Loader 335: odmowa uzycia dowiazania " + Path.GetFileName(path));
        }

        private static void NativeCheckX86(byte[] payload, bool isDll)
        {
            if (payload.Length < 512 || payload[0] != 'M' || payload[1] != 'Z')
                throw new InvalidOperationException("Loader 335: brak naglowka MZ.");
            var pe = BitConverter.ToInt32(payload, 0x3c);
            if (pe < 64 || pe > payload.Length - 28 ||
                BitConverter.ToInt32(payload, pe) != 0x00004550 ||
                BitConverter.ToUInt16(payload, pe + 4) != 0x014c ||
                BitConverter.ToUInt16(payload, pe + 24) != 0x010b ||
                ((BitConverter.ToUInt16(payload, pe + 22) & 0x2000) != 0) != isDll)
                throw new InvalidOperationException("Loader 335: wymagany plik PE32 x86.");
        }

        private static byte[] InstalledLoaderBytes()
        {
            using (var stream = Assembly.GetExecutingAssembly()
                .GetManifestResourceStream(RegisteredRuntimeResource))
            {
                if (stream == null || stream.Length < 512 || stream.Length > 1024 * 1024)
                    throw new InvalidOperationException(
                        "Updater nie zawiera zweryfikowanego uniwersalnego launchera x86.");
                using (var output = new MemoryStream())
                {
                    stream.CopyTo(output);
                    var bytes = output.ToArray();
                    NativeCheckX86(bytes, false);
                    return bytes;
                }
            }
        }

        /* No filename-only injection: an installed exact-SHA package is the
         * sole authority for the module names and their order (dlls.txt).
         * Missing, additional, stale, tampered or non-x86 entries fail closed. */
        private bool TryLaunchInstalledModules(string root, string exe,
                                                Dictionary<string, object> state)
        {
            root = Path.GetFullPath(root);
            UpdaterSafety.RequireNoLegacyEpoch(root);
            exe = Path.GetFullPath(exe);
            var order = SafeDestination(root, "dlls.txt");
            if (state == null)
            {
                if (File.Exists(order))
                    throw new InvalidOperationException(
                        "Wykryto dlls.txt bez stanu zainstalowanej paczki. Wykonaj Sprawdź / napraw.");
                return false;
            }

            var names = AsArray(GetValue(state, "managed_files"))
                .Select(x => Convert.ToString(x)).ToArray();
            var registered = names.Where(x => x != null &&
                x.EndsWith(".dll", StringComparison.OrdinalIgnoreCase)).ToArray();
            if (registered.Length == 0)
            {
                if (File.Exists(order) || names.Contains("dlls.txt", StringComparer.OrdinalIgnoreCase))
                    throw new InvalidOperationException("Manifest DLL jest niekompletny. Wykonaj Sprawdź / napraw.");
                return false;
            }
            var hashes = AsDictionary(GetValue(state, "managed_sha256"));
            if (hashes == null || !names.Contains("dlls.txt", StringComparer.OrdinalIgnoreCase))
                throw new InvalidOperationException(
                    "Brak potwierdzonego manifestu DLL. Wykonaj Sprawdź / napraw.");
            if (!string.Equals(Path.GetFileName(exe), "Wow.exe", StringComparison.OrdinalIgnoreCase) ||
                !string.Equals(Sha256File(exe), UpdaterBuildInfo.PinnedClientSha256,
                               StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("Niewłaściwy Wow.exe dla build 12340.");
            if (IsGameRunning(root))
                throw new InvalidOperationException("WoW już działa. Zamknij grę przed uruchomieniem aktywnych DLL.");

            if (!File.Exists(order) || !UpdaterSafety.IsSha256Hex(GetString(hashes, "dlls.txt")) ||
                !string.Equals(Sha256File(order), GetString(hashes, "dlls.txt"),
                               StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("dlls.txt różni się od zainstalowanego manifestu.");

            var listed = File.ReadAllLines(order).Where(x => !string.IsNullOrWhiteSpace(x))
                .Select(x => x.Trim()).ToArray();
            var expected = new HashSet<string>(registered, StringComparer.OrdinalIgnoreCase);
            if (listed.Length != registered.Length || expected.Count != listed.Length ||
                !expected.SetEquals(listed) ||
                listed.Any(x => x != Path.GetFileName(x) ||
                    !x.EndsWith(".dll", StringComparison.OrdinalIgnoreCase)))
                throw new InvalidOperationException("Nieprawidłowy zestaw lub kolejność DLL. Wykonaj Sprawdź / napraw.");

            foreach (var name in listed)
            {
                var dll = SafeDestination(root, name);
                if (!File.Exists(dll) || !UpdaterSafety.IsSha256Hex(GetString(hashes, name)) ||
                    !string.Equals(Sha256File(dll), GetString(hashes, name),
                                   StringComparison.OrdinalIgnoreCase))
                    throw new InvalidOperationException(
                        "Brak lub niezgodny SHA256 modułu " + name + ". Wykonaj Sprawdź / napraw.");
                NativeCheckX86(File.ReadAllBytes(dll), true);
            }

            var bytes = InstalledLoaderBytes();
            var loaderHash = Sha256(bytes);
            var manager = Path.Combine(root, ".wow335_updater");
            var loaderDir = Path.Combine(manager, "native_loader");
            NativeRejectReparse(manager);
            NativeRejectReparse(loaderDir);
            Directory.CreateDirectory(loaderDir);
            var loader = Path.Combine(loaderDir, RegisteredRuntimeLoader);
            NativeRejectReparse(loader);
            if (!File.Exists(loader) ||
                !string.Equals(Sha256File(loader), loaderHash, StringComparison.OrdinalIgnoreCase))
            {
                var stage = loader + "." + Guid.NewGuid().ToString("N") + ".tmp";
                try
                {
                    File.WriteAllBytes(stage, bytes);
                    if (!string.Equals(Sha256File(stage), loaderHash, StringComparison.OrdinalIgnoreCase))
                        throw new IOException("Launcher nie przeszedł weryfikacji SHA256.");
                    UpdaterSafety.ReplaceFile(stage, loader, ".native_loader.bak");
                }
                finally { if (File.Exists(stage)) File.Delete(stage); }
            }
            if (!string.Equals(Sha256File(loader), loaderHash, StringComparison.OrdinalIgnoreCase))
                throw new IOException("Launcher różni się od wbudowanego artefaktu.");

            var start = new ProcessStartInfo(loader)
            {
                Arguments = "--exe \"" + exe.Replace("\"", "") + "\"",
                WorkingDirectory = loaderDir,
                UseShellExecute = false
            };
            // Explicit native process start: the game-only Process guard must not redirect the loader.
            using (var running = System.Diagnostics.Process.Start(start))
            {
                if (running == null) throw new IOException("Nie uruchomiono launchera modułów.");
                Log("Uruchomiono launcher dla " + listed.Length + " DLL pakietu " +
                    ShortSha(GetString(state, "head_sha")) + " (PID " + running.Id +
                    "). Załadowanie i działanie w grze wymagają potwierdzenia.");
            }
            return true;
        }
    }
}
