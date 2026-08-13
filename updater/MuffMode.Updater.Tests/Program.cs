using System.Reflection;
using System.Reflection.Emit;
using System.Security.Cryptography;
using MuffMode.Updater;

namespace MuffMode.Updater.Tests;

internal static class Program
{
    private readonly record struct CalledMethod(int MetadataToken, int Offset);

    private const string TestRelativePath = @"rerelease\baseq2\maps\obsolete-test.bsp";
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
        Run("updater bootstrap inventory stays rename-safe", VerifiesStableBootstrapInventory);
        Run("legacy updater bridge inventory stays exact", VerifiesLegacyUpdaterBridgeInventory);
        Run("server-config backup directory names round-trip", VerifiesConfigBackupDirectoryNames);
        Run("empty operator configs can be restored atomically", RestoresEmptyOperatorConfig);
        Run("partial host installs roll back as one bundle", RollsBackPartialHostBundle);
        Run("installed receipt rewrites preserve the destination manifest", VerifiesReceiptExcludedDestinationManifest);
        Run("GitHub Markdown release notes render as styled rich text", RendersGitHubReleaseNotesMarkdown);
        Run("zero-padded release tags are accepted", AcceptsZeroPaddedReleaseTags);
        Run("a release tag must be a bare version", RejectsTagsThatAreNotBareVersions);
        Run("an unversioned release tag is skipped, not fatal", SkipsReleaseWithUnversionedTag);
        Run("a release title cannot override the release tag", PrefersTagOverTitleForReleaseVersion);

        Console.WriteLine($"Updater tests: {passed} passed, {skipped} skipped, {failed} failed.");
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

    private static void RendersGitHubReleaseNotesMarkdown()
    {
        Exception? failure = null;
        var thread = new Thread(() =>
        {
            try
            {
                using var target = new RichTextBox
                {
                    BackColor = Color.FromArgb(18, 20, 21)
                };
                target.CreateControl();

                var release = new ReleaseInfo(
                    new SemanticVersion(1, 2, 3),
                    42,
                    "v1.2.3",
                    "beta",
                    "Muff Mode v1.2.3 Beta",
                    "",
                    "https://github.com/DarkMatter-Productions/MuffMode/releases/tag/v1.2.3",
                    true,
                    new DateTimeOffset(2026, 8, 13, 12, 0, 0, TimeSpan.Zero),
                    43,
                    "muffmode-1.2.3-beta.zip",
                    "https://example.invalid/muffmode.zip",
                    8 * 1024 * 1024,
                    "application/zip",
                    null,
                    "sha256:abc");
                const string markdown = "# Highlights\n\n- **Safer hosting** with `mapdb.json` and [full details](https://example.com/details).\n\n> Ready for lobby hosts.\n\n```cfg\nexec lobby-casual.cfg\n```";

                ReleaseNotesRenderer.Render(target, release, markdown);

                Assert(target.Text.Contains("Highlights", StringComparison.Ordinal), "The Markdown heading disappeared.");
                Assert(target.Text.Contains("• Safer hosting", StringComparison.Ordinal), "The list was not rendered with a display bullet.");
                Assert(target.Text.Contains("full details — https://example.com/details", StringComparison.Ordinal), "The Markdown link was not rendered readably.");
                Assert(target.Text.Contains("exec lobby-casual.cfg", StringComparison.Ordinal), "The fenced code block disappeared.");
                Assert(!target.Text.Contains("**", StringComparison.Ordinal), "Bold Markdown delimiters remained visible.");
                Assert(!target.Text.Contains("```", StringComparison.Ordinal), "Code-fence delimiters remained visible.");

                var boldStart = target.Text.IndexOf("Safer hosting", StringComparison.Ordinal);
                target.Select(boldStart, "Safer hosting".Length);
                Assert(target.SelectionFont?.Bold == true, "Bold Markdown was not styled bold.");

                var codeStart = target.Text.IndexOf("mapdb.json", StringComparison.Ordinal);
                target.Select(codeStart, "mapdb.json".Length);
                Assert(target.SelectionBackColor != target.BackColor, "Inline code did not receive code styling.");
            }
            catch (Exception ex)
            {
                failure = ex;
            }
        });
        thread.SetApartmentState(ApartmentState.STA);
        thread.Start();
        thread.Join();

        if (failure is not null)
        {
            throw new InvalidOperationException("The rich-text Markdown renderer failed on its UI thread.", failure);
        }
    }

