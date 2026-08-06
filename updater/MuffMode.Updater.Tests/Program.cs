using System.Reflection;
using System.Reflection.Emit;
using System.Security.Cryptography;
using MuffMode.Updater;

namespace MuffMode.Updater.Tests;

internal static class Program
{
    private readonly record struct CalledMethod(int MetadataToken, int Offset);

    private const string TestRelativePath = @"rerelease\maps\obsolete-test.bsp";
    private static readonly byte[] ExpectedPayload = CreateExpectedPayload();
    private static readonly string ExpectedSha256 = Convert.ToHexString(
        SHA256.HashData(ExpectedPayload)).ToLowerInvariant();
    private static readonly IReadOnlyDictionary<ushort, OpCode> OpCodesByValue =
        typeof(OpCodes)
            .GetFields(BindingFlags.Public | BindingFlags.Static)
            .Where(field => field.FieldType == typeof(OpCode))
            .Select(field => (OpCode)field.GetValue(null)!)
            .ToDictionary(opCode => unchecked((ushort)opCode.Value));

    private static int passed;
    private static int skipped;
    private static int failed;

    private static int Main()
    {
        Run("exact size and SHA-256 removes the obsolete file", RemovesExactPayload);
        Run("wrong size is preserved", PreservesWrongSize);
        Run("wrong SHA-256 is preserved", PreservesWrongHash);
        Run("directory at the obsolete path is preserved", PreservesDirectory);
        Run("path traversal cannot remove a file outside the install root", PreservesOutsideRoot);
        Run("file reparse point is preserved", PreservesFileReparsePoint);
        Run("reparse-point parent cannot redirect cleanup outside the install root", PreservesReparseParentTarget);
        Run("deferred self-update always attempts obsolete-file cleanup", VerifiesDeferredCleanupWiring);

        Console.WriteLine($"Updater cleanup tests: {passed} passed, {skipped} skipped, {failed} failed.");
        return failed == 0 ? 0 : 1;
    }

    private static void Run(string name, Action test)
    {
        try
        {
            test();
            passed++;
            Console.WriteLine($"[PASS] {name}");
        }
        catch (SkippedTestException ex)
        {
            skipped++;
            Console.WriteLine($"[SKIP] {name}: {ex.Message}");
        }
        catch (Exception ex)
        {
            failed++;
            Console.Error.WriteLine($"[FAIL] {name}: {ex}");
        }
    }

    private static void RemovesExactPayload()
    {
        WithSandbox((_, installRoot) =>
        {
            var candidate = WriteCandidate(installRoot, ExpectedPayload);
            var outcome = Cleanup(installRoot);

            AssertEqual(ObsoleteFileCleanupDisposition.Removed, outcome.Disposition);
            Assert(!File.Exists(candidate), "The verified obsolete file still exists.");
            Assert(!Directory.Exists(candidate), "The verified obsolete path became a directory.");
        });
    }

    private static void PreservesWrongSize()
    {
        WithSandbox((_, installRoot) =>
        {
            var wrongSizePayload = ExpectedPayload[..^1];
            var candidate = WriteCandidate(installRoot, wrongSizePayload);
            var outcome = Cleanup(installRoot);

            AssertEqual(ObsoleteFileCleanupDisposition.PreservedLengthMismatch, outcome.Disposition);
            AssertEqual((long)wrongSizePayload.Length, outcome.ActualLength);
            AssertFileContents(candidate, wrongSizePayload);
        });
    }

    private static void PreservesWrongHash()
    {
        WithSandbox((_, installRoot) =>
        {
            var wrongHashPayload = (byte[])ExpectedPayload.Clone();
            wrongHashPayload[^1] ^= 0xff;
            var candidate = WriteCandidate(installRoot, wrongHashPayload);
            var outcome = Cleanup(installRoot);

            AssertEqual(ObsoleteFileCleanupDisposition.PreservedHashMismatch, outcome.Disposition);
            AssertFileContents(candidate, wrongHashPayload);
        });
    }

