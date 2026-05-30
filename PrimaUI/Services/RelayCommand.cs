 // ============================================================
  // Prima Multi Seat - Relay Command (MVVM Helper)
  // ============================================================

- using System.Windows.Input;
+ using System;
+ using System.Threading;
+ using System.Threading.Tasks;
+ using System.Windows;
+ using System.Windows.Input;

  namespace PrimaUI.Services;

- public class RelayCommand : ICommand
+ /// <summary>
+ /// An async-capable ICommand implementation for WPF MVVM.
+ /// Prevents re-entrancy while a task is executing and exposes
+ /// optional cancellation support.
+ /// </summary>
+ public sealed class RelayCommand : ICommand
  {
-     private readonly Func<object?, Task> _execute;
-     private readonly Func<object?, bool>? _canExecute;
+     private readonly Func<object?, CancellationToken, Task> _execute;
+     private readonly Func<object?, bool>?                   _canExecute;
+     private          bool                                    _isExecuting;

-     public RelayCommand(Func<object?, Task> execute, Func<object?, bool>? canExecute = null)
+     // New: async+token constructor (primary)
+     public RelayCommand(
+         Func<object?, CancellationToken, Task> execute,
+         Func<object?, bool>? canExecute = null)
      {
-         _execute    = execute;
+         _execute    = execute ?? throw new ArgumentNullException(nameof(execute));
          _canExecute = canExecute;
      }

+     // New: async-only overload (no token)
+     public RelayCommand(Func<object?, Task> execute, Func<object?, bool>? canExecute = null)
+         : this((p, _) => execute(p), canExecute) { }

+     // New: sync overload
+     public RelayCommand(Action<object?> execute, Func<object?, bool>? canExecute = null)
+         : this((p, _) => { execute(p); return Task.CompletedTask; }, canExecute) {if (execute is null)
        throw new ArgumentNullException(nameof(execute)); }

-     public bool CanExecute(object? parameter) => _canExecute?.Invoke(parameter) ?? true;
+     // Now also returns false while executing (re-entrancy guard)
+     public bool CanExecute(object? parameter)
+         => !_isExecuting && (_canExecute?.Invoke(parameter) ?? true);

      public async void Execute(object? parameter)
      {
-         try { await _execute(parameter); }
-         catch (Exception ex) {
-             System.Windows.MessageBox.Show(ex.Message, "Error");
-         }
-         CommandManager.InvalidateRequerySuggested();
+         if (!CanExecute(parameter)) return;
+
+         _isExecuting = true;
+         RaiseCanExecuteChanged();
+
+         try
+         {
+             await _execute(parameter, CancellationToken.None);
+         }
+         catch (OperationCanceledException) { /* swallowed */ }
+         catch (Exception ex)
+         {
+             MessageBox.Show(ex.Message, "Error",
+                 MessageBoxButton.OK, MessageBoxImage.Error);
+         }
+         finally
+         {
+             _isExecuting = false;
+             RaiseCanExecuteChanged();
+             CommandManager.InvalidateRequerySuggested();
+         }
      }

      public event EventHandler? CanExecuteChanged
      {
-         add    { CommandManager.RequerySuggested += value; }
-         remove { CommandManager.RequerySuggested -= value; }
+         add    => CommandManager.RequerySuggested += value;
+         remove => CommandManager.RequerySuggested -= value;
      }

+     // New: public helper
+     public void RaiseCanExecuteChanged()
+         => CommandManager.InvalidateRequerySuggested();
  }