    private static void VerifiesStableBootstrapInventory()
    {
        var requiredPackageFiles = GetPrivateStaticStringSet("RequiredPackageFiles");
        var requiredInstallPlanFiles = GetPrivateStaticStringSet("RequiredInstallPlanFiles");
        var mutableHostAssets = new[]
        {
            "factories.cfg",
            "lobby-casual.cfg",
            "lobby-competitive.cfg",
            "lobby-horde.cfg",
            "lobby-party.cfg",
            "mapdb.json",
            "muffmode-map-cycle.txt",
            "muffmode-map-pool.json"
        };

        foreach (var fileName in mutableHostAssets)
        {
            Assert(
                !requiredPackageFiles.Any(path => Path.GetFileName(path).Equals(fileName, StringComparison.OrdinalIgnoreCase)),
                $"RequiredPackageFiles hard-pins renameable host asset {fileName}.");
            Assert(
                !requiredInstallPlanFiles.Any(path => Path.GetFileName(path).Equals(fileName, StringComparison.OrdinalIgnoreCase)),
                $"RequiredInstallPlanFiles hard-pins renameable host asset {fileName}.");
        }
    }

    private static void VerifiesLegacyUpdaterBridgeInventory()
    {
        var actual = GetPrivateStaticStringSet("LegacyUpdaterCompatibilityFiles")
            .Select(Path.GetFileName)
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        var expected = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
        {
            "gt-CA.cfg",
            "gt-CTF.cfg",
            "gt-DUEL.cfg",
            "gt-FFA.cfg",
            "gt-HORDE.cfg",
            "gt-INSTAGIB.cfg",
            "gt-NADEFEST.cfg",
            "gt-REDROVER.cfg",
            "gt-STRIKE.cfg",
            "gt-TDM.cfg"
        };

        Assert(actual.SetEquals(expected), "The v0.60.20 bridge file inventory changed.");
    }

    private static void VerifiesConfigBackupDirectoryNames()
    {
        WithSandbox((_, installRoot) =>
        {
            var backupRoot = Path.Combine(
                installRoot,
                "rerelease",
                "baseq2",
                "MuffModeBackups");
            Directory.CreateDirectory(backupRoot);

            const string timestamp = "20260813-120000";
            var generated = (string)InvokeInstallationManagerMethod(
                "GetUniqueConfigBackupDirectory",
                backupRoot,
                timestamp)!;
            AssertEqual(
                Path.Combine(backupRoot, $"server-configs-before-muffmode-{timestamp}"),
                generated);
            Directory.CreateDirectory(generated);

            var indexed = (string)InvokeInstallationManagerMethod(
                "GetUniqueConfigBackupDirectory",
                backupRoot,
                timestamp)!;
            AssertEqual(
                Path.Combine(backupRoot, $"server-configs-before-muffmode-{timestamp}-2"),
                indexed);

            foreach (var prefix in new[]
                     {
                         "server-configs-before-muffmode-",
                         "server-configs.before-muffmode-"
                     })
            {
                var directoryName = $"{prefix}{timestamp}-compat";
                var directory = Path.Combine(backupRoot, directoryName);
                Directory.CreateDirectory(directory);
                var markerName = (string)InvokeInstallationManagerMethod(
                    "GetConfigBackupDirectoryName",
                    installRoot,
                    directory)!;
                AssertEqual(directoryName, markerName);
            }
        });
    }

