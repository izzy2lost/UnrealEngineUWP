// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Sessions;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Storage;

namespace Horde.Server.Logs
{
	/// <summary>
	/// Information about a log file
	/// </summary>
	public interface ILog
	{
		/// <summary>
		/// Identifier for the log. Randomly generated.
		/// </summary>
		public LogId Id { get; }

		/// <summary>
		/// Unique id of the job containing this log
		/// </summary>
		public JobId JobId { get; }

		/// <summary>
		/// The lease allowed to write to this log
		/// </summary>
		public LeaseId? LeaseId { get; }

		/// <summary>
		/// The session allowed to write to this log
		/// </summary>
		public SessionId? SessionId { get; }

		/// <summary>
		/// Type of data stored in this log 
		/// </summary>
		public LogType Type { get; }

		/// <summary>
		/// Namespace containing the log data
		/// </summary>
		public NamespaceId NamespaceId { get; }

		/// <summary>
		/// Name of the ref used to store data for this log
		/// </summary>
		public RefName RefName { get; }

		//		/// <summary>
		//		/// Number of lines (V2 storage backend)
		//		/// </summary>
		//		public int LineCount { get; }

		//		/// <summary>
		//		/// Whether the log is complete (V2 storage backend)
		//		/// </summary>
		//		public bool Complete { get; }

		/// <summary>
		/// Read a set of lines from the given log file
		/// </summary>
		/// <param name="index">Index of the first line to read</param>
		/// <param name="count">Maximum number of lines to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of lines</returns>
		Task<List<Utf8String>> ReadLinesAsync(int index, int count, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets metadata about the log file
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Metadata about the log file</returns>
		Task<LogMetadata> GetMetadataAsync(CancellationToken cancellationToken);

		/// <summary>
		/// Updates the line count for a log file (v2 backend only)
		/// </summary>
		/// <param name="lineCount">New line count for the log file</param>
		/// <param name="complete">Flag indicating whether the log is complete, or can still be tailed for additional data</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>The updated log file document</returns>
		Task<ILog> UpdateLineCountAsync(int lineCount, bool complete, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets lines from the given log 
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Data for the requested range</returns>
		Task<Stream> OpenRawStreamAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets lines from the given log 
		/// </summary>
		/// <param name="offset"></param>
		/// <param name="length"></param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Data for the requested range</returns>
		Task<Stream> OpenRawStreamAsync(long offset, long length, CancellationToken cancellationToken);

		/// <summary>
		/// Parses a stream of json text and outputs plain text
		/// </summary>
		/// <param name="outputStream">Output stream to receive the text data</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Async text</returns>
		Task CopyPlainTextStreamAsync(Stream outputStream, CancellationToken cancellationToken = default);

		/// <summary>
		/// Search for the specified text in a log file
		/// </summary>
		/// <param name="text">Text to search for</param>
		/// <param name="firstLine">Line to start search from</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="stats">Receives stats for the search</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>List of line numbers containing the given term</returns>
		Task<List<int>> SearchLogDataAsync(string text, int firstLine, int count, SearchStats stats, CancellationToken cancellationToken);
	}

	/// <summary>
	/// Metadata about a log file
	/// </summary>
	public class LogMetadata
	{
		/// <summary>
		/// Length of the log file
		/// </summary>
		public long Length { get; set; }

		/// <summary>
		/// Number of lines in the log file
		/// </summary>
		public int MaxLineIndex { get; set; }
	}

	/// <summary>
	/// Extension methods for log files
	/// </summary>
	public static class LogExtensions
	{
		/// <summary>
		/// Parses a stream of json text and outputs plain text
		/// </summary>
		/// <param name="log">The log file to query</param>
		/// <param name="outputStream">Output stream to receive the text data</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Async text</returns>
		public static async Task CopyRawStreamAsync(this ILog log, Stream outputStream, CancellationToken cancellationToken)
		{
			await using Stream stream = await log.OpenRawStreamAsync(cancellationToken);
			await stream.CopyToAsync(outputStream, cancellationToken);
		}
	}
}
