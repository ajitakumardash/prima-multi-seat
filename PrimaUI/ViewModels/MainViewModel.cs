// ============================================================
// Prima Multi Seat - Main View Model
// MVVM: Binds sidebar navigation and top-level commands
// ============================================================

using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using System.Windows.Input;
using System.Windows.Media;
using PrimaUI.Services;

namespace PrimaUI.ViewModels;

public class NavItem
{
    public required string Icon  { get; set; }
    public required string Label { get; set; }
    public required string Page  { get; set; }
}

public class MainViewModel : INotifyPropertyChanged
{
    private readonly IPCClient _ipc;
    private string _engineStatus    = "Connecting...";
    private Brush  _engineStatusColor = Brushes.Orange;
    private string _pageTitle       = "Dashboard";

    public ObservableCollection<NavItem> NavItems { get; } = new()
    {
        new() { Icon = "📊", Label = "Dashboard",   Page = "Dashboard" },
        new() { Icon = "🖥", Label = "Seats",        Page = "Seats"     },
        new() { Icon = "⌨", Label = "Devices",      Page = "Devices"   },
        new() { Icon = "🔊", Label = "Audio",        Page = "Audio"     },
        new() { Icon = "⚙", Label = "Settings",     Page = "Settings"  },
        new() { Icon = "📋", Label = "Logs",         Page = "Logs"      },
    };

    public string EngineStatus
    {
        get => _engineStatus;
        set { _engineStatus = value; OnPropertyChanged(); }
    }

    public Brush EngineStatusColor
    {
        get => _engineStatusColor;
        set { _engineStatusColor = value; OnPropertyChanged(); }
    }

    public string PageTitle
    {
        get => _pageTitle;
        set { _pageTitle = value; OnPropertyChanged(); }
    }

    public ICommand FailsafeCommand { get; }
    public ICommand RefreshCommand  { get; }

    public MainViewModel()
    {
        _ipc = new IPCClient();

        FailsafeCommand = new RelayCommand(async _ => {
            var result = System.Windows.MessageBox.Show(
                "Trigger emergency failsafe recovery?\n\n" +
                "This will disable all seat isolation immediately.",
                "Prima Multi Seat - Failsafe",
                System.Windows.MessageBoxButton.YesNo,
                System.Windows.MessageBoxImage.Warning);
            if (result == System.Windows.MessageBoxResult.Yes) {
                await _ipc.SendCommandAsync("failsafe");
            }
        });

        RefreshCommand = new RelayCommand(async _ => {
            await RefreshStatus();
        });

        _ = RefreshStatus();
        StartPolling();
    }

    private async Task RefreshStatus()
    {
        var status = await _ipc.GetStatusAsync();
        if (status != null) {
            EngineStatus      = "Running";
            EngineStatusColor = new SolidColorBrush(Color.FromRgb(34, 197, 94));
        } else {
            EngineStatus      = "Offline";
            EngineStatusColor = Brushes.Red;
        }
    }

    private void StartPolling()
    {
        var timer = new System.Windows.Threading.DispatcherTimer();
        timer.Interval = TimeSpan.FromSeconds(5);
        timer.Tick    += async (s, e) => await RefreshStatus();
        timer.Start();
    }

    public event PropertyChangedEventHandler? PropertyChanged;
    private void OnPropertyChanged([CallerMemberName] string? name = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
}