    private static void RestoresEmptyOperatorConfig()
    {
        WithSandbox((_, installRoot) =>
        {
            var backupDirectory = Path.Combine(
                installRoot,
                "rerelease",
                "baseq2",
                "MuffModeBackups",
                "server-configs-before-muffmode-test");
            var destinationDirectory = Path.Combine(installRoot, "rerelease", "baseq2");
            Directory.CreateDirectory(backupDirectory);
            Directory.CreateDirectory(destinationDirectory);

            var backupPath = Path.Combine(backupDirectory, "server-base.cfg");
            var destinationPath = Path.Combine(destinationDirectory, "server-base.cfg");
            File.WriteAllBytes(backupPath, []);
            File.WriteAllText(destinationPath, "package contents");

            InvokeInstallationManagerMethod(
                "CopyRollbackFileAtomically",
                installRoot,
                backupPath,
                destinationPath,
                "rollback copy for rerelease/baseq2/server-base.cfg");

            Assert(File.Exists(destinationPath), "The empty operator config was not restored.");
            AssertEqual(0L, new FileInfo(destinationPath).Length);
        });
    }

    private static void RollsBackPartialHostBundle()
    {
        WithSandbox((_, installRoot) =>
        {
            var packageRoot = Path.Combine(installRoot, "package-source");
            var baseq2 = Path.Combine(installRoot, "rerelease", "baseq2");
            var backupDirectory = Path.Combine(
                baseq2,
                "MuffModeBackups",
                "server-configs-before-muffmode-rollback-test");
            Directory.CreateDirectory(packageRoot);
            Directory.CreateDirectory(baseq2);
            Directory.CreateDirectory(backupDirectory);

            var changedDestination = Path.Combine(baseq2, "server-base.cfg");
            var newDestination = Path.Combine(baseq2, "lobby-casual.cfg");
            var matchingDestination = Path.Combine(baseq2, "factories.cfg");
            var changedSource = Path.Combine(packageRoot, "server-base.cfg");
            var newSource = Path.Combine(packageRoot, "lobby-casual.cfg");
            var matchingSource = Path.Combine(packageRoot, "factories.cfg");

            File.WriteAllText(changedSource, "new baseline");
            File.WriteAllText(newSource, "new lobby");
            File.WriteAllText(matchingSource, "unchanged factory");
            File.WriteAllText(changedDestination, "operator baseline");
            File.WriteAllText(matchingDestination, "unchanged factory");
            File.SetAttributes(matchingDestination, FileAttributes.ReadOnly);
            File.WriteAllText(Path.Combine(backupDirectory, "server-base.cfg"), "operator baseline");

            var installPlan = CreatePrivateInstallPlanFromSources(
                (changedSource, changedDestination, @"rerelease\baseq2\server-base.cfg"),
                (newSource, newDestination, @"rerelease\baseq2\lobby-casual.cfg"),
                (matchingSource, matchingDestination, @"rerelease\baseq2\factories.cfg"));
            var rollbackFiles = InvokeInstallationManagerMethod(
                "CaptureServerConfigRollbackFiles",
                installPlan)!;

            File.SetAttributes(matchingDestination, FileAttributes.Normal);
            File.WriteAllText(changedDestination, "new baseline");
            File.WriteAllText(newDestination, "new lobby");

            InvokeInstallationManagerMethod(
                "RollbackServerConfigs",
                installRoot,
                rollbackFiles,
                backupDirectory);

            AssertEqual("operator baseline", File.ReadAllText(changedDestination));
            Assert(!File.Exists(newDestination), "A newly installed host file survived rollback.");
            AssertEqual("unchanged factory", File.ReadAllText(matchingDestination));
            Assert(
                (File.GetAttributes(matchingDestination) & FileAttributes.ReadOnly) != 0,
                "Rollback did not restore attributes on a pre-existing matching host file.");
        });
    }

