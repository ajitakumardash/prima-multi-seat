// ============================================================
// Prima Multi Seat - IPC Client
// Communicates with PrimaCore via Named Pipe
// ============================================================

using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Pipes;
using System.Threading;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace PrimaUI.Services;

/// <summary>
/// Async named-pipe client that sends JSON commands to PrimaCore
/// and deserialises the responses.
/// </summary>
public sealed class IPCClient : IDisposable
{
    // ── constants ──────────────────────────────────────────
    private const string PipeName  = "PrimaMultiSeat";
    private const int    TimeoutMs = 2_000;

    // ── state ──────────────────────────────────────────────
    private bool _disposed;

    // ── public API ─────────────────────────────────────────

    /// <summary>
    /// Retrieves the current status dictionary from PrimaCore.
    /// Returns <see langword="null"/> on connection failure or timeout.
    /// </summary>
    /// <param name="ct">Optional cancellation token.</param>
    public async Task<Dictionary<string, object>?> GetStatusAsync(
        CancellationToken ct = default)
    {
        try
        {
            return await SendReceiveAsync<Dictionary<string, object>>("status", ct)
                         .ConfigureAwait(false);
        }
        catch
        {
            return null;
        }
    }

    /// <summary>
    /// Sends a fire-and-verify command to PrimaCore.
    /// Returns <see langword="true"/> when the server acknowledges; <see langword="false"/> on error.
    /// </summary>
    /// <param name="command">Command string understood by PrimaCore.</param>
    /// <param name="ct">Optional cancellation token.</param>
    public async Task<bool> SendCommandAsync(
        string            command,
        CancellationToken ct = default)
    {
        try
        {
            await SendReceiveAsync<object>(command, ct).ConfigureAwait(false);
            return true;
        }
        catch
        {
            return false;
        }
    }

    // ── core ───────────────────────────────────────────────

    /// <summary>
    /// Opens a transient named-pipe connection, sends <paramref name="message"/>,
    /// reads one JSON line, and deserialises it to <typeparamref name="T"/>.
    /// </summary>
    private async Task<T?> SendReceiveAsync<T>(
        string            message,
        CancellationToken callerToken = default)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        // Combine the caller's token with a hard 2-second timeout.
        using var timeoutCts = new CancellationTokenSource(TimeoutMs);
        using var linkedCts  = CancellationTokenSource.CreateLinkedTokenSource(
                                   callerToken, timeoutCts.Token);

        CancellationToken ct = linkedCts.Token;

        await using var pipe = new NamedPipeClientStream(
            ".", PipeName, PipeDirection.InOut, PipeOptions.Asynchronous);

        await pipe.ConnectAsync(ct).ConfigureAwait(false);

        if (!pipe.IsConnected) return default;

        await using var netStream = (Stream)pipe; // keeps the pipe alive
        using  var writer = new StreamWriter(pipe, leaveOpen: true) { AutoFlush = true };
        using  var reader = new StreamReader(pipe, leaveOpen: true);

        await writer.WriteLineAsync(message.AsMemory(), ct).ConfigureAwait(false);

        string? response = await reader.ReadLineAsync(ct).ConfigureAwait(false);

        if (string.IsNullOrWhiteSpace(response)) return default;

        return JsonConvert.DeserializeObject<T>(response);
    }

    // ── IDisposable ────────────────────────────────────────

    /// <inheritdoc/>
    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        GC.SuppressFinalize(this); // CA1816 — suppress finaliser if one is added later
    }
}
