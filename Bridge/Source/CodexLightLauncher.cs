using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Windows.Forms;

[assembly: AssemblyTitle("CodexLight Tray")]
[assembly: AssemblyProduct("CodexLight")]
[assembly: AssemblyDescription("CodexLight Windows tray bridge")]
[assembly: AssemblyVersion("1.0.0.0")]
[assembly: AssemblyFileVersion("1.0.0.0")]

internal static class CodexLightLauncher
{
    private const string AppName = "CodexLight";
    private const string TrayResource = "CodexLight.TrayScript";
    private const string MonitorResource = "CodexLight.MonitorScript";
    private const string PythonResource = "CodexLight.PythonRuntime";

    [STAThread]
    private static int Main(string[] args)
    {
        try
        {
            string mode = ParseMode(args);
            string appDirectory = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                AppName);
            Directory.CreateDirectory(appDirectory);

            string trayPath = Path.Combine(appDirectory, "CodexLightTray.ps1");
            string monitorPath = Path.Combine(appDirectory, "codex_light_monitor.py");
            WriteResourceIfChanged(TrayResource, trayPath);
            WriteResourceIfChanged(MonitorResource, monitorPath);

            string runtimeDirectory = EnsurePythonRuntime(appDirectory);
            string pythonwPath = Path.Combine(runtimeDirectory, "pythonw.exe");
            string powershellPath = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.Windows),
                "System32",
                "WindowsPowerShell",
                "v1.0",
                "powershell.exe");

            var startInfo = new ProcessStartInfo
            {
                FileName = powershellPath,
                Arguments = BuildTrayArguments(trayPath, pythonwPath, appDirectory, mode),
                WorkingDirectory = appDirectory,
                UseShellExecute = false,
                CreateNoWindow = true,
                WindowStyle = ProcessWindowStyle.Hidden,
            };
            Process.Start(startInfo);
            return 0;
        }
        catch (Exception exception)
        {
            MessageBox.Show(
                "Unable to start CodexLight tray.\r\n\r\n" + exception.Message,
                AppName,
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
            return 1;
        }
    }

    private static string ParseMode(string[] args)
    {
        if (args.Length == 0)
        {
            return "WIRELESS";
        }

        string mode = args[0].Trim().ToUpperInvariant();
        return mode == "AUTO" || mode == "WIRED" || mode == "WIRELESS"
            ? mode
            : "WIRELESS";
    }

    private static string BuildTrayArguments(
        string trayPath,
        string pythonwPath,
        string appDirectory,
        string mode)
    {
        var arguments = new StringBuilder();
        arguments.Append("-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File ");
        arguments.Append(Quote(trayPath));
        arguments.Append(" -Python ").Append(Quote(pythonwPath));
        arguments.Append(" -WorkDir ").Append(Quote(appDirectory));
        arguments.Append(" -ConnectionMode ").Append(Quote(mode));
        arguments.Append(" -SerialPort auto -SerialBaud 115200 -UdpPort 4210");
        return arguments.ToString();
    }

    private static string Quote(string value)
    {
        return "\"" + value.Replace("\"", "\\\"") + "\"";
    }

    private static string EnsurePythonRuntime(string appDirectory)
    {
        string runtimeDirectory = Path.Combine(appDirectory, "runtime");
        string pythonwPath = Path.Combine(runtimeDirectory, "pythonw.exe");
        if (File.Exists(pythonwPath))
        {
            return runtimeDirectory;
        }

        using (var mutex = new Mutex(false, @"Local\CodexLightRuntimeInstaller"))
        {
            if (!mutex.WaitOne(TimeSpan.FromSeconds(30)))
            {
                throw new TimeoutException("Timed out while preparing the embedded Python runtime.");
            }

            try
            {
                if (File.Exists(pythonwPath))
                {
                    return runtimeDirectory;
                }

                string temporaryDirectory = runtimeDirectory + ".new-" + Process.GetCurrentProcess().Id;
                if (Directory.Exists(temporaryDirectory))
                {
                    Directory.Delete(temporaryDirectory, true);
                }
                Directory.CreateDirectory(temporaryDirectory);

                string archivePath = Path.Combine(temporaryDirectory, "python-runtime.zip");
                WriteResource(PythonResource, archivePath);
                ZipFile.ExtractToDirectory(archivePath, temporaryDirectory);
                File.Delete(archivePath);

                if (!File.Exists(Path.Combine(temporaryDirectory, "pythonw.exe")))
                {
                    throw new InvalidDataException("The embedded Python runtime is incomplete.");
                }

                if (Directory.Exists(runtimeDirectory))
                {
                    Directory.Delete(temporaryDirectory, true);
                }
                else
                {
                    Directory.Move(temporaryDirectory, runtimeDirectory);
                }
            }
            finally
            {
                mutex.ReleaseMutex();
            }
        }

        return runtimeDirectory;
    }

    private static void WriteResourceIfChanged(string resourceName, string destinationPath)
    {
        string existingHash = File.Exists(destinationPath) ? FileHash(destinationPath) : string.Empty;
        string resourceHash = ResourceHash(resourceName);
        if (string.Equals(existingHash, resourceHash, StringComparison.OrdinalIgnoreCase))
        {
            return;
        }

        string temporaryPath = destinationPath + ".new";
        WriteResource(resourceName, temporaryPath);
        if (File.Exists(destinationPath))
        {
            File.Replace(temporaryPath, destinationPath, null);
        }
        else
        {
            File.Move(temporaryPath, destinationPath);
        }
    }

    private static void WriteResource(string resourceName, string destinationPath)
    {
        using (Stream source = ResourceStream(resourceName))
        using (var destination = new FileStream(destinationPath, FileMode.Create, FileAccess.Write, FileShare.None))
        {
            source.CopyTo(destination);
        }
    }

    private static string ResourceHash(string resourceName)
    {
        using (Stream stream = ResourceStream(resourceName))
        using (SHA256 algorithm = SHA256.Create())
        {
            return Hex(algorithm.ComputeHash(stream));
        }
    }

    private static string FileHash(string path)
    {
        using (var stream = File.OpenRead(path))
        using (SHA256 algorithm = SHA256.Create())
        {
            return Hex(algorithm.ComputeHash(stream));
        }
    }

    private static string Hex(byte[] value)
    {
        var builder = new StringBuilder(value.Length * 2);
        foreach (byte item in value)
        {
            builder.Append(item.ToString("x2"));
        }
        return builder.ToString();
    }

    private static Stream ResourceStream(string resourceName)
    {
        Stream stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(resourceName);
        if (stream == null)
        {
            throw new InvalidOperationException("Missing embedded resource: " + resourceName);
        }
        return stream;
    }
}