    private static void PreservesDirectory()
    {
        WithSandbox((_, installRoot) =>
        {
            var candidate = GetCandidatePath(installRoot);
            Directory.CreateDirectory(candidate);
            var outcome = Cleanup(installRoot);

            AssertEqual(ObsoleteFileCleanupDisposition.PreservedDirectory, outcome.Disposition);
            Assert(Directory.Exists(candidate), "The directory at the obsolete path was removed.");
        });
    }

    private static void PreservesOutsideRoot()
    {
        WithSandbox((sandboxRoot, installRoot) =>
        {
            var outsidePath = Path.Combine(sandboxRoot, "outside.bsp");
            File.WriteAllBytes(outsidePath, ExpectedPayload);
            var outcome = InstallationManager.CleanupObsoleteFileBestEffort(
                installRoot,
                @"..\outside.bsp",
                ExpectedPayload.Length,
                ExpectedSha256);

            AssertEqual(
                ObsoleteFileCleanupDisposition.PreservedUnsafeOrInaccessible,
                outcome.Disposition);
            AssertFileContents(outsidePath, ExpectedPayload);
        });
    }

    private static void PreservesFileReparsePoint()
    {
        WithSandbox((sandboxRoot, installRoot) =>
        {
            var outsidePath = Path.Combine(sandboxRoot, "outside-file.bsp");
            File.WriteAllBytes(outsidePath, ExpectedPayload);
            var candidate = GetCandidatePath(installRoot);
            Directory.CreateDirectory(Path.GetDirectoryName(candidate)!);

            try
            {
                try
                {
                    File.CreateSymbolicLink(candidate, outsidePath);
                }
                catch (Exception ex) when (IsUnsupportedReparseTest(ex))
                {
                    throw new SkippedTestException($"file symbolic links are unavailable ({ex.GetType().Name})");
                }

                var outcome = Cleanup(installRoot);
                AssertEqual(
                    ObsoleteFileCleanupDisposition.PreservedReparsePoint,
                    outcome.Disposition);
                Assert(IsReparsePoint(candidate), "The file symbolic link was removed or replaced.");
                AssertFileContents(outsidePath, ExpectedPayload);
            }
            finally
            {
                DeleteFileLinkBestEffort(candidate);
            }
        });
    }

    private static void PreservesReparseParentTarget()
    {
        WithSandbox((sandboxRoot, installRoot) =>
        {
            var outsideMaps = Path.Combine(sandboxRoot, "outside-maps");
            Directory.CreateDirectory(outsideMaps);
            var outsidePath = Path.Combine(outsideMaps, "obsolete-test.bsp");
            File.WriteAllBytes(outsidePath, ExpectedPayload);

            var rereleasePath = Path.Combine(installRoot, "rerelease");
            Directory.CreateDirectory(rereleasePath);
            var mapsLink = Path.Combine(rereleasePath, "maps");

            try
            {
                try
                {
                    Directory.CreateSymbolicLink(mapsLink, outsideMaps);
                }
                catch (Exception ex) when (IsUnsupportedReparseTest(ex))
                {
                    throw new SkippedTestException($"directory symbolic links are unavailable ({ex.GetType().Name})");
                }

                var outcome = Cleanup(installRoot);
                Assert(
                    outcome.Disposition is ObsoleteFileCleanupDisposition.PreservedReparsePoint
                        or ObsoleteFileCleanupDisposition.PreservedUnsafeOrInaccessible,
                    $"Unexpected cleanup disposition: {outcome.Disposition}.");
                Assert(IsReparsePoint(mapsLink), "The directory symbolic link was removed or replaced.");
                AssertFileContents(outsidePath, ExpectedPayload);
            }
            finally
            {
                DeleteDirectoryLinkBestEffort(mapsLink);
            }
        });
    }

