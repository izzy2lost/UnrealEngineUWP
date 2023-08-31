// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute.Transports;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Compute.Clients
{
	/// <summary>
	/// Implementation of <see cref="IComputeClient"/> which marshals data over a loopback connection to a method running on a background task in the same process.
	/// </summary>
	public sealed class LocalComputeClient : IComputeClient
	{
		class LeaseImpl : IComputeLease
		{
			public IReadOnlyList<string> Properties { get; } = new List<string>();
			public IReadOnlyDictionary<string, int> AssignedResources => new Dictionary<string, int>();
			public RemoteComputeSocket Socket => _socket;

			readonly RemoteComputeSocket _socket;

			public LeaseImpl(RemoteComputeSocket socket) => _socket = socket;

			/// <inheritdoc/>
			public ValueTask DisposeAsync() => _socket.DisposeAsync();

			/// <inheritdoc/>
			public ValueTask CloseAsync(CancellationToken cancellationToken) => _socket.CloseAsync(cancellationToken);
		}

		class PrefixLogger : ILogger
		{
			readonly string _prefix;
			readonly ILogger _inner;

			public PrefixLogger(string prefix, ILogger inner)
			{
				_prefix = prefix;
				_inner = inner;
			}

			public IDisposable BeginScope<TState>(TState state) => _inner.BeginScope<TState>(state);
			public bool IsEnabled(LogLevel logLevel) => _inner.IsEnabled(logLevel);

			public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter)
			{
				if (state is IEnumerable<KeyValuePair<string, object>> enumerable)
				{
					List<KeyValuePair<string, object>> copy = new List<KeyValuePair<string, object>>(enumerable);

					int idx = copy.FindIndex(x => x.Key.Equals("{OriginalFormat}", StringComparison.OrdinalIgnoreCase));
					if (idx != -1 && copy[idx].Value is string format)
					{
						copy[idx] = new KeyValuePair<string, object>(copy[idx].Key, "[{_tag}] " + format);
						copy.Add(new KeyValuePair<string, object>("_tag", _prefix));
						_inner.Log(logLevel, eventId, copy, exception, (s, e) => $"[{_prefix}] {formatter(state, exception)}");
						return;
					}
				}
				_inner.Log(logLevel, eventId, state, exception, formatter);
			}
		}

		readonly BackgroundTask _listenerTask;
		readonly Socket _listener;
		readonly Socket _socket;
		readonly bool _executeInProcess;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="port">Port to connect on</param>
		/// <param name="sandboxDir">Sandbox directory for the worker</param>
		/// <param name="executeInProcess">Whether to run external assemblies in-process. Useful for debugging.</param>
		/// <param name="logger">Logger for diagnostic output</param>
		public LocalComputeClient(int port, DirectoryReference sandboxDir, bool executeInProcess, ILogger logger)
		{
			_logger = logger;
			_executeInProcess = executeInProcess;

			_listener = new Socket(SocketType.Stream, ProtocolType.IP);
			_listener.Bind(new IPEndPoint(IPAddress.Loopback, port));
			_listener.Listen();

			_listenerTask = BackgroundTask.StartNew(ctx => RunListenerAsync(_listener, sandboxDir, _executeInProcess, new PrefixLogger("REMOTE", logger), ctx));

			_socket = new Socket(SocketType.Stream, ProtocolType.IP);
			_socket.Connect(IPAddress.Loopback, port);
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			_socket.Dispose();
			await _listenerTask.DisposeAsync();
			_listener.Dispose();
		}

		/// <summary>
		/// Sets up the loopback listener and calls the server method
		/// </summary>
		static async Task RunListenerAsync(Socket listener, DirectoryReference sandboxDir, bool executeInProcess, ILogger logger, CancellationToken cancellationToken)
		{
			using Socket tcpSocket = await listener.AcceptAsync(cancellationToken);

			using StorageCache storageCache = new StorageCache();

			await using (RemoteComputeSocket socket = new RemoteComputeSocket(new TcpTransport(tcpSocket), logger))
			{
				AgentMessageHandler worker = new AgentMessageHandler(sandboxDir, storageCache, null, executeInProcess, null, logger);
				await worker.RunAsync(socket, cancellationToken);
				await socket.CloseAsync(cancellationToken);
			}
		}

		/// <inheritdoc/>
		public Task<IComputeLease?> TryAssignWorkerAsync(ClusterId clusterId, Requirements? requirements, CancellationToken cancellationToken)
		{
#pragma warning disable CA2000 // Dispose objects before losing scope
			RemoteComputeSocket socket = new RemoteComputeSocket(new TcpTransport(_socket), new PrefixLogger("CLIENT", _logger));
			return Task.FromResult<IComputeLease?>(new LeaseImpl(socket));
#pragma warning restore CA2000 // Dispose objects before losing scope
		}
	}
}
