// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// Stats reported for copy operations
	/// </summary>
	public interface ICopyStats
	{
		/// <summary>
		/// Number of files that have been copied
		/// </summary>
		int Count { get; }

		/// <summary>
		/// Total size of data to be copied
		/// </summary>
		long Size { get; }

		/// <summary>
		/// Processing speed, in bytes per second
		/// </summary>
		double Rate { get; }
	}

	/// <summary>
	/// Reports progress info back to callers
	/// </summary>
	class CopyStats : ICopyStats
	{
		readonly object _lockObject = new object();
		readonly Stopwatch _timer = Stopwatch.StartNew();
		readonly IProgress<ICopyStats> _progress;
		long _lastTotalSize;

		public int Count { get; set; }
		public long Size { get; set; }
		public double Rate { get; set; }

		public CopyStats(IProgress<ICopyStats> progress)
		{
			_progress = progress;
		}

		public void Update(int count, long size)
		{
			lock (_lockObject)
			{
				Count += count;
				Size += size;
				if (_timer.Elapsed > TimeSpan.FromSeconds(10.0))
				{
					Rate = (Size - _lastTotalSize) / _timer.Elapsed.TotalSeconds;
					_lastTotalSize = Size;

					_progress.Report(this);
					_timer.Restart();
				}
			}
		}

		public void Flush()
		{
			lock (_lockObject)
			{
				_progress.Report(this);
				_timer.Restart();
			}
		}
	}

	/// <summary>
	/// Progress logger for writing copy stats
	/// </summary>
	public class CopyStatsLogger : IProgress<ICopyStats>
	{
		readonly int _totalCount;
		readonly long _totalSize;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public CopyStatsLogger(ILogger logger)
			=> _logger = logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public CopyStatsLogger(int totalCount, long totalSize, ILogger logger)
		{
			_totalCount = totalCount;
			_totalSize = totalSize;
			_logger = logger;
		}

		/// <inheritdoc/>
		public void Report(ICopyStats stats)
		{
			if (_totalCount > 0 && _totalSize > 0)
			{
				_logger.LogInformation("Copied {NumFiles:n0}/{TotalFiles:n0} files ({Size:n1}/{TotalSize:n1}mb, {Rate:n1}mb/s, {Pct}%)", stats.Count, _totalCount, stats.Size / (1024.0 * 1024.0), _totalSize / (1024.0 * 1024.0), stats.Rate / (1024.0 * 1024.0), (int)((Math.Max(stats.Size, 1) * 100) / Math.Max(_totalSize, 1)));
			}
			else if (_totalCount > 0)
			{
				_logger.LogInformation("Copied {NumFiles:n0}/{TotalFiles:n0} files ({Size:n1}mb, {Rate:n1}mb/s)", stats.Count, _totalCount, stats.Size / (1024.0 * 1024.0), stats.Rate / (1024.0 * 1024.0));
			}
			else if (_totalSize > 0)
			{
				_logger.LogInformation("Copied {NumFiles:n0} files ({Size:n1}/{TotalSize:n1}mb, {Rate:n1}mb/s, {Pct}%)", stats.Count, stats.Size / (1024.0 * 1024.0), _totalSize / (1024.0 * 1024.0), stats.Rate / (1024.0 * 1024.0), (int)((Math.Max(stats.Size, 1) * 100) / Math.Max(_totalSize, 1)));
			}
			else
			{
				_logger.LogInformation("Copied {NumFiles:n0} files ({Size:n1}mb, {Rate:n1}mb/s)", stats.Count, stats.Size / (1024.0 * 1024.0), stats.Rate / (1024.0 * 1024.0));
			}
		}
	}

	/// <summary>
	/// Extension methods for extracting data from directory nodes
	/// </summary>
	public static class DirectoryNodeExtract
	{
		/// <summary>
		/// Utility function to allow extracting a packed directory to disk
		/// </summary>
		/// <param name="directoryNode">Directory to update</param>
		/// <param name="directoryInfo"></param>
		/// <param name="logger"></param>
		/// <param name="cancellationToken"></param>
		public static Task CopyToDirectoryAsync(this DirectoryNode directoryNode, DirectoryInfo directoryInfo, ILogger logger, CancellationToken cancellationToken) => CopyToDirectoryAsync(directoryNode, directoryInfo, null, logger, cancellationToken);

		record class OutputDir(string Path, DirectoryNode Node);
		record class OutputFile(OutputDir Directory, FileEntry FileEntry);
		record class OutputChunk(OutputFile File, long Offset, long Length, IBlobHandle Handle);

#pragma warning disable IDE0060
		static void TraceBlobRead(string type, string path, IBlobHandle handle, ILogger logger)
		{
			//			logger.LogTrace(KnownLogEvents.Horde_BlobRead, "Blob [{Type,-20}] Path=\"{Path}\", Locator={Locator}", type, path, handle.GetLocator());
		}
#pragma warning restore IDE0060

		/// <summary>
		/// Utility function to allow extracting a packed directory to disk
		/// </summary>
		/// <param name="directoryNode">Directory to update</param>
		/// <param name="directoryInfo">Direcotry to write to</param>
		/// <param name="progress">Sink for progress updates</param>
		/// <param name="logger">Logger for output</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async Task CopyToDirectoryAsync(this DirectoryNode directoryNode, DirectoryInfo directoryInfo, IProgress<ICopyStats>? progress, ILogger logger, CancellationToken cancellationToken)
		{
			int numTasks = Math.Min(1 + (int)(directoryNode.Length / (16 * 1024 * 1024)), 16);
			logger.LogInformation("Splitting read into {NumThreads} threads", numTasks);

			CopyStats? copyStats = null;
			if (progress != null)
			{
				copyStats = new CopyStats(progress);
			}

			Channel<OutputChunk> chunks = Channel.CreateBounded<OutputChunk>(new BoundedChannelOptions(128 * 1024) { FullMode = BoundedChannelFullMode.Wait });
			using (CancellationTokenSource cancellationSource = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken))
			{
				// Helper method to run a background task and set a cancellation source on error
				async Task RunBackgroundTask(Func<CancellationToken, Task> taskFunc)
				{
					try
					{
						await taskFunc(cancellationSource.Token);
					}
					catch (OperationCanceledException)
					{
						// Ignore
					}
					catch (Exception ex)
					{
						logger.LogError(ex, "Error while extracting data: {Message}", ex.Message);
						cancellationSource.Cancel();
					}
				}

				List<Task> tasks = new List<Task>();
				tasks.Add(RunBackgroundTask(ctx => FindOutputChunksRootAsync(directoryNode, chunks.Writer, logger, ctx)));
				for (int idx = 0; idx < numTasks; idx++)
				{
					tasks.Add(RunBackgroundTask(ctx => ExtractAsync(chunks.Reader, new DirectoryReference(directoryInfo), copyStats, logger, ctx)));
				}

				await Task.WhenAll(tasks);
			}
		}

		static async Task ExtractAsync(ChannelReader<OutputChunk> chunkReader, DirectoryReference baseDir, CopyStats? copyStats, ILogger logger, CancellationToken cancellationToken)
		{
			OutputChunk? chunk = await ReadNextChunkAsync(chunkReader, cancellationToken);
			while (chunk != null)
			{
				// Open the file for the current chunk
				OutputFile file = chunk.File;

				FileReference locator = FileReference.Combine(baseDir, file.Directory.Path, file.FileEntry.Name);
				DirectoryReference.CreateDirectory(locator.Directory);

				FileInfo fileInfo = locator.ToFileInfo();
				if (fileInfo.Exists && (fileInfo.Attributes & FileAttributes.ReadOnly) != 0)
				{
					fileInfo.Attributes &= ~FileAttributes.ReadOnly;
				}

				await using (FileStream stream = fileInfo.Open(FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.ReadWrite))
				{
					stream.SetLength(file.FileEntry.Length);
					if (file.FileEntry.Length == 0)
					{
						// If this file is empty, don't write anything and just move to the next chunk
						chunk = await ReadNextChunkAsync(chunkReader, cancellationToken);
					}
					else
					{
						// Process as many chunks as we can for this file
						using MemoryMappedFile memoryMappedFile = MemoryMappedFile.CreateFromFile(stream, null, file.FileEntry.Length, MemoryMappedFileAccess.ReadWrite, HandleInheritability.None, false);
						using MemoryMappedView memoryMappedView = new MemoryMappedView(memoryMappedFile, 0, file.FileEntry.Length);

						while (chunk != null && chunk.File == file)
						{
							// Write this chunk
							using (BlobData data = await chunk.Handle.ReadBlobDataAsync(cancellationToken))
							{
								TraceBlobRead("Leaf", CombinePaths(chunk.File.Directory.Path, chunk.File.FileEntry.Name), chunk.Handle, logger);
								data.Data.CopyTo(memoryMappedView!.GetMemory(chunk.Offset, data.Data.Length));
							}

							// Update the stats
							int numCompleteFiles = 0;
							if (chunk.Offset + chunk.Length == file.FileEntry.Length)
							{
								numCompleteFiles = 1;
							}

							copyStats?.Update(numCompleteFiles, chunk.Length);

							// Read the next chunk
							chunk = await ReadNextChunkAsync(chunkReader, cancellationToken);
						}
					}
				}

				// Set correct permissions on the output file
				FileEntry.SetPermissions(fileInfo, file.FileEntry.Flags);
			}
		}

		static async ValueTask<OutputChunk?> ReadNextChunkAsync(ChannelReader<OutputChunk> chunkReader, CancellationToken cancellationToken)
		{
			await chunkReader.WaitToReadAsync(cancellationToken);

			OutputChunk? chunk;
			if (chunkReader.TryRead(out chunk))
			{
				return chunk;
			}
			else
			{
				return null;
			}
		}

		static async Task FindOutputChunksRootAsync(DirectoryNode node, ChannelWriter<OutputChunk> chunks, ILogger logger, CancellationToken cancellationToken)
		{
			await FindOutputChunksAsync("", node, chunks, logger, cancellationToken);
			chunks.Complete();
		}

		static async Task FindOutputChunksAsync(string path, DirectoryNode node, ChannelWriter<OutputChunk> chunks, ILogger logger, CancellationToken cancellationToken)
		{
			OutputDir outputDir = new OutputDir(path, node);

			foreach (FileEntry fileEntry in node.Files)
			{
				OutputFile outputFile = new OutputFile(outputDir, fileEntry);
				await FindOutputChunksAsync(outputFile, 0, fileEntry.Target, chunks, logger, cancellationToken);
			}

			foreach (DirectoryEntry directoryEntry in node.Directories)
			{
				DirectoryNode subDirectoryNode = await directoryEntry.Handle.ReadBlobAsync(cancellationToken);

				string subPath = CombinePaths(outputDir.Path, directoryEntry.Name);
				TraceBlobRead("Directory", subPath, directoryEntry.Handle, logger);

				await FindOutputChunksAsync(subPath, subDirectoryNode, chunks, logger, cancellationToken);
			}
		}

		static async Task<long> FindOutputChunksAsync(OutputFile outputFile, long offset, ChunkedDataNodeRef dataRef, ChannelWriter<OutputChunk> chunks, ILogger logger, CancellationToken cancellationToken)
		{
			if (dataRef.Type == ChunkedDataNodeType.Leaf)
			{
				await chunks.WriteAsync(new OutputChunk(outputFile, offset, dataRef.Length, dataRef.Handle), cancellationToken);
				if (dataRef.Length < 0)
				{
					// Backwards compatibility hack for v2 format
					using BlobData data = await dataRef.Handle.ReadBlobDataAsync(cancellationToken);
					return data.Data.Length;
				}
				return dataRef.Length;
			}
			else
			{
				using BlobData data = await dataRef.Handle.ReadBlobDataAsync(cancellationToken);
				TraceBlobRead("Interior", CombinePaths(outputFile.Directory.Path, outputFile.FileEntry.Name), dataRef.Handle, logger);

				if (data.Type.Guid == LeafChunkedDataNodeConverter.BlobType.Guid)
				{
					await chunks.WriteAsync(new OutputChunk(outputFile, offset, dataRef.Length, dataRef.Handle), cancellationToken);
					return data.Data.Length;
				}
				else
				{
					long length = 0;

					InteriorChunkedDataNode interiorNode = BlobSerializer.Deserialize<InteriorChunkedDataNode>(data);
					foreach (ChunkedDataNodeRef childRef in interiorNode.Children)
					{
						length += await FindOutputChunksAsync(outputFile, offset + length, childRef, chunks, logger, cancellationToken);
					}

					return length;
				}
			}
		}

		static string CombinePaths(string basePath, string nextPath)
		{
			if (basePath.Length > 0)
			{
				return $"{basePath}/{nextPath}";
			}
			else
			{
				return nextPath;
			}
		}
	}
}

