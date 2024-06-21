// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;
using EpicGames.Horde.Logs;
using HordeServer.Logs.Data;

namespace HordeServer.Logs.Storage
{
	/// <summary>
	/// Empty implementation of log storage
	/// </summary>
	public sealed class NullLogStorage : ILogStorage
	{
		/// <inheritdoc/>
		public void Dispose()
		{
		}

		/// <inheritdoc/>
		public Task<LogIndexData?> ReadIndexAsync(LogId logId, long length)
		{
			return Task.FromResult<LogIndexData?>(null);
		}

		/// <inheritdoc/>
		public Task<LogChunkData?> ReadChunkAsync(LogId logId, long offset, int lineIndex)
		{
			return Task.FromResult<LogChunkData?>(null);
		}
	}
}
