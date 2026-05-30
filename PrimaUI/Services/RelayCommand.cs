// ============================================================
// Prima Multi Seat - Relay Command (MVVM Helper)
// ============================================================

using System;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;

namespace PrimaUI.Services;

/// <summary>
/// An async-capable <see cref="ICommand"/> implementation for WPF MVVM.
/// Prevents re-entrancy while a task is executing and exposes
/// optional cancellation support.
/// </summary>
public sealed class RelayCommand : ICommand
{
    // ── fields ─────────────────────────────────────────────
    private readonly Func<object?, CancellationToken, Task> _execute;
    private readonly Func<object?, bool>?                   _canExecute;
    private          bool                                    _isExecuting;

    // ── constructors ───────────────────────────────────────

    /// <summary>
    /// Creates a <see cref="RelayCommand"/> that runs an async delegate.
    /// </summary>
    /// <param name="execute">
    ///   Async action to run; receives the command parameter and a
    ///   <see cref="CancellationToken"/>.
    /// </param>
    /// <param name="canExecute">
    ///   Optional predicate evaluated by WPF to enable / disable
    ///   bound controls.
    /// </param>
    public RelayCommand(
        Func<object?, CancellationToken, Task> execute,
        Func<object?, bool>?                   canExecute = null)
    {
        _execute    = execute    ?? throw new ArgumentNullException(nameof(execute));
        _canExecute = canExecute;
    }

    /// <summary>
    /// Convenience overload for delegates that don't need a
    /// <see cref="CancellationToken"/>.
    /// </summary>
    public RelayCommand(
        Func<object?, Task>  execute,
        Func<object?, bool>? canExecute = null)
        : this((p, _) => execute(p), canExecute)
    {
        if (execute is null) throw new ArgumentNullException(nameof(execute));
    }

    /// <summary>
    /// Convenience overload for synchronous delegates.
    /// </summary>
    public RelayCommand(
        Action<object?>      execute,
        Func<object?, bool>? canExecute = null)
        : this((p, _) => { execute(p); return Task.CompletedTask; }, canExecute)
    {
        if (execute is null) throw new ArgumentNullException(nameof(execute));
    }

    // ── ICommand ───────────────────────────────────────────

    /// <inheritdoc/>
    /// Returns <see langword="false"/> while a task is already executing.
    public bool CanExecute(object? parameter)
        => !_isExecuting && (_canExecute?.Invoke(parameter) ?? true);

    /// <inheritdoc/>
    public async void Execute(object? parameter)
    {
        if (!CanExecute(parameter)) return;

        _isExecuting = true;
        RaiseCanExecuteChanged();   // disable bound controls immediately

        try
        {
            await _execute(parameter, CancellationToken.None);
        }
        catch (OperationCanceledException)
        {
            // Swallow cancellation — not an error worth surfacing.
        }
        catch (Exception ex)
        {
            MessageBox.Show(ex.Message, "Error",
                MessageBoxButton.OK, MessageBoxImage.Error);
        }
        finally
        {
            _isExecuting = false;
            RaiseCanExecuteChanged();   // re-enable controls when done
            CommandManager.InvalidateRequerySuggested();
        }
    }

    // ── CanExecuteChanged ──────────────────────────────────

    /// <inheritdoc/>
    public event EventHandler? CanExecuteChanged
    {
        add    => CommandManager.RequerySuggested += value;
        remove => CommandManager.RequerySuggested -= value;
    }

    // ── helpers ────────────────────────────────────────────

    /// <summary>
    /// Manually raises <see cref="CanExecuteChanged"/> so bound controls
    /// re-query <see cref="CanExecute"/> immediately.
    /// </summary>
    public void RaiseCanExecuteChanged()
        => CommandManager.InvalidateRequerySuggested();
}
