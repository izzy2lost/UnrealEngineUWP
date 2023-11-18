// Copyright Epic Games, Inc. All Rights Reserved.

using System.Buffers;
using System.Diagnostics;
using System.Net.Sockets;
using EpicGames.Core;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Transports;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Agent.Leases.Handlers
{
	/// <summary>
	/// Handler for compute tasks
	/// </summary>
	class ComputeHandler : LeaseHandler<ComputeTask>
	{
		class TcpTransportWithTimeout : ComputeTransport
		{
			readonly TcpTransport _inner;
			long _lastPingTicks;

			static readonly double s_ticksToSystemTicks = (double)TimeSpan.TicksPerSecond / Stopwatch.Frequency;

			public TcpTransportWithTimeout(Socket socket)
			{
				_inner = new TcpTransport(socket);
				_lastPingTicks = Stopwatch.GetTimestamp();
			}

			public TimeSpan TimeSinceActivity => TimeSpan.FromTicks((long)((Stopwatch.GetTimestamp() - Interlocked.CompareExchange(ref _lastPingTicks, 0, 0)) * s_ticksToSystemTicks));

			public override ValueTask MarkCompleteAsync(CancellationToken cancellationToken) => _inner.MarkCompleteAsync(cancellationToken);

			public override async ValueTask<int> RecvAsync(Memory<byte> buffer, CancellationToken cancellationToken)
			{
				int result = await _inner.RecvAsync(buffer, cancellationToken);
				if (result > 0)
				{
					Interlocked.Exchange(ref _lastPingTicks, Stopwatch.GetTimestamp());
				}
				return result;
			}

			public override async ValueTask SendAsync(ReadOnlySequence<byte> buffer, CancellationToken cancellationToken)
			{
				await _inner.SendAsync(buffer, cancellationToken);
				Interlocked.Exchange(ref _lastPingTicks, Stopwatch.GetTimestamp());
			}
		}

		class CombinedLogger : ILogger
		{
			readonly ILogger[] _loggers;

			public CombinedLogger(params ILogger[] loggers) { _loggers = loggers; }

			public IDisposable? BeginScope<TState>(TState state) where TState : notnull => null!;

			public bool IsEnabled(LogLevel logLevel) => _loggers.Any(x => x.IsEnabled(logLevel));

			public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter)
			{
				foreach (ILogger logger in _loggers)
				{
					logger.Log<TState>(logLevel, eventId, state, exception, formatter);
				}
			}
		}

		static TimeSpan NoDataTimeout { get; } = TimeSpan.FromSeconds(20);

		readonly ComputeListenerService _listenerService;
		readonly IServerLoggerFactory _serverLoggerFactory;
		readonly AgentSettings _settings;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeHandler(ComputeListenerService listenerService, IServerLoggerFactory serverLoggerFactory, IOptions<AgentSettings> settings, ILogger<ComputeHandler> logger)
		{
			_listenerService = listenerService;
			_serverLoggerFactory = serverLoggerFactory;
			_settings = settings.Value;
			_logger = logger;
		}

		/// <inheritdoc/>
		public override async Task<LeaseResult> ExecuteAsync(ISession session, string leaseId, ComputeTask computeTask, CancellationToken cancellationToken)
		{
			await using IServerLogger? serverLogger = (computeTask.LogId != null)? _serverLoggerFactory.CreateLogger(session, computeTask.LogId, null, true, LogLevel.Trace) : null;

			ILogger logger = _logger;
			if (serverLogger != null)
			{
				logger = new CombinedLogger(serverLogger, logger);
			}

			if (!String.IsNullOrEmpty(computeTask.ParentLeaseId))
			{
				logger.LogInformation("Parent lease: {LeaseId}", computeTask.ParentLeaseId);
			}

			logger.LogInformation("Starting compute task (lease {LeaseId}). Waiting for connection with nonce {Nonce}...", leaseId, StringUtils.FormatHexString(computeTask.Nonce.Span));
			ClearTerminationSignalFile();

			TcpClient? tcpClient = null;
			try
			{
				const int TimeoutSeconds = 30;

				tcpClient = await _listenerService.WaitForClientAsync(new ByteString(computeTask.Nonce.Memory), TimeSpan.FromSeconds(TimeoutSeconds), cancellationToken);
				if (tcpClient == null)
				{
					logger.LogInformation("Timed out waiting for connection after {Time}s.", TimeoutSeconds); 
					return LeaseResult.Success;
				}

				logger.LogInformation("Matched connection for {Nonce}", StringUtils.FormatHexString(computeTask.Nonce.Span));

				TcpTransportWithTimeout transport = new TcpTransportWithTimeout(tcpClient.Client);
				using (CancellationTokenSource cts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken))
				{
					await using BackgroundTask timeoutTask = BackgroundTask.StartNew(ctx => TickTimeoutAsync(transport, cts, logger, ctx));
					try
					{
						await using (RemoteComputeSocket socket = new RemoteComputeSocket(transport, logger))
						{
							DirectoryReference sandboxDir = DirectoryReference.Combine(session.WorkingDir, "Sandbox", leaseId);
							try
							{
								DirectoryReference.CreateDirectory(sandboxDir);

								DirectoryReference sharedDir = DirectoryReference.Combine(session.WorkingDir, "Saved");
								DirectoryReference.CreateDirectory(sharedDir);

								Dictionary<string, string?> newEnvVars = new Dictionary<string, string?>();
								newEnvVars["UE_HORDE_SHARED_DIR"] = sharedDir.FullName;
								newEnvVars["UE_HORDE_TERMINATION_SIGNAL_FILE"] = _settings.GetTerminationSignalFile().FullName;

								AgentMessageHandler worker = new AgentMessageHandler(sandboxDir, newEnvVars, false, _settings.WineExecutablePath, serverLogger ?? _logger);
								await worker.RunAsync(socket, cts.Token);
								await socket.CloseAsync(cts.Token);
								return LeaseResult.Success;
							}
							finally
							{
								FileUtils.ForceDeleteDirectory(sandboxDir);
							}
						}
					}
					catch (OperationCanceledException ex) when (cts.IsCancellationRequested && transport.TimeSinceActivity > NoDataTimeout)
					{
						logger.LogError(ex, "Lease was terminated due to no data being received for {Time} seconds", (int)NoDataTimeout.TotalSeconds);
						return LeaseResult.Failed;
					}
				}
			}
			catch (Exception ex)
			{
				logger.LogError(ex, "Exception while executing compute task: {Message}", ex.Message);
				return LeaseResult.Failed;
			}
			finally
			{
				tcpClient?.Dispose();
			}
		}

		private void ClearTerminationSignalFile()
		{
			string path = _settings.GetTerminationSignalFile().FullName;
			try
			{
				File.Delete(path);
			}
			catch (Exception e)
			{
				// If this file is not removed and lingers on from previous executions,
				// new compute tasks may pick it up and erroneously decide to terminate.
				_logger.LogError(e, "Unable to delete termination signal file {Path}", path);
			}
		}

		static async Task TickTimeoutAsync(TcpTransportWithTimeout transport, CancellationTokenSource cts, ILogger logger, CancellationToken cancellationToken)
		{
			while(!cancellationToken.IsCancellationRequested)
			{
				TimeSpan reaminingTime = NoDataTimeout - transport.TimeSinceActivity;
				if (reaminingTime < TimeSpan.Zero)
				{
					logger.LogWarning("Terminating compute task due to timeout (last tick at {Time})", DateTime.UtcNow - transport.TimeSinceActivity);
					cts.Cancel();
					break;
				}
				await Task.Delay(reaminingTime + TimeSpan.FromSeconds(0.2), cancellationToken);
			}
		}
	}
}

