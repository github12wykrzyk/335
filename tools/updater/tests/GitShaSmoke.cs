using System;
using WoW335Updater;

// Runs as an actual Windows x86 .NET executable in the updater CI, not just a source-text assertion.
internal static class GitShaSmoke
{
    private static void Check(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }

    private static int Main()
    {
        var commit = "a9e8a3a05d6006bde92035efa6ea72d7bd4b7389";
        var fileHash = "55bb429af7a1ef8359c66d27bffcfadd1c15577c18817ed5f97920cdcca6ac7c";
        Check(commit.Length == 40 && fileHash.Length == 64, "test vectors");
        Check(UpdaterSafety.IsGitCommitSha(commit), "valid HEAD must pass");
        Check(!UpdaterSafety.IsGitCommitSha(fileHash), "file SHA256 must not be accepted as commit SHA");
        Check(!UpdaterSafety.IsGitCommitSha(commit.Substring(1)), "short Git SHA");
        Check(!UpdaterSafety.IsGitCommitSha(commit + "0"), "long Git SHA");
        Check(!UpdaterSafety.IsGitCommitSha(commit.ToUpperInvariant()), "uppercase Git SHA");
        Check(!UpdaterSafety.IsGitCommitSha(new string('g', 40)), "non-hex Git SHA");
        Check(!UpdaterSafety.IsGitCommitSha(null), "null Git SHA");
        Check(!UpdaterSafety.IsSha256Hex(commit), "40-byte Git SHA is not a SHA256");
        Check(UpdaterSafety.IsSha256Hex(fileHash), "valid SHA256 must pass");
        Check(!UpdaterSafety.IsSha256Hex(fileHash.Substring(1)), "short SHA256");
        Console.WriteLine("GIT_SHA_VALIDATION_SMOKE: PASS");
        return 0;
    }
}
