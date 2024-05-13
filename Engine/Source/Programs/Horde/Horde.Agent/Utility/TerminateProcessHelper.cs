// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Utility
{
	/// <summary>
	/// Flags for processes to terminate
	/// </summary>
	[Flags]
	public enum TerminateCondition
	{
		/// <summary>
		/// Not specified; terminate in all circumstances
		/// </summary>
		None = 0,

		/// <summary>
		/// When a session starts
		/// </summary>
		BeforeSession = 1,

		/// <summary>
		/// Before running a conform
		/// </summary>
		BeforeConform = 2,

		/// <summary>
		/// Before executing a batch
		/// </summary>
		BeforeBatch = 4,

		/// <summary>
		/// Terminate at the end of a batch
		/// </summary>
		AfterBatch = 8,

		/// <summary>
		/// After a step completes
		/// </summary>
		AfterStep = 16,
	}

	/// <summary>
	/// Utility methods for terminating processes
	/// </summary>
	public static class TerminateProcessHelper
	{
		/// <summary>
		/// Terminate processes matching certain criteria
		/// </summary>
		public static Task TerminateProcessesAsync(TerminateCondition condition, DirectoryReference workingDir, IReadOnlyDictionary<string, TerminateCondition> processNamesToTerminate, ILogger logger, CancellationToken cancellationToken)
		{
			// Terminate child processes from any previous runs
			ProcessUtils.TerminateProcesses(x => ShouldTerminateProcess(x, condition, workingDir, processNamesToTerminate), logger, cancellationToken);
			return Task.CompletedTask;
		}

		/// <summary>
		/// Callback for determining whether a process should be terminated
		/// </summary>
		static bool ShouldTerminateProcess(FileReference imageFile, TerminateCondition condition, DirectoryReference workingDir, IReadOnlyDictionary<string, TerminateCondition> processNamesToTerminate)
		{
			if (imageFile.IsUnderDirectory(workingDir))
			{
				return true;
			}

			string fileName = imageFile.GetFileName();
			if (processNamesToTerminate.TryGetValue(fileName, out TerminateCondition terminateFlags))
			{
				if (terminateFlags == TerminateCondition.None || (terminateFlags & condition) != 0)
				{
					return true;
				}
			}

			return false;
		}
	}
}
