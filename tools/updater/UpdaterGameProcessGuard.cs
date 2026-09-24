using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;

namespace WoW335Updater
{
    // Never kill by basename alone: a second WoW may run from another directory.
    internal static class GameProcessGuard
    {
        private static List<System.Diagnostics.Process> Find(string root, string installedExeName)
        {
            var found = new List<System.Diagnostics.Process>();
            var directory = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)
                + Path.DirectorySeparatorChar;
            var installedName = Path.GetFileName(installedExeName ?? string.Empty);
            var selfId = System.Diagnostics.Process.GetCurrentProcess().Id;
            foreach (var candidate in System.Diagnostics.Process.GetProcesses())
            {
                var match = false;
                try
                {
                    if (candidate.Id == selfId) continue;
                    var module = candidate.MainModule;
                    var path = module == null ? null : module.FileName;
                    if (string.IsNullOrEmpty(path) ||
                        !Path.GetFullPath(path).StartsWith(directory, StringComparison.OrdinalIgnoreCase))
                        continue;
                    var name = Path.GetFileName(path);
                    if (string.Equals(name, "WoW335Updater.exe", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(name, "WoW335UpdaterBootstrap.exe", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(name, "WoW335RuntimeLoader.exe", StringComparison.OrdinalIgnoreCase))
                        continue;
                    var standard = string.Equals(name, "WoW.exe", StringComparison.OrdinalIgnoreCase) ||
                        (name.StartsWith("WoW_", StringComparison.OrdinalIgnoreCase) &&
                         name.EndsWith(".exe", StringComparison.OrdinalIgnoreCase));
                    match = standard || (!string.IsNullOrWhiteSpace(installedName) &&
                        string.Equals(name, installedName, StringComparison.OrdinalIgnoreCase));
                    if (match) found.Add(candidate);
                }
                catch (System.ComponentModel.Win32Exception)
                {
                    // A protected unrelated process must never be terminated by name alone.
                }
                catch (InvalidOperationException)
                {
                    // Process exited while checking its executable path.
                }
                finally
                {
                    if (!match) candidate.Dispose();
                }
            }
            return found;
        }

        internal static bool IsRunning(string root, string installedExeName)
        {
            var processes = Find(root, installedExeName);
            try
            {
                foreach (var process in processes)
                    if (!process.HasExited) return true;
                return false;
            }
            finally
            {
                foreach (var process in processes) process.Dispose();
            }
        }

        private static async Task<bool> WaitUntilExitedAsync(List<System.Diagnostics.Process> processes, TimeSpan timeout)
        {
            var deadline = DateTime.UtcNow + timeout;
            while (true)
            {
                var running = false;
                foreach (var process in processes)
                    if (!process.HasExited) { running = true; break; }
                if (!running) return true;
                if (DateTime.UtcNow >= deadline) return false;
                await Task.Delay(100);
            }
        }

        internal static async Task<int> StopForUpdateAsync(string root, string installedExeName, Action<string> log)
        {
            var closed = 0;
            // A second pass also detects a game opened during the first shutdown.
            for (var pass = 0; pass < 2; pass++)
            {
                var processes = Find(root, installedExeName);
                try
                {
                    if (processes.Count == 0) return closed;
                    foreach (var process in processes)
                    {
                        if (process.HasExited) continue;
                        if (log != null) log("Zamykam grę przed aktualizacją (PID " + process.Id + ").");
                        process.CloseMainWindow();
                    }
                    if (!await WaitUntilExitedAsync(processes, TimeSpan.FromSeconds(4)))
                    {
                        foreach (var process in processes)
                        {
                            if (process.HasExited) continue;
                            if (log != null) log("Gra nie zamknęła się w 4 s; wymuszam zakończenie PID " + process.Id + ".");
                            process.Kill();
                        }
                        if (!await WaitUntilExitedAsync(processes, TimeSpan.FromSeconds(5)))
                            throw new InvalidOperationException("Nie udało się zamknąć gry. Aktualizacja została wstrzymana.");
                    }
                    closed += processes.Count;
                }
                finally
                {
                    foreach (var process in processes) process.Dispose();
                }
            }
            if (IsRunning(root, installedExeName))
                throw new InvalidOperationException("Gra została ponownie uruchomiona. Aktualizacja została wstrzymana.");
            return closed;
        }
    }
}
