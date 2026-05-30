// ============================================================
// Prima Multi Seat - Service Installer Helper
// Provides programmatic service install/uninstall
// (also used by Inno Setup via command-line arguments)
// ============================================================

// Only BCL namespaces are used; no external NuGet packages required.
using System;
using System.Diagnostics;
using System.ServiceProcess;

namespace PrimaService;

/// <summary>
/// Installs, uninstalls, starts, and stops the Prima Multi Seat Windows service
/// using the native sc.exe utility so that all SCM metadata (description, failure
/// actions, start type) is set correctly without requiring the
/// System.ServiceProcess installer classes.
/// </summary>
public static class ServiceInstaller
{
    private const string ServiceName        = "PrimaMultiSeatService";
    private const string ServiceDisplayName = "Prima Multi Seat Service";
    private const string ServiceDescription =
        "Manages Prima Multi Seat core engine and provides auto-recovery.";

    // ── Install ───────────────────────────────────────────────────────────────

    /// <summary>
    /// Registers the Windows service with the SCM and configures failure-recovery
    /// actions. The service is set to start automatically at system boot under
    /// the LocalSystem account.
    /// </summary>
    /// <param name="exePath">
    /// Absolute path to the service executable. Must not be null or empty.
    /// </param>
    /// <returns><see langword="true"/> on success; <see langword="false"/> on any
    /// sc.exe failure.</returns>
    public static bool Install(string exePath)
    {
        if (string.IsNullOrWhiteSpace(exePath))
            throw new ArgumentException("Executable path must not be null or empty.", nameof(exePath));

        try
        {
            // sc.exe requires a space after each '=' and the value to be
            // double-quoted when it contains spaces. We embed literal
            // double-quote characters using \" inside the interpolated string.
            //
            // Resulting sc.exe command (illustrative):
            //   sc.exe create PrimaMultiSeatService
            //       binPath= "C:\path\to\service.exe"
            //       DisplayName= "Prima Multi Seat Service"
            //       start= auto
            //       obj= LocalSystem
            bool created = RunSc(
                $"create {ServiceName} " +
                $"binPath= \"{exePath}\" " +
                $"DisplayName= \"{ServiceDisplayName}\" " +
                $"start= auto " +
                $"obj= LocalSystem");

            if (!created)
            {
                Console.Error.WriteLine($"sc.exe create failed for service '{ServiceName}'.");
                return false;
            }

            // Set a human-readable description visible in services.msc.
            RunSc($"description {ServiceName} \"{ServiceDescription}\"");

            // Configure automatic restart on crash:
            //   reset= 60  – reset the failure count after 60 seconds of good uptime
            //   actions=   – restart after 3 s, 5 s, then 10 s on the third failure
            RunSc(
                $"failure {ServiceName} " +
                $"reset= 60 " +
                $"actions= restart/3000/restart/5000/restart/10000");

            Console.WriteLine($"Service '{ServiceName}' installed successfully.");
            return true;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"Install failed: {ex.Message}");
            return false;
        }
    }

    // ── Uninstall ─────────────────────────────────────────────────────────────

    /// <summary>
    /// Stops (if running) and removes the Windows service from the SCM.
    /// </summary>
    /// <returns><see langword="true"/> on success; <see langword="false"/> on any
    /// sc.exe failure.</returns>
    public static bool Uninstall()
    {
        try
        {
            // Best-effort stop; ignore errors so the delete still runs even if
            // the service is already stopped or in a pending state.
            Stop();

            bool deleted = RunSc($"delete {ServiceName}");
            if (!deleted)
            {
                Console.Error.WriteLine($"sc.exe delete failed for service '{ServiceName}'.");
                return false;
            }

            Console.WriteLine($"Service '{ServiceName}' uninstalled.");
            return true;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"Uninstall failed: {ex.Message}");
            return false;
        }
    }

    // ── Start ─────────────────────────────────────────────────────────────────

    /// <summary>
    /// Starts the service if it is not already running. Errors are written to
    /// <see cref="Console.Error"/> rather than thrown so the method is safe to
    /// call from setup/teardown paths.
    /// </summary>
    public static void Start()
    {
        try
        {
            using ServiceController svc = new(ServiceName);

            // Refresh() synchronises the in-process Status snapshot with the SCM.
            svc.Refresh();

            if (svc.Status != ServiceControllerStatus.Running &&
                svc.Status != ServiceControllerStatus.StartPending)
            {
                svc.Start();
                svc.WaitForStatus(ServiceControllerStatus.Running, TimeSpan.FromSeconds(30));
            }
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"Start failed for service '{ServiceName}': {ex.Message}");
        }
    }

    // ── Stop ──────────────────────────────────────────────────────────────────

    /// <summary>
    /// Stops the service if it is running and the SCM reports it can be stopped.
    /// Errors are written to <see cref="Console.Error"/> rather than thrown.
    /// </summary>
    public static void Stop()
    {
        try
        {
            using ServiceController svc = new(ServiceName);

            // Refresh() synchronises the in-process Status snapshot with the SCM.
            svc.Refresh();

            if (svc.Status == ServiceControllerStatus.Running && svc.CanStop)
            {
                svc.Stop();
                svc.WaitForStatus(ServiceControllerStatus.Stopped, TimeSpan.FromSeconds(30));
            }
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"Stop failed for service '{ServiceName}': {ex.Message}");
        }
    }

    // ── Private helpers ───────────────────────────────────────────────────────

    /// <summary>
    /// Runs sc.exe with the supplied <paramref name="args"/> and returns
    /// <see langword="true"/> when the process exits with code 0.
    /// </summary>
    /// <remarks>
    /// Both stdout and stderr are drained asynchronously to prevent the sc.exe
    /// process from blocking if the OS pipe buffer fills up, while still keeping
    /// the method synchronous for the caller.
    /// </remarks>
    private static bool RunSc(string args)
    {
        ProcessStartInfo psi = new()
        {
            FileName               = "sc.exe",
            Arguments              = args,
            UseShellExecute        = false,
            CreateNoWindow         = true,
            RedirectStandardOutput = true,
            RedirectStandardError  = true,
        };

        using Process proc = Process.Start(psi)
            ?? throw new InvalidOperationException("Failed to start sc.exe process.");

        // Drain both streams asynchronously to avoid deadlocks when the
        // sc.exe output exceeds the pipe buffer capacity (~4 KB on Windows).
        System.Threading.Tasks.Task<string> stdoutTask = proc.StandardOutput.ReadToEndAsync();
        System.Threading.Tasks.Task<string> stderrTask = proc.StandardError.ReadToEndAsync();

        proc.WaitForExit();

        // Await the drain tasks synchronously — WaitForExit() guarantees the
        // process has finished writing, so these complete essentially instantly.
        string stdout = stdoutTask.GetAwaiter().GetResult();
        string stderr = stderrTask.GetAwaiter().GetResult();

        if (proc.ExitCode != 0 && !string.IsNullOrWhiteSpace(stderr))
        {
            Console.Error.WriteLine($"sc.exe error (exit {proc.ExitCode}): {stderr.Trim()}");
        }
        else if (!string.IsNullOrWhiteSpace(stdout))
        {
            // Echo informational sc.exe output so it is visible in install logs.
            Console.WriteLine(stdout.Trim());
        }

        return proc.ExitCode == 0;
    }
}