    private static void VerifiesDeferredCleanupWiring()
    {
        var managerType = typeof(InstallationManager);
        var deferredCleanup = GetPrivateStaticMethod(managerType, "RunCleanupSelfUpdateCommand");
        var stagedFileCleanup = GetPrivateStaticMethod(managerType, "DeleteStagedSelfUpdateFiles");
        var obsoleteFileCleanup = GetPrivateStaticMethod(managerType, "CleanupObsoleteAerowalkMapBestEffort");
        var calledMethods = ReadCalledMethods(deferredCleanup);

        var stagedCleanupIndex = calledMethods.FindIndex(
            call => call.MetadataToken == stagedFileCleanup.MetadataToken);
        var obsoleteCleanupIndex = calledMethods.FindIndex(
            call => call.MetadataToken == obsoleteFileCleanup.MetadataToken);
        Assert(stagedCleanupIndex >= 0, "Deferred cleanup no longer calls staged-file cleanup.");
        Assert(obsoleteCleanupIndex >= 0, "Deferred cleanup no longer calls obsolete-map cleanup.");
        Assert(
            obsoleteCleanupIndex > stagedCleanupIndex,
            "Obsolete-map cleanup must be attempted after staged-file cleanup.");

        var stagedCleanupOffset = calledMethods[stagedCleanupIndex].Offset;
        var obsoleteCleanupOffset = calledMethods[obsoleteCleanupIndex].Offset;
        var finallyProtectsCleanup = deferredCleanup
            .GetMethodBody()!
            .ExceptionHandlingClauses
            .Any(clause =>
                clause.Flags == ExceptionHandlingClauseOptions.Finally
                && IsOffsetInRange(stagedCleanupOffset, clause.TryOffset, clause.TryLength)
                && IsOffsetInRange(obsoleteCleanupOffset, clause.HandlerOffset, clause.HandlerLength));
        Assert(
            finallyProtectsCleanup,
            "Obsolete-map cleanup must remain in a finally handler around staged-file cleanup.");
    }

    private static ObsoleteFileCleanupOutcome Cleanup(string installRoot)
    {
        return InstallationManager.CleanupObsoleteFileBestEffort(
            installRoot,
            TestRelativePath,
            ExpectedPayload.Length,
            ExpectedSha256);
    }

    private static string WriteCandidate(string installRoot, byte[] payload)
    {
        var candidate = GetCandidatePath(installRoot);
        Directory.CreateDirectory(Path.GetDirectoryName(candidate)!);
        File.WriteAllBytes(candidate, payload);
        return candidate;
    }

    private static string GetCandidatePath(string installRoot)
    {
        return Path.Combine(installRoot, TestRelativePath);
    }

