// ============================================================
// Prima Multi Seat - Relay Command (MVVM Helper)
// ============================================================

using System.Windows.Input;

namespace PrimaUI.Services;

public class RelayCommand : ICommand
{
    private readonly Func<object?, Task> _execute;
    private readonly Func<object?, bool>? _canExecute;

    public RelayCommand(Func<object?, Task> execute, Func<object?, bool>? canExecute = null)
    {
        _execute    = execute;
        _canExecute = canExecute;
    }

    public bool CanExecute(object? parameter) => _canExecute?.Invoke(parameter) ?? true;

    public async void Execute(object? parameter)
    {
        try { await _execute(parameter); }
        catch (Exception ex) {
            System.Windows.MessageBox.Show(ex.Message, "Error");
        }
        CommandManager.InvalidateRequerySuggested();
    }

    public event EventHandler? CanExecuteChanged
    {
        add    { CommandManager.RequerySuggested += value; }
        remove { CommandManager.RequerySuggested -= value; }
    }
}