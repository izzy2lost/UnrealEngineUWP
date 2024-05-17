// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde;
using EpicGames.Horde.Logs;
using Horde.Agent.Utility;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Services
{
	/// <summary>
	/// Interface for creating <see cref="IServerLogger"/> instances
	/// </summary>
	interface IServerLoggerFactory
	{
		/// <summary>
		/// Creates a logger which uploads data to the server
		/// </summary>
		/// <param name="hordeClient">The Horde client</param>
		/// <param name="logId">The log id</param>
		/// <param name="minimumLevel">Minimum output level for messages</param>
		/// <returns>New logger instance</returns>
		IServerLogger CreateLogger(IHordeClient hordeClient, LogId logId, LogLevel minimumLevel = LogLevel.Information);
	}

	/// <summary>
	/// Implementation of <see cref="IServerLoggerFactory"/>
	/// </summary>
	class ServerLoggerFactory : IServerLoggerFactory
	{
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public ServerLoggerFactory(ILogger<ServerLoggerFactory> logger)
		{
			_logger = logger;
		}

		/// <inheritdoc/>
		public IServerLogger CreateLogger(IHordeClient hordeClient, LogId logId, LogLevel minimumLevel)
		{
#pragma warning disable CA2000 // Dispose objects before losing scope
			return new ServerLogger(hordeClient, logId, minimumLevel, _logger);
#pragma warning restore CA2000 // Dispose objects before losing scope
		}
	}

	static class ServerLoggerExtensions
	{
		sealed class ServerLoggerWithLocalLogger : IServerLogger
		{
			readonly IServerLogger _serverLogger;
			readonly ILogger _localLogger;

			public ServerLoggerWithLocalLogger(IServerLogger serverLogger, ILogger localLogger)
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
			return new ServerLoggerWithLocalLogger(serverLogger, localLogger);
		}
	}
}