    private static void VerifiesReceiptExcludedDestinationManifest()
    {
        WithSandbox((_, installRoot) =>
        {
            var baseq2 = Path.Combine(installRoot, "rerelease", "baseq2");
            Directory.CreateDirectory(baseq2);

            var ordinaryPath = Path.Combine(baseq2, "server-base.cfg");
            var jsonReceiptPath = Path.Combine(baseq2, "muffmode-version.json");
            var textReceiptPath = Path.Combine(baseq2, "muffmode.version");
            File.WriteAllText(ordinaryPath, "stable config");
            File.WriteAllText(jsonReceiptPath, "package metadata");
            File.WriteAllText(textReceiptPath, "package version");

            Assert(InstallationManager.IsMutableInstalledReceiptPath(
                @"rerelease\baseq2\muffmode-version.json"), "The JSON receipt was not classified as mutable.");
            Assert(InstallationManager.IsMutableInstalledReceiptPath(
                "rerelease/baseq2/muffmode.version"), "The text receipt was not classified as mutable.");
            Assert(!InstallationManager.IsMutableInstalledReceiptPath(
                @"rerelease\baseq2\server-base.cfg"), "A stable host file was classified as a receipt.");

            var installPlan = CreatePrivateInstallPlan(
                (ordinaryPath, @"rerelease\baseq2\server-base.cfg"),
                (jsonReceiptPath, @"rerelease\baseq2\muffmode-version.json"),
                (textReceiptPath, @"rerelease\baseq2\muffmode.version"));
            var before = (string)InvokeInstallationManagerMethod(
                "ComputeInstalledDestinationManifestSha256",
                installPlan)!;

            File.WriteAllText(jsonReceiptPath, "installed receipt metadata");
            File.WriteAllText(textReceiptPath, "installed version");
            var afterReceiptRewrite = (string)InvokeInstallationManagerMethod(
                "ComputeInstalledDestinationManifestSha256",
                installPlan)!;
            AssertEqual(before, afterReceiptRewrite);

            File.WriteAllText(ordinaryPath, "changed config");
            var afterStableFileChange = (string)InvokeInstallationManagerMethod(
                "ComputeInstalledDestinationManifestSha256",
                installPlan)!;
            Assert(
                !string.Equals(before, afterStableFileChange, StringComparison.Ordinal),
                "The destination manifest ignored a stable installed file change.");
        });
    }

    private static Array CreatePrivateInstallPlan(params (string DestinationPath, string RelativePath)[] files)
    {
        return CreatePrivateInstallPlanFromSources(
            files.Select(file => (
                SourcePath: file.DestinationPath,
                file.DestinationPath,
                file.RelativePath)).ToArray());
    }

    private static Array CreatePrivateInstallPlanFromSources(
        params (string SourcePath, string DestinationPath, string RelativePath)[] files)
    {
        var installFileType = typeof(InstallationManager).GetNestedType(
            "PackageInstallFile",
            BindingFlags.NonPublic)
            ?? throw new InvalidOperationException("PackageInstallFile is missing from InstallationManager.");
        var constructor = installFileType.GetConstructors(
                BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic)
            .Single(candidate => candidate.GetParameters().Length == 5);
        var installPlan = Array.CreateInstance(installFileType, files.Length);

        for (var index = 0; index < files.Length; index++)
        {
            var (sourcePath, destinationPath, relativePath) = files[index];
            var bytes = File.ReadAllBytes(sourcePath);
            var sha256 = Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();
            var installFile = constructor.Invoke(
                [sourcePath, relativePath, destinationPath, (long)bytes.Length, sha256]);
            installPlan.SetValue(installFile, index);
        }

        return installPlan;
    }

    private static IReadOnlyCollection<string> GetPrivateStaticStringSet(string fieldName)
    {
        var field = typeof(InstallationManager).GetField(
            fieldName,
            BindingFlags.NonPublic | BindingFlags.Static)
            ?? throw new InvalidOperationException($"{fieldName} is missing from InstallationManager.");
        return field.GetValue(null) as IReadOnlyCollection<string>
            ?? throw new InvalidOperationException($"{fieldName} is not a string collection.");
    }

    private static object? InvokeInstallationManagerMethod(string name, params object?[] arguments)
    {
        var method = GetPrivateStaticMethod(typeof(InstallationManager), name);
        try
        {
            return method.Invoke(null, arguments);
        }
        catch (TargetInvocationException ex)
        {
            throw new InvalidOperationException(ex.InnerException!.Message, ex.InnerException);
        }
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

