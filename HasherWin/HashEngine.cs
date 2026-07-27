using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace HasherWin;

internal sealed record FileEntry(string Name, long Size, string Hash);

internal static class HashEngine
{
    private const int ReadBufferSize = 1024 * 1024;

    public static List<(string FullPath, long Size)> CollectFiles(string rootDirectory)
    {
        var result = new List<(string, long)>();
        CollectFilesRecursive(rootDirectory, result);
        return result;
    }

    private static void CollectFilesRecursive(string directory, List<(string FullPath, long Size)> result)
    {
        foreach (var file in Directory.EnumerateFiles(directory))
        {
            var info = new FileInfo(file);
            result.Add((file, info.Length));
        }

        foreach (var subDir in Directory.EnumerateDirectories(directory))
            CollectFilesRecursive(subDir, result);
    }

    public static string ComputeSha256(string filePath)
    {
        using var sha = SHA256.Create();
        using var stream = new FileStream(
            filePath,
            FileMode.Open,
            FileAccess.Read,
            FileShare.Read,
            ReadBufferSize,
            FileOptions.SequentialScan);

        var hash = sha.ComputeHash(stream);
        return Convert.ToHexString(hash).ToLowerInvariant();
    }

    public static List<FileEntry> BuildManifest(string inputDir, IProgress<(int Current, int Total, string RelativePath)>? progress, CancellationToken cancel)
    {
        var files = CollectFiles(inputDir);
        if (files.Count == 0)
            return [];

        var root = Path.GetFullPath(inputDir).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        var results = new List<FileEntry>(files.Count);
        var total = files.Count;

        for (var i = 0; i < files.Count; i++)
        {
            cancel.ThrowIfCancellationRequested();

            var (fullPath, size) = files[i];
            var relative = Path.GetRelativePath(root, fullPath);
            progress?.Report((i + 1, total, relative));

            var hash = ComputeSha256(fullPath);
            results.Add(new FileEntry(relative, size, hash));
        }

        return results;
    }

    public static void WriteJsonManifest(IEnumerable<FileEntry> files, string outputPath)
    {
        var payload = new
        {
            files = files.Select(f => new
            {
                name = f.Name,
                size = f.Size,
                hash = f.Hash
            }).ToArray()
        };

        var options = new JsonSerializerOptions { WriteIndented = true };
        var json = JsonSerializer.Serialize(payload, options);
        File.WriteAllText(outputPath, json + Environment.NewLine, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
    }
}
