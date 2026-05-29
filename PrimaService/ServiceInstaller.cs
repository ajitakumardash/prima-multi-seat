// ============================================================
// Prima Multi Seat - Service Installer Helper
// Provides programmatic service install/uninstall
// (also used by Inno Setup via command-line arguments)
// ============================================================

using System.Runtime.InteropServices;
using System.ServiceProcess;
using Microsoft.Extensions.Logging;

namespace PrimaService;

public static class ServiceInstaller
{
    private const string ServiceName        = "PrimaMultiSeatService";
    private const string ServiceDisplayName = "Prima Multi Seat Service";
    private const string ServiceDescription =
        "Manages Prima Multi Seat core engine and provides auto-recovery.";

    // ── Install ───────────────────────────────────────────
    public static bool Install(string exePath)
    {
        try
        {
            // Use sc.exe for reliable service registration
            var result = RunSc($"create {ServiceName} " +
                               $"binPath= "{exePath}" " +
                               $"DisplayName= "{ServiceDisplayName}" " +
                               $"start= auto " +
                               $"obj= LocalSystem");

            if (!result) return false;

            // Set description
            RunSc($"description {ServiceName} "{ServiceDescription}"");

            // Configure failure actions (restart on crash)
            RunSc($"failure {ServiceName} reset= 60 actions= restart/3000/restart/5000/restart/10000");

            Console.WriteLine($"Service '{ServiceName}' installed successfully.");
            return true;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"Install failed: {ex.Message}");
            return false;
        }
    }

    // ── Uninstall ─────────────────────────────────────────
    public static bool Uninstall()
    {
        try
        {
            Stop();
            RunSc($"delete {ServiceName}");
            Console.WriteLine($"Service '{ServiceName}' uninstalled.");
            return true;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"Uninstall failed: {ex.Message}");
            return false;
        }
    }

    // ── Start / Stop ──────────────────────────────────────
    public static void Start()
    {
        try {
            var svc = new ServiceController(ServiceName);
            if (svc.Status != ServiceControllerStatus.Running)
                svc.Start();
        } catch { }
    }

    public static void Stop()
    {
        try {
            var svc = new ServiceController(ServiceName);
            if (svc.Status == ServiceControllerStatus.Running)
                svc.Stop();
        } catch { }
    }

    // ── Helpers ───────────────────────────────────────────
    private static bool RunSc(string args)
    {
        var psi = new System.Diagnostics.ProcessStartInfo
        {
            FileName               = "sc.exe",
            Arguments              = args,
            UseShellExecute        = false,
            CreateNoWindow         = true,
            RedirectStandardOutput = true,
            RedirectStandardError  = true,
        };
        using var proc = System.Diagnostics.Process.Start(psi)!;
        proc.WaitForExit();
        return proc.ExitCode == 0;
    }
}