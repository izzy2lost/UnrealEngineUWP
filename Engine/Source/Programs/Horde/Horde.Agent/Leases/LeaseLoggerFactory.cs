// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
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

		readonly DirectoryReference _logDir;
		readonly BackgroundTask _backgroundTask;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public LeaseLoggerFactory(IOptions<AgentSettings> settings, ILogger<LeaseLoggerFactory> logger)
		{
			_logDir = DirectoryReference.Combine(new DirectoryReference(settings.Value.WorkingDir ?? DirectoryReference.GetCurrentDirectory().FullName), "Leases");
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

		/// <summary>
		/// Create a new logger factory for the given lease id
		/// </summary>
		public ILoggerFactory CreateLoggerFactory(string leaseId)
		{
			return Logging.CreateFileLoggerFactory(_logDir, leaseId);
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
}
