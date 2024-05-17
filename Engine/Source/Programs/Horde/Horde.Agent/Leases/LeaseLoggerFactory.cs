// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Logs;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Agent.Leases
{
	/// <summary>
	/// Creates local log files for leases executed on this agent
	/// </summary>
	sealed class LeaseLoggerFactory : IAsyncDisposable
	{
		static TimeSpan MaxAge { get; } = TimeSpan.FromDays(3.0);

		readonly AgentSettings _settings;
		readonly DirectoryReference _logDir;
		readonly BackgroundTask _backgroundTask;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public LeaseLoggerFactory(IOptions<AgentSettings> settings, ILogger<LeaseLoggerFactory> logger)
		{
			_settings = settings.Value;
			_logDir = DirectoryReference.Combine(settings.Value.LogsDir, "Leases");
			_logger = logger;
			_backgroundTask = BackgroundTask.StartNew(BackgroundCleanupAsync);
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			await _backgroundTask.DisposeAsync();
		}

		/// <summary>
		/// Remove all log files older than the given time
		/// </summary>
		public void Cleanup(TimeSpan maxAge)
		{
			try
			{
				DirectoryInfo directoryInfo = _logDir.ToDirectoryInfo();
				if (directoryInfo.Exists)
				{
					DateTime cleanTimeUtc = DateTime.UtcNow - maxAge;
					foreach (FileInfo fileInfo in _logDir.ToDirectoryInfo().EnumerateFiles())
					{
						if (fileInfo.LastWriteTimeUtc < cleanTimeUtc)
						{
							try
							{
								fileInfo.Delete();
							}
							catch (Exception ex)
							{
								_logger.LogInformation(ex, "Error deleting log file {File}: {Message}", fileInfo.FullName, ex.Message);
							}
						}
					}
				}
			}
			catch (Exception ex)
			{
				_logger.LogInformation(ex, "Error deleting log file in {Dir}: {Message}", _logDir, ex.Message);
			}
		}

		class LoggerProvider : ILoggerProvider
		{
			readonly ILogger _logger;

			public LoggerProvider(ILogger logger)
				=> _logger = logger;

			public ILogger CreateLogger(string categoryName)
				=> _logger;

			public void Dispose() { }
		}

		/// <summary>
		/// Create a new logger factory for the given lease id
		/// </summary>
		public ILoggerFactory CreateLoggerFactory(LeaseId leaseId)
		{
			return LoggerFactory.Create(builder =>
			{
				builder.AddProvider(Logging.CreateFileLoggerProvider(_logDir, leaseId.ToString()));
				if (_settings.WriteStepOutputToLogger)
				{
					builder.AddProvider(new LoggerProvider(_logger));
				}
			});
		}

		async Task BackgroundCleanupAsync(CancellationToken cancellationToken)
		{
			while (!cancellationToken.IsCancellationRequested)
			{
				Cleanup(MaxAge);
				await Task.Delay(TimeSpan.FromHours(1.0), cancellationToken);
			}
		}
	}

	static class LeaseLoggerExtensions
	{
		sealed class ServerLoggerWithLeaseLogger : IServerLogger
		{
			readonly IServerLogger _serverLogger;
			readonly ILogger _localLogger;

			public ServerLoggerWithLeaseLogger(IServerLogger serverLogger, ILogger localLogger)
			{
				_serverLogger = serverLogger;
				_localLogger = localLogger;
			}

			/// <inheritdoc/>
			public IDisposable? BeginScope<TState>(TState state) where TState : notnull
				=> _localLogger.BeginScope(state);

			/// <inheritdoc/>
			public ValueTask DisposeAsync()
				=> _serverLogger.DisposeAsync();

			/// <inheritdoc/>
			public bool IsEnabled(LogLevel logLevel)
				=> _serverLogger.IsEnabled(logLevel) || _localLogger.IsEnabled(logLevel);

			/// <inheritdoc/>
			public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter)
			{
				_serverLogger.Log(logLevel, eventId, state, exception, formatter);
				_localLogger.Log(logLevel, eventId, state, exception, formatter);
			}

			/// <inheritdoc/>
			public Task StopAsync()
				=> _serverLogger.StopAsync();
		}

		/// <summary>
		/// Creates a logger which uploads data to the server
		/// </summary>
		/// <param name="serverLogger">Logger for the server</param>
		/// <param name="localLogger">Local log output device</param>
		/// <returns>New logger instance</returns>
		public static IServerLogger WithLocalLogger(this IServerLogger serverLogger, ILogger localLogger)
		{
			return new ServerLoggerWithLeaseLogger(serverLogger, localLogger);
		}
	}
}
