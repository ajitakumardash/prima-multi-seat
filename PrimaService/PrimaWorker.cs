// ============================================================
// Prima Multi Seat - Background Service Worker
// Monitors core engine process, auto-restarts on crash,
// handles device reconnect events via WMI.
// ============================================================

using System;                               // File, Path, Task, CancellationToken, Exception,
                                            // EventArgs, IntPtr, Environment
using System.Diagnostics;                   // Process, ProcessStartInfo
using System.Reflection;                    // Assembly.GetExecutingAssembly()
using System.Runtime.InteropServices;       // DllImport, StructLayout, LayoutKind
using System.ServiceProcess;                // ServiceBase (referenced by ServiceStatus convention)
using Microsoft.Extensions.Hosting;         // BackgroundService
using Microsoft.Extensions.Logging;         // ILogger
using Newtonsoft.Json;                      // JsonConvert (config deserialization)

namespace PrimaService;

/// <summary>
/// Windows Service that keeps PrimaMultiSeat.exe alive
/// and handles auto-recovery scenarios.
/// </summary>
public class PrimaWorker : BackgroundService
{
    private readonly ILogger<PrimaWorker> _logger;
    private Process? _coreProcess;
    private readonly string _installDir;
    private readonly string _coreExePath;
    private readonly string _configPath;
    private int _restartCount = 0;
    private const int MaxRestarts = 5;
    private const int RestartDelayMs = 3000;

    // ── Win32 Imports ──────────────────────────────────────
    [DllImport("advapi32.dll", SetLastError = true)]
    private static extern bool SetServiceStatus(IntPtr serviceHandle, ref ServiceStatus status);

    public PrimaWorker(ILogger<PrimaWorker> logger)
    {
        _logger    = logger;
        _installDir = Path.GetDirectoryName(
            Assembly.GetExecutingAssembly().Location)!;
        _coreExePath = Path.Combine(_installDir, "PrimaMultiSeat.exe");
        _configPath  = Path.Combine(_installDir, "config.json");
    }

    // ── Service Entry ──────────────────────────────────────
    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        _logger.LogInformation("Prima Multi Seat Service starting...");

        while (!stoppingToken.IsCancellationRequested)
        {
            try
            {
                await EnsureCoreRunning(stoppingToken);
                await Task.Delay(5000, stoppingToken);
            }
            catch (OperationCanceledException) { break; }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Service loop error");
                await Task.Delay(1000, stoppingToken);
            }
        }

        StopCoreProcess();
        _logger.LogInformation("Prima Multi Seat Service stopped.");
    }

    // ── Core Process Management ────────────────────────────
    private async Task EnsureCoreRunning(CancellationToken token)
    {
        if (!File.Exists(_coreExePath))
        {
            _logger.LogWarning("PrimaMultiSeat.exe not found at {Path}", _coreExePath);
            return;
        }

        if (_coreProcess == null || _coreProcess.HasExited)
        {
            if (_restartCount >= MaxRestarts)
            {
                _logger.LogError("Max restart attempts ({Max}) reached. Stopping.", MaxRestarts);
                return;
            }

            if (_restartCount > 0)
            {
                _logger.LogWarning("Core process exited. Restart #{Count} in {Delay}ms",
                    _restartCount, RestartDelayMs);
                await Task.Delay(RestartDelayMs, token);
            }

            StartCoreProcess();
            _restartCount++;
        }
        else
        {
            // Core is running — check health
            _restartCount = 0;
        }
    }

    private void StartCoreProcess()
    {
        _logger.LogInformation("Starting PrimaMultiSeat.exe...");

        var psi = new ProcessStartInfo
        {
            FileName               = _coreExePath,
            WorkingDirectory       = _installDir,
            UseShellExecute        = false,
            CreateNoWindow         = true,
            RedirectStandardOutput = false,
        };

        _coreProcess = new Process { StartInfo = psi, EnableRaisingEvents = true };
        _coreProcess.Exited += OnCoreExited;

        try {
            _coreProcess.Start();
            _logger.LogInformation("Core started with PID {Pid}", _coreProcess.Id);
        }
        catch (Exception ex) {
            _logger.LogError(ex, "Failed to start core process");
        }
    }

    private void StopCoreProcess()
    {
        if (_coreProcess != null && !_coreProcess.HasExited)
        {
            _logger.LogInformation("Stopping core process...");
            try {
                _coreProcess.CloseMainWindow();
                if (!_coreProcess.WaitForExit(3000))
                    _coreProcess.Kill();
            }
            catch (Exception ex) {
                _logger.LogWarning(ex, "Error stopping core process");
            }
        }
    }

    private void OnCoreExited(object? sender, EventArgs e)
    {
        int exitCode = _coreProcess?.ExitCode ?? -1;
        _logger.LogWarning("Core process exited with code {Code}", exitCode);
    }

    // ── Service Stop ───────────────────────────────────────
    public override async Task StopAsync(CancellationToken cancellationToken)
    {
        _logger.LogInformation("Service stop requested");
        StopCoreProcess();
        await base.StopAsync(cancellationToken);
    }
}

// ── Service Status Struct ──────────────────────────────────
[StructLayout(LayoutKind.Sequential)]
public struct ServiceStatus
{
    public int dwServiceType;
    public int dwCurrentState;
    public int dwControlsAccepted;
    public int dwWin32ExitCode;
    public int dwServiceSpecificExitCode;
    public int dwCheckPoint;
    public int dwWaitHint;
}
