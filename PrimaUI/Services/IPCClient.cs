// ============================================================
// Prima Multi Seat - IPC Client
// Communicates with PrimaCore via Named Pipe
// ============================================================

using System.IO.Pipes;
using Newtonsoft.Json;

namespace PrimaUI.Services;

public class IPCClient : IDisposable
{
    private const string PipeName    = "PrimaMultiSeat";
    private const int    TimeoutMs   = 2000;
    private bool         _disposed;

    // ── Get Status ────────────────────────────────────────
    public async Task<Dictionary<string, object>?> GetStatusAsync()
    {
        try {
            return await SendReceiveAsync<Dictionary<string, object>>("status");
        } catch {
            return null;
        }
    }

    // ── Send Command ──────────────────────────────────────
    public async Task<bool> SendCommandAsync(string command)
    {
        try {
            await SendReceiveAsync<object>(command);
            return true;
        } catch {
            return false;
        }
    }

    // ── Core Send/Receive ─────────────────────────────────
    private async Task<T?> SendReceiveAsync<T>(string message)
    {
        using var pipe = new NamedPipeClientStream(".", PipeName,
            PipeDirection.InOut, PipeOptions.Asynchronous);

        await pipe.ConnectAsync(TimeoutMs);
        if (!pipe.IsConnected) return default;

        using var writer = new StreamWriter(pipe) { AutoFlush = true };
        using var reader = new StreamReader(pipe);

        await writer.WriteLineAsync(message);
        var response = await reader.ReadLineAsync();

        if (string.IsNullOrEmpty(response)) return default;
        return JsonConvert.DeserializeObject<T>(response);
    }

    public void Dispose()
    {
        if (!_disposed) { _disposed = true; }
    }
}