    private static void WithSandbox(Action<string, string> test)
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "MuffModeUpdaterTests",
            Guid.NewGuid().ToString("N"));
        var installRoot = Path.Combine(sandboxRoot, "install");
        Directory.CreateDirectory(installRoot);

        try
        {
            test(sandboxRoot, installRoot);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, recursive: true);
            }
            catch
            {
                // Test sandboxes contain no user data. A locked temporary file should
                // not mask the behavioral assertion that already ran.
            }
        }
    }

    private static MethodInfo GetPrivateStaticMethod(Type type, string name)
    {
        return type.GetMethod(name, BindingFlags.NonPublic | BindingFlags.Static)
            ?? throw new InvalidOperationException($"Could not find {type.FullName}.{name}.");
    }

    private static List<CalledMethod> ReadCalledMethods(MethodInfo method)
    {
        var il = method.GetMethodBody()?.GetILAsByteArray()
            ?? throw new InvalidOperationException($"{method.Name} has no readable IL body.");
        var calls = new List<CalledMethod>();

        for (var offset = 0; offset < il.Length;)
        {
            var value = (ushort)il[offset++];
            if (value == 0xfe)
            {
                Assert(offset < il.Length, $"{method.Name} ends with an incomplete IL opcode.");
                value = (ushort)(0xfe00 | il[offset++]);
            }

            if (!OpCodesByValue.TryGetValue(value, out var opCode))
            {
                throw new InvalidOperationException($"Unknown IL opcode 0x{value:x4} in {method.Name}.");
            }

            if (opCode.OperandType == OperandType.InlineMethod)
            {
                Assert(offset + sizeof(int) <= il.Length, $"{method.Name} has a truncated method token.");
                calls.Add(new(BitConverter.ToInt32(il, offset), offset - opCode.Size));
            }

            offset += GetOperandSize(opCode.OperandType, il, offset);
            Assert(offset <= il.Length, $"{method.Name} has a truncated IL operand.");
        }

        return calls;
    }

    private static bool IsOffsetInRange(int offset, int rangeOffset, int rangeLength)
    {
        return offset >= rangeOffset && offset < checked(rangeOffset + rangeLength);
    }

    private static int GetOperandSize(OperandType operandType, byte[] il, int operandOffset)
    {
        return operandType switch
        {
            OperandType.InlineNone => 0,
            OperandType.ShortInlineBrTarget or OperandType.ShortInlineI or OperandType.ShortInlineVar => 1,
            OperandType.InlineVar => 2,
            OperandType.InlineBrTarget
                or OperandType.InlineField
                or OperandType.InlineI
                or OperandType.InlineMethod
                or OperandType.InlineSig
                or OperandType.InlineString
                or OperandType.InlineTok
                or OperandType.ShortInlineR => 4,
            OperandType.InlineI8 or OperandType.InlineR => 8,
            OperandType.InlineSwitch => GetSwitchOperandSize(il, operandOffset),
            _ => throw new InvalidOperationException($"Unsupported IL operand type: {operandType}.")
        };
    }

    private static int GetSwitchOperandSize(byte[] il, int operandOffset)
    {
        Assert(operandOffset + sizeof(int) <= il.Length, "A switch operand is truncated.");
        var count = BitConverter.ToInt32(il, operandOffset);
        Assert(count >= 0, "A switch operand has a negative case count.");
        return checked(sizeof(int) + (count * sizeof(int)));
    }

    private static bool IsUnsupportedReparseTest(Exception ex)
    {
        return ex is UnauthorizedAccessException
            or IOException
            or PlatformNotSupportedException
            or NotSupportedException;
    }

    private static bool IsReparsePoint(string path)
    {
        try
        {
            return (File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0;
        }
        catch
        {
            return false;
        }
    }

    private static void DeleteFileLinkBestEffort(string path)
    {
        try
        {
            if (IsReparsePoint(path))
            {
                File.Delete(path);
            }
        }
        catch
        {
            // The unique temporary sandbox is removed best-effort by the caller.
        }
    }

    private static void DeleteDirectoryLinkBestEffort(string path)
    {
        try
        {
            if (IsReparsePoint(path))
            {
                Directory.Delete(path, recursive: false);
            }
        }
        catch
        {
            // The unique temporary sandbox is removed best-effort by the caller.
        }
    }

    private static void AssertFileContents(string path, byte[] expected)
    {
        Assert(File.Exists(path), $"Expected preserved file is missing: {path}");
        Assert(File.ReadAllBytes(path).SequenceEqual(expected), $"Preserved file changed: {path}");
    }

    private static void Assert(bool condition, string message)
    {
        if (!condition)
        {
            throw new InvalidOperationException(message);
        }
    }

    private static void AssertEqual<T>(T expected, T actual)
    {
        if (!EqualityComparer<T>.Default.Equals(expected, actual))
        {
            throw new InvalidOperationException($"Expected {expected}, but found {actual}.");
        }
    }

    private static byte[] CreateExpectedPayload()
    {
        var payload = new byte[4096];
        for (var index = 0; index < payload.Length; index++)
        {
            payload[index] = (byte)((index * 37 + 11) & 0xff);
        }

        return payload;
    }

    private sealed class SkippedTestException(string message) : Exception(message);
}
