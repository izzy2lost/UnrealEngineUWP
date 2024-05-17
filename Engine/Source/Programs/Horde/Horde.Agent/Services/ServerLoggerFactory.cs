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
		/// <param name="localLogger">Local log output device</param>
		/// <param name="warnings">Whether to suppress warnings</param>
		/// <param name="outputLevel">Minimum output level for messages</param>
		/// <returns>New logger instance</returns>
		IServerLogger CreateLogger(IHordeClient hordeClient, LogId logId, ILogger localLogger, bool? warnings, LogLevel outputLevel = LogLevel.Information);
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
		public IServerLogger CreateLogger(IHordeClient hordeClient, LogId logId, ILogger localLogger, bool? warnings, LogLevel outputLevel)
		{
#pragma warning disable CA2000 // Dispose objects before losing scope
			return new ServerLogger(hordeClient, logId, warnings, outputLevel, localLogger, _logger);
#pragma warning restore CA2000 // Dispose objects before losing scope
		}
	}
}
