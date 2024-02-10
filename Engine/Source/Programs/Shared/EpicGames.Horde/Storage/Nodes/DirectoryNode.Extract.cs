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

		class OutputFile
		{
			public string Path { get; }
			public FileInfo FileInfo { get; }
			public FileEntry FileEntry { get; }

			bool _createdFile;
			int _remainingChunks;

			public OutputFile(string path, FileInfo fileInfo, FileEntry fileEntry)
			{
				Path = path;
				FileInfo = fileInfo;
				FileEntry = fileEntry;
			}

			public int IncrementRemaining() => Interlocked.Increment(ref _remainingChunks);
			public int DecrementRemaining() => Interlocked.Decrement(ref _remainingChunks);

			public FileStream OpenStream()
			{
				lock (FileEntry)
				{
					if (!_createdFile)
					{
						if (FileInfo.Exists)
						{
							if ((FileInfo.Attributes & FileAttributes.ReadOnly) != 0)
							{
								FileInfo.Attributes &= ~FileAttributes.ReadOnly;
							}
							if (FileInfo.LinkTarget != null)
							{
								FileInfo.Delete();
							}
						}
						else
						{
							FileInfo.Directory?.Create();
						}
					}

					FileStream? stream = null;
					try
					{
						stream = FileInfo.Open(FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.ReadWrite);
						if (!_createdFile)
						{
							stream.SetLength(FileEntry.Length);
							_createdFile = true;
						}
						return stream;
					}
					catch
					{
						stream?.Dispose();
						throw;
					}
				}
			}
		}

		record class OutputChunk(OutputFile File, long Offset, long Length, IBlobHandle Handle);

		record class OutputBatch(List<OutputChunk> Chunks);

		// Writes output chunks to a channel. Buffers one chunk until FlushAsync() is called to ensure
		// the remaining chunk reference count doesn't reach zero until the last chunk has been processed.
		class OutputChunkWriter
		{
			public OutputFile OutputFile { get; }

			readonly ChannelWriter<OutputChunk> _chunkWriter;
			OutputChunk? _bufferedChunk;

			public OutputChunkWriter(OutputFile file, ChannelWriter<OutputChunk> chunkWriter)
			{
				OutputFile = file;
				_chunkWriter = chunkWriter;
			}

			public async Task WriteAsync(long offset, long length, IBlobHandle handle, CancellationToken cancellationToken)
			{
				if (_bufferedChunk != null)
				{
					await _chunkWriter.WriteAsync(_bufferedChunk, cancellationToken);
				}

				OutputFile.IncrementRemaining();
				_bufferedChunk = new OutputChunk(OutputFile, offset, length, handle);
			}

			public async Task FlushAsync(CancellationToken cancellationToken)
			{
				if (_bufferedChunk != null)
				{
					await _chunkWriter.WriteAsync(_bufferedChunk, cancellationToken);
					_bufferedChunk = null;
				}
			}
		}

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

				Channel<OutputChunk> chunks = Channel.CreateBounded<OutputChunk>(new BoundedChannelOptions(128 * 1024) { FullMode = BoundedChannelFullMode.Wait });
				tasks.Add(RunBackgroundTask(ctx => FindOutputChunksRootAsync(directoryInfo, directoryNode, chunks.Writer, logger, ctx)));

				Channel<OutputBatch> batches = Channel.CreateUnbounded<OutputBatch>();
				tasks.Add(RunBackgroundTask(ctx => BatchReadRequestsAsync(chunks.Reader, batches.Writer, ctx)));

				for (int idx = 0; idx < numTasks; idx++)
				{
					tasks.Add(RunBackgroundTask(ctx => ExtractAsync(batches.Reader, copyStats, logger, ctx)));
				}

				await Task.WhenAll(tasks);
			}
		}

		static async Task ExtractAsync(ChannelReader<OutputBatch> batchReader, CopyStats? copyStats, ILogger logger, CancellationToken cancellationToken)
		{
			while (await batchReader.WaitToReadAsync(cancellationToken))
			{
				OutputBatch? batch;
				while (batchReader.TryRead(out batch))
				{
					for (int chunkIdx = 0; chunkIdx < batch.Chunks.Count; )
					{
						OutputFile file = batch.Chunks[chunkIdx].File;

						int maxChunkIdx = chunkIdx + 1;
						while (maxChunkIdx < batch.Chunks.Count && batch.Chunks[maxChunkIdx].File == file)
						{
							maxChunkIdx++;
						}

						try
						{
							await ExtractChunksToFileAsync(file, batch.Chunks.Slice(chunkIdx, maxChunkIdx - chunkIdx), copyStats, logger, cancellationToken);
						}
						catch (OperationCanceledException)
						{
							throw;
						}
						catch (Exception ex)
						{
							throw new StorageException($"Unable to extract {file?.FileInfo?.FullName}: {ex.Message}", ex);
						}

						chunkIdx = maxChunkIdx;
					}
				}
			}
		}

		static async Task ExtractChunksToFileAsync(OutputFile file, ListSegment<OutputChunk> chunks, CopyStats? copyStats, ILogger logger, CancellationToken cancellationToken)
		{
			// Open the file for the current chunk
			int remainingChunks = 0;
			await using (FileStream stream = file.OpenStream())
			{
				if (file.FileEntry.Length == 0)
				{
					// If this file is empty, don't write anything and just move to the next chunk
					remainingChunks = file.DecrementRemaining();
				}
				else
				{
					// Process as many chunks as we can for this file
					using MemoryMappedFile memoryMappedFile = MemoryMappedFile.CreateFromFile(stream, null, file.FileEntry.Length, MemoryMappedFileAccess.ReadWrite, HandleInheritability.None, false);
					using MemoryMappedView memoryMappedView = new MemoryMappedView(memoryMappedFile, 0, file.FileEntry.Length);

					for(int chunkIdx = 0; chunkIdx < chunks.Count; chunkIdx++)
					{
						OutputChunk chunk = chunks[chunkIdx];

						// Write this chunk
						using (BlobData data = await chunk.Handle.ReadBlobDataAsync(cancellationToken))
						{
							TraceBlobRead("Leaf", chunk.File.Path, chunk.Handle, logger);
							data.Data.CopyTo(memoryMappedView!.GetMemory(chunk.Offset, data.Data.Length));
						}

						// Update the stats
						remainingChunks = file.DecrementRemaining();
						copyStats?.Update(0, chunk.Length);
					}
				}
			}

			// Set correct permissions on the output file
			if (remainingChunks == 0)
			{
				FileEntry.SetPermissions(file.FileInfo!, file.FileEntry.Flags);
				copyStats?.Update(1, 0);
			}
		}

		static async Task FindOutputChunksRootAsync(DirectoryInfo rootDir, DirectoryNode node, ChannelWriter<OutputChunk> chunks, ILogger logger, CancellationToken cancellationToken)
		{
			await FindOutputChunksForDirectoryAsync(rootDir, "", node, chunks, logger, cancellationToken);
			chunks.Complete();
		}

		static async Task FindOutputChunksForDirectoryAsync(DirectoryInfo rootDir, string path, DirectoryNode node, ChannelWriter<OutputChunk> chunks, ILogger logger, CancellationToken cancellationToken)
		{
			foreach (FileEntry fileEntry in node.Files)
			{
				string filePath = CombinePaths(path, fileEntry.Name);
				FileInfo fileInfo = new FileInfo(Path.Combine(rootDir.FullName, filePath));
				OutputFile outputFile = new OutputFile(filePath, fileInfo, fileEntry);

				await FindOutputChunksForFileAsync(outputFile, chunks, logger, cancellationToken);
			}

			foreach (DirectoryEntry directoryEntry in node.Directories)
			{
				string subPath = CombinePaths(path, directoryEntry.Name);
				TraceBlobRead("Directory", subPath, directoryEntry.Handle, logger);
				DirectoryNode subDirectoryNode = await directoryEntry.Handle.ReadBlobAsync(cancellationToken);

				await FindOutputChunksForDirectoryAsync(rootDir, subPath, subDirectoryNode, chunks, logger, cancellationToken);
			}
		}

		static async Task FindOutputChunksForFileAsync(OutputFile outputFile, ChannelWriter<OutputChunk> chunks, ILogger logger, CancellationToken cancellationToken)
		{
			OutputChunkWriter outputWriter = new OutputChunkWriter(outputFile, chunks);
			await FindOutputChunksAsync(outputWriter, 0, outputFile.FileEntry.Target, logger, cancellationToken);
			await outputWriter.FlushAsync(cancellationToken);
		}

		static async Task<long> FindOutputChunksAsync(OutputChunkWriter chunkWriter, long offset, ChunkedDataNodeRef dataRef, ILogger logger, CancellationToken cancellationToken)
		{
			if (dataRef.Type == ChunkedDataNodeType.Leaf)
			{
				await chunkWriter.WriteAsync(offset, dataRef.Length, dataRef.Handle, cancellationToken);
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
				TraceBlobRead("Interior", chunkWriter.OutputFile.Path, dataRef.Handle, logger);

				if (data.Type.Guid == LeafChunkedDataNodeConverter.BlobType.Guid)
				{
					await chunkWriter.WriteAsync(offset, dataRef.Length, dataRef.Handle, cancellationToken);
					return data.Data.Length;
				}
				else
				{
					long length = 0;

					InteriorChunkedDataNode interiorNode = BlobSerializer.Deserialize<InteriorChunkedDataNode>(data);
					foreach (ChunkedDataNodeRef childRef in interiorNode.Children)
					{
						length += await FindOutputChunksAsync(chunkWriter, offset + length, childRef, logger, cancellationToken);
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

		static async Task BatchReadRequestsAsync(ChannelReader<OutputChunk> chunkReader, ChannelWriter<OutputBatch> batchWriter, CancellationToken cancellationToken)
		{
			List<OutputChunk> batch = new List<OutputChunk>();
			while (await chunkReader.WaitToReadAsync(cancellationToken))
			{
				OutputChunk? chunk;
				while (chunkReader.TryRead(out chunk))
				{
					if (batch.Count > 0 && chunk.File != batch[0].File)
					{
						await batchWriter.WriteAsync(new OutputBatch(batch), cancellationToken);
						batch = new List<OutputChunk>();
					}
					batch.Add(chunk);
				}
			}
			if (batch.Count > 0)
			{
				await batchWriter.WriteAsync(new OutputBatch(batch), cancellationToken);
			}
		}
	}
}

