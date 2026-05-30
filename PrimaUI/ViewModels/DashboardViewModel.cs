// ============================================================
// Prima Multi Seat - Dashboard View Model
// ============================================================

using System;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Windows.Input;
using System.Windows.Media;
using PrimaUI.Services;

namespace PrimaUI.ViewModels;

public class DashboardViewModel : INotifyPropertyChanged
{
    private readonly IPCClient _ipc;
    private string _seat1Status = "Inactive";
    private string _seat2Status = "Inactive";
    private string _cpuUsage   = "0%";
    private string _ramUsage   = "0%";
    private string _uptime     = "0:00";
    private double _cpuValue   = 0;
    private double _ramValue   = 0;

    // ── Seat 1 Properties ─────────────────────────────────
    public string Seat1Status
    {
        get => _seat1Status;
        set { _seat1Status = value; OnPropertyChanged(); OnPropertyChanged(nameof(Seat1StatusBg)); }
    }
    public Brush Seat1StatusBg =>
        _seat1Status == "Active"
            ? new SolidColorBrush(Color.FromRgb(34,197,94))
            : new SolidColorBrush(Color.FromRgb(107,114,128));

    public string Seat1Monitor   { get; set; } = "Display 1 (Primary)";
    public string Seat1Keyboard  { get; set; } = "HID Keyboard #1";
    public string Seat1Mouse     { get; set; } = "HID Mouse #1";
    public string Seat1Audio     { get; set; } = "Speakers (Default)";

    // ── Seat 2 Properties ─────────────────────────────────
    public string Seat2Status
    {
        get => _seat2Status;
        set { _seat2Status = value; OnPropertyChanged(); OnPropertyChanged(nameof(Seat2StatusBg)); }
    }
    public Brush Seat2StatusBg =>
        _seat2Status == "Active"
            ? new SolidColorBrush(Color.FromRgb(34,197,94))
            : new SolidColorBrush(Color.FromRgb(107,114,128));

    public string Seat2Monitor   { get; set; } = "Display 2";
    public string Seat2Keyboard  { get; set; } = "HID Keyboard #2";
    public string Seat2Mouse     { get; set; } = "HID Mouse #2";
    public string Seat2Audio     { get; set; } = "Headphones (HDMI)";

    // ── System Resources ──────────────────────────────────
    public string CpuUsage { get => _cpuUsage; set { _cpuUsage = value; OnPropertyChanged(); } }
    public string RamUsage { get => _ramUsage; set { _ramUsage = value; OnPropertyChanged(); } }
    public string Uptime   { get => _uptime;   set { _uptime   = value; OnPropertyChanged(); } }
    public double CpuValue { get => _cpuValue; set { _cpuValue = value; OnPropertyChanged(); } }
    public double RamValue { get => _ramValue; set { _ramValue = value; OnPropertyChanged(); } }

    // ── Commands ──────────────────────────────────────────
    public ICommand StartSeat1Command { get; }
    public ICommand StopSeat1Command  { get; }
    public ICommand StartSeat2Command { get; }
    public ICommand StopSeat2Command  { get; }

    public DashboardViewModel()
    {
        _ipc = new IPCClient();

        StartSeat1Command = new RelayCommand(async _ => {
            await _ipc.SendCommandAsync("start:0");
            Seat1Status = "Active";
        });
        StopSeat1Command = new RelayCommand(async _ => {
            await _ipc.SendCommandAsync("stop:0");
            Seat1Status = "Inactive";
        });
        StartSeat2Command = new RelayCommand(async _ => {
            await _ipc.SendCommandAsync("start:1");
            Seat2Status = "Active";
        });
        StopSeat2Command = new RelayCommand(async _ => {
            await _ipc.SendCommandAsync("stop:1");
            Seat2Status = "Inactive";
        });

        StartPolling();
    }

    private void StartPolling()
    {
        var rng   = new Random();
        var start = DateTime.Now;
        var timer = new System.Windows.Threading.DispatcherTimer();
        timer.Interval = TimeSpan.FromSeconds(2);
        timer.Tick += (s, e) => {
            // In production, read from PerformanceCounter
            var cpu = rng.Next(5, 30);
            var ram = rng.Next(30, 60);
            CpuValue = cpu;
            RamValue = ram;
            CpuUsage = cpu + "%";
            RamUsage = ram + "%";
            var elapsed = DateTime.Now - start;
            Uptime = $"{(int)elapsed.TotalHours}:{elapsed.Minutes:D2}";
        };
        timer.Start();
    }

    public event PropertyChangedEventHandler? PropertyChanged;
    private void OnPropertyChanged([CallerMemberName] string? name = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
}