    // MuffMode shipped v0.21.07, v0.21.09 and v0.22.00. Requiring a tag to round-trip through
    // SemanticVersion.ToString() rejected all three, and one rejected release aborted the whole
    // release lookup, so the updater could not check for updates at all.
    private static void AcceptsZeroPaddedReleaseTags()
    {
        (string Tag, SemanticVersion Expected)[] cases =
        {
            ("v0.22.00", new SemanticVersion(0, 22, 0)),
            ("v0.21.07", new SemanticVersion(0, 21, 7)),
            ("v0.21.09", new SemanticVersion(0, 21, 9)),
            ("v0.70.30", new SemanticVersion(0, 70, 30)),
            ("0.70.30", new SemanticVersion(0, 70, 30)),
        };

        foreach (var (tag, expected) in cases)
        {
            Assert(SemanticVersion.TryParseExact(tag, out var actual), $"Release tag {tag} was rejected.");
            AssertEqual(expected, actual);
            Assert(
                InvokeValidateReleaseTag(tag, expected) == tag,
                $"Release tag {tag} did not survive validation.");
        }
    }

    // The tag names a download URL and a staged install path, so it must carry nothing else.
    private static void RejectsTagsThatAreNotBareVersions()
    {
        string[] tags =
        {
            "latest",
            "release-1.2.3-final",
            "1.2.3-evil",
            "v1.2.3/../../etc",
            "v1.2.3?x=1",
            "v1.2.3#frag",
            "v1.2.3-alpha",
            "v1.2.3+build",
            "v1.2.3.4",
            "1.2",
            "v1.2.3\nv9.9.9",
            "v9999999999.0.0",
            "",
        };

        foreach (var tag in tags)
        {
            Assert(
                !SemanticVersion.TryParseExact(tag, out _),
                $"Tag {tag.Replace("\n", "\\n")} was accepted as a bare version.");
        }
    }

    // The repository carries a rolling "latest" tag titled "v0.18.5 BETA". Deriving the version
    // from that title made it a candidate whose tag could never validate, which threw and took
    // the entire release lookup down with it.
    private static void SkipsReleaseWithUnversionedTag()
    {
        var release = new GitHubReleaseDto
        {
            Id = 1,
            TagName = "latest",
            Name = "v0.18.5 BETA",
            PublishedAt = DateTimeOffset.UtcNow.AddDays(-1),
            Assets =
            [
                new GitHubAssetDto
                {
                    Id = 2,
                    Name = "muffmode-0.18.5-beta.zip",
                    BrowserDownloadUrl = "https://github.com/DarkMatter-Productions/MuffMode/releases/download/latest/muffmode-0.18.5-beta.zip",
                    Size = 2 * 1024 * 1024,
                    ContentType = "application/zip",
                    State = "uploaded",
                }
            ],
        };

        Assert(InvokeTryCreateReleaseInfo(release) is null, "An unversioned release tag was not skipped.");
    }

    private static void PrefersTagOverTitleForReleaseVersion()
    {
        var release = new GitHubReleaseDto
        {
            Id = 1,
            TagName = "not-a-version",
            Name = "MuffMode v9.9.9",
            PublishedAt = DateTimeOffset.UtcNow.AddDays(-1),
            Assets =
            [
                new GitHubAssetDto
                {
                    Id = 2,
                    Name = "muffmode-9.9.9.zip",
                    BrowserDownloadUrl = "https://github.com/DarkMatter-Productions/MuffMode/releases/download/not-a-version/muffmode-9.9.9.zip",
                    Size = 2 * 1024 * 1024,
                    ContentType = "application/zip",
                    State = "uploaded",
                }
            ],
        };

        Assert(
            InvokeTryCreateReleaseInfo(release) is null,
            "A release title promoted an unversioned tag into an update candidate.");
    }

    private static object? InvokeTryCreateReleaseInfo(GitHubReleaseDto release)
    {
        return InvokeReleaseClientMethod("TryCreateReleaseInfo", release);
    }

    private static string InvokeValidateReleaseTag(string tag, SemanticVersion version)
    {
        return (string)InvokeReleaseClientMethod("ValidateReleaseTag", tag, version)!;
    }

    private static object? InvokeReleaseClientMethod(string name, params object?[] arguments)
    {
        var method = typeof(GitHubReleaseClient)
            .GetMethod(name, BindingFlags.NonPublic | BindingFlags.Static)
            ?? throw new InvalidOperationException($"{name} is missing from GitHubReleaseClient.");

        try
        {
            return method.Invoke(null, arguments);
        }
        catch (TargetInvocationException ex)
        {
            throw new InvalidOperationException(ex.InnerException!.Message, ex.InnerException);
        }
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
