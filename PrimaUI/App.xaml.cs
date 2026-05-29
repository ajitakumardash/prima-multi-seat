// ============================================================
// Prima Multi Seat - WPF Application Entry
// ============================================================
using System.Windows;
using PrimaUI.Services;

namespace PrimaUI;

public partial class App : Application
{
    private IPCClient? _ipcClient;

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        _ipcClient = new IPCClient();

        // Register unhandled exception handler
        DispatcherUnhandledException += (s, ex) => {
            MessageBox.Show($"Unexpected error:\n{ex.Exception.Message}",
                "Prima Multi Seat Error", MessageBoxButton.OK, MessageBoxImage.Error);
            ex.Handled = true;
        };
    }

    protected override void OnExit(ExitEventArgs e)
    {
        _ipcClient?.Dispose();
        base.OnExit(e);
    }
}