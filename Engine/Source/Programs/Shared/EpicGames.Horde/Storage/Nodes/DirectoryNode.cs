// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.IO.Compression;
using System.IO.MemoryMappedFiles;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Serialization;
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
	/// Flags for a directory node
	/// </summary>
	public enum DirectoryFlags
	{
		/// <summary>
		/// No flags specified
		/// </summary>
		None = 0,
	}

	/// <summary>
	/// A directory node
	/// </summary>
	[BlobConverter(typeof(DirectoryNodeConverter))]
	public class DirectoryNode
	{
		/// <summary>
		/// Type of serialized directory node blobs
		/// </summary>
		public static Guid BlobTypeGuid { get; } = new Guid("{0714EC11-4D07-291A-8AE7-7F86799980D6}");

		readonly SortedDictionary<string, FileEntry> _nameToFileEntry = new SortedDictionary<string, FileEntry>(StringComparer.Ordinal);
		readonly SortedDictionary<string, DirectoryEntry> _nameToDirectoryEntry = new SortedDictionary<string, DirectoryEntry>(StringComparer.Ordinal);

		/// <summary>
		/// Total size of this directory
		/// </summary>
		public long Length => _nameToFileEntry.Values.Sum(x => x.Length) + _nameToDirectoryEntry.Values.Sum(x => x.Length);

		/// <summary>
		/// Flags for this directory 
		/// </summary>
		public DirectoryFlags Flags { get; }

		/// <summary>
		/// All the files within this directory
		/// </summary>
		public IReadOnlyCollection<FileEntry> Files => _nameToFileEntry.Values;

		/// <summary>
		/// Map of name to file entry
		/// </summary>
		public IReadOnlyDictionary<string, FileEntry> NameToFile => _nameToFileEntry;

		/// <summary>
		/// All the subdirectories within this directory
		/// </summary>
		public IReadOnlyCollection<DirectoryEntry> Directories => _nameToDirectoryEntry.Values;

		/// <summary>
		/// Map of name to file entry
		/// </summary>
		public IReadOnlyDictionary<string, DirectoryEntry> NameToDirectory => _nameToDirectoryEntry;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="flags"></param>
		public DirectoryNode(DirectoryFlags flags = DirectoryFlags.None)
		{
			Flags = flags;
		}

		/// <summary>
		/// Clear the contents of this directory
		/// </summary>
		public void Clear()
		{
			_nameToFileEntry.Clear();
			_nameToDirectoryEntry.Clear();
		}

		/// <summary>
		/// Check whether an entry with the given name exists in this directory
		/// </summary>
		/// <param name="name">Name of the entry to search for</param>
		/// <returns>True if the entry exists</returns>
		public bool Contains(string name) => TryGetFileEntry(name, out _) || TryGetDirectoryEntry(name, out _);

		#region File operations

		/// <summary>
		/// Adds a new file entry to this directory
		/// </summary>
		/// <param name="entry">The entry to add</param>
		public void AddFile(FileEntry entry)
		{
			_nameToFileEntry[entry.Name] = entry;
		}

		/// <summary>
		/// Adds a new file with the given name
		/// </summary>
		/// <param name="name">Name of the new directory</param>
		/// <param name="flags">Flags for the new file</param>
		/// <param name="length">Length of the file</param>
		/// <param name="data">Chunked data for the file</param>
		/// <returns>The new directory object</returns>
		public FileEntry AddFile(string name, FileEntryFlags flags, long length, ChunkedData data)
		{
			FileEntry entry = new FileEntry(name, flags, length, data);
			AddFile(entry);
			return entry;
		}

		/// <summary>
		/// Attempts to get a file entry with the given name
		/// </summary>
		/// <param name="name">Name of the file</param>
		/// <returns>Entry for the given name</returns>
		public FileEntry GetFileEntry(string name) => _nameToFileEntry[name];

		/// <summary>
		/// Attempts to get a file entry with the given name
		/// </summary>
		/// <param name="name">Name of the file</param>
		/// <param name="entry">Entry for the file</param>
		/// <returns>True if the file was found</returns>
		public bool TryGetFileEntry(string name, [NotNullWhen(true)] out FileEntry? entry) => _nameToFileEntry.TryGetValue(name, out entry);

		/// <summary>
		/// Opens a file for reading
		/// </summary>
		/// <param name="name">Name of the file to open</param>
		/// <returns>Stream for the file</returns>
		public Stream OpenFile(string name) => GetFileEntry(name).OpenAsStream();

		/// <summary>
		/// Attempts to open a file for reading
		/// </summary>
		/// <param name="name">Name of the file</param>
		/// <returns>File stream, or null if the file does not exist</returns>
		public Stream? TryOpenFile(string name)
		{
			FileEntry? entry;
			if (TryGetFileEntry(name, out entry))
			{
				return entry.OpenAsStream();
			}
			else
			{
				return null;
			}
		}

		/// <summary>
		/// Deletes the file entry with the given name
		/// </summary>
		/// <param name="name">Name of the entry to delete</param>
		/// <returns>True if the entry was found, false otherwise</returns>
		public bool DeleteFile(string name) => _nameToFileEntry.Remove(name);

		/// <summary>
		/// Attempts to get a file entry from a path
		/// </summary>
		/// <param name="path">Path to the directory</param>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns>The directory with the given path, or null if it was not found</returns>
		public async ValueTask<FileEntry?> GetFileEntryByPathAsync(string path, CancellationToken cancellationToken = default)
		{
			FileEntry? fileEntry;

			int slashIdx = path.LastIndexOf('/');
			if (slashIdx == -1)
			{
				if (!TryGetFileEntry(path, out fileEntry))
				{
					return null;
				}
			}
			else
			{
				DirectoryNode? directoryNode = await GetDirectoryByPathAsync(path.Substring(0, slashIdx), cancellationToken);
				if (directoryNode == null)
				{
					return null;
				}
				if (!directoryNode.TryGetFileEntry(path.Substring(slashIdx + 1), out fileEntry))
				{
					return null;
				}
			}

			return fileEntry;
		}

		/// <summary>
		/// Attempts to get a directory entry from a path
		/// </summary>
		/// <param name="path">Path to the directory</param>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns>The directory with the given path, or null if it was not found</returns>
		public ValueTask<DirectoryNode?> GetDirectoryByPathAsync(string path, CancellationToken cancellationToken = default) => GetDirectoryByPathAsync(this, path, cancellationToken);

		static async ValueTask<DirectoryNode?> GetDirectoryByPathAsync(DirectoryNode directoryNode, string path, CancellationToken cancellationToken = default)
		{
			while (path.Length > 0)
			{
				string directoryName;

				int slashIdx = path.IndexOf('/', StringComparison.Ordinal);
				if (slashIdx == -1)
				{
					directoryName = path;
					path = String.Empty;
				}
				else
				{
					directoryName = path.Substring(0, slashIdx);
					path = path.Substring(slashIdx + 1);
				}

				DirectoryEntry? directoryEntry;
				if (!directoryNode.TryGetDirectoryEntry(directoryName, out directoryEntry))
				{
					return null;
				}

				directoryNode = await directoryEntry.Handle.ReadBlobAsync(cancellationToken);
			}
			return directoryNode;
		}

		/// <summary>
		/// Deletes a file with the given path
		/// </summary>
		/// <param name="path"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public async ValueTask<bool> DeleteFileByPathAsync(string path, CancellationToken cancellationToken = default)
		{
			string remainingPath = path;
			for (DirectoryNode? directory = this; directory != null;)
			{
				int length = remainingPath.IndexOf('/', StringComparison.Ordinal);
				if (length == -1)
				{
					return directory.DeleteFile(remainingPath);
				}
				if (length > 0)
				{
					directory = await directory.TryOpenDirectoryAsync(remainingPath.Substring(0, length), cancellationToken);
				}
				remainingPath = remainingPath.Substring(length + 1);
			}
			return false;
		}

		#endregion

		#region Directory operations

		/// <summary>
		/// Adds a new directory with the given name
		/// </summary>
		/// <param name="entry">Name of the new directory</param>
		public void AddDirectory(DirectoryEntry entry)
		{
			if (TryGetFileEntry(entry.Name, out _))
			{
				throw new ArgumentException($"A file with the name '{entry.Name}' already exists in this directory", nameof(entry));
			}

			_nameToDirectoryEntry.Add(entry.Name, entry);
		}

		/// <summary>
		/// Get a directory entry with the given name
		/// </summary>
		/// <param name="name">Name of the directory</param>
		/// <returns>The entry with the given name</returns>
		public DirectoryEntry GetDirectoryEntry(string name) => _nameToDirectoryEntry[name];

		/// <summary>
		/// Attempts to get a directory entry with the given name
		/// </summary>
		/// <param name="name">Name of the directory</param>
		/// <param name="entry">Entry for the directory</param>
		/// <returns>True if the directory was found</returns>
		public bool TryGetDirectoryEntry(string name, [NotNullWhen(true)] out DirectoryEntry? entry) => _nameToDirectoryEntry.TryGetValue(name, out entry);

		/// <summary>
		/// Tries to get a directory with the given name
		/// </summary>
		/// <param name="name">Name of the new directory</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The new directory object</returns>
		public async ValueTask<DirectoryNode> OpenDirectoryAsync(string name, CancellationToken cancellationToken = default)
		{
			DirectoryNode? directoryNode = await TryOpenDirectoryAsync(name, cancellationToken);
			if (directoryNode == null)
			{
				throw new DirectoryNotFoundException();
			}
			return directoryNode;
		}

		/// <summary>
		/// Tries to get a directory with the given name
		/// </summary>
		/// <param name="name">Name of the new directory</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The new directory object</returns>
		public async ValueTask<DirectoryNode?> TryOpenDirectoryAsync(string name, CancellationToken cancellationToken = default)
		{
			if (TryGetDirectoryEntry(name, out DirectoryEntry? entry))
			{
				return await entry.Handle.ReadBlobAsync(cancellationToken);
			}
			else
			{
				return null;
			}
		}

		/// <summary>
		/// Deletes the file entry with the given name
		/// </summary>
		/// <param name="name">Name of the entry to delete</param>
		/// <returns>True if the entry was found, false otherwise</returns>
		public bool DeleteDirectory(string name) => _nameToDirectoryEntry.Remove(name);

		#endregion

		/// <inheritdoc cref="AddFilesAsync(DirectoryReference, IEnumerable{FileInfo}, IBlobWriter, ChunkingOptions?, IProgress{ICopyStats}?, CancellationToken)"/>
		public async Task AddFilesAsync(DirectoryInfo directoryInfo, IBlobWriter writer, ChunkingOptions? options = null, IProgress<ICopyStats>? progress = null, CancellationToken cancellationToken = default)
		{
			await AddFilesAsync(new DirectoryReference(directoryInfo), directoryInfo.EnumerateFiles("*", SearchOption.AllDirectories).ToList(), writer, options, progress, cancellationToken);
		}

		/// <inheritdoc cref="AddFilesAsync(DirectoryReference, IEnumerable{FileInfo}, IBlobWriter, ChunkingOptions?, IProgress{ICopyStats}?, CancellationToken)"/>
		public Task AddFilesAsync(DirectoryReference baseDir, IEnumerable<FileReference> files, IBlobWriter writer, ChunkingOptions? options = null, IProgress<ICopyStats>? progress = null, CancellationToken cancellationToken = default)
		{
			return AddFilesAsync(baseDir, files.Select(x => x.ToFileInfo()).ToList(), writer, options, progress, cancellationToken);
		}

		/// <inheritdoc cref="AddFilesAsync(DirectoryReference, IEnumerable{FileInfo}, IBlobWriter, ChunkingOptions?, IProgress{ICopyStats}?, CancellationToken)"/>
		public Task AddFilesAsync(DirectoryInfo baseDir, IEnumerable<FileInfo> files, IBlobWriter writer, ChunkingOptions? options = null, IProgress<ICopyStats>? progress = null, CancellationToken cancellationToken = default)
		{
			return AddFilesAsync(new DirectoryReference(baseDir), files.ToList(), writer, options, progress, cancellationToken);
		}

		/// <summary>
		/// Adds files from a directory to the storage
		/// </summary>
		/// <param name="baseDir">Base directory to base paths relative to</param>
		/// <param name="files">Files to add</param>
		/// <param name="options">Options for chunking file content</param>
		/// <param name="writer">Writer for new node data</param>
		/// <param name="progress">Feedback interface for progress updates</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task AddFilesAsync(DirectoryReference baseDir, IEnumerable<FileInfo> files, IBlobWriter writer, ChunkingOptions? options = null, IProgress<ICopyStats>? progress = null, CancellationToken cancellationToken = default)
		{
			options ??= new ChunkingOptions();

			CopyStats? copyStats = null;
			if (progress != null)
			{
				copyStats = new CopyStats(progress);
			}

			DirectoryUpdate update = new DirectoryUpdate();

			// Process the input sequence in 2gb batches
			const long MaxBatchSize = 2 * 1024 * 1024 * 1024L;
			using (IEnumerator<FileInfo> fileEnumerator = files.GetEnumerator())
			{
				List<FileInfo> batch = new List<FileInfo>();
				for (bool moreData = fileEnumerator.MoveNext(); moreData; )
				{
					batch.Clear();

					// Take the next batch of files
					long batchSize = 0;
					while (batchSize < MaxBatchSize && moreData)
					{
						FileInfo file = fileEnumerator.Current;
						batch.Add(file);
						batchSize += file.Length;
						moreData = fileEnumerator.MoveNext();
					}

					// Partition them up into parallel writers
					List<(int Start, int Count)> partitions = ComputePartitions(batch, batchSize);
					LeafChunkedData[] leafChunkedFiles = new LeafChunkedData[batch.Count];
					await Parallel.ForEachAsync(partitions, cancellationToken, (filePartition, ctx) => CreateLeafChunkNodesAsync(writer, batch, leafChunkedFiles, filePartition.Start, filePartition.Count, copyStats, options, cancellationToken));

					// Write all the interior nodes and generate the directory update
					for (int idx = 0; idx < batch.Count; idx++)
					{
						FileInfo file = batch[idx];

						FileEntryFlags flags = FileEntryFlags.None;
						if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
						{
							int mode = FileUtils.GetFileMode_Linux(file.FullName);
							if ((mode & ((1 << 0) | (1 << 3) | (1 << 6))) != 0)
							{
								flags |= FileEntryFlags.Executable;
							}
						}
						else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
						{
							int mode = FileUtils.GetFileMode_Mac(file.FullName);
							if ((mode & ((1 << 0) | (1 << 3) | (1 << 6))) != 0)
							{
								flags |= FileEntryFlags.Executable;
							}
						}

						FileUpdate entry = new FileUpdate(file.Name, flags, file.Length, leafChunkedFiles[idx]);
						update.AddFile(new FileReference(file).MakeRelativeTo(baseDir), entry);
					}
				}
			}

			// Add all the new entries to the tree
			await UpdateAsync(update, writer, cancellationToken);
		}

		static List<(int Index, int Count)> ComputePartitions(IReadOnlyList<FileInfo> files, long totalSize)
		{
			// Maximum number of streams to write in parallel
			const int MaxPartitions = 16;

			// Minimum size of output payload for each writer
			const long MinSizePerPartition = 1024 * 1024;

			// Calculate the number of writers
			int numPartitions = 1 + (int)Math.Min(totalSize / MinSizePerPartition, MaxPartitions);

			// Create the partitions
			List<(int Index, int Count)> partitions = new List<(int, int)>();

			long writtenSize = 0;
			long remainingSizeForPartition = 0;

			int startIndex = 0;
			for (int index = 0; index < files.Count; index++)
			{
				FileInfo file = files[index];
				if (file.Length >= remainingSizeForPartition && partitions.Count < numPartitions)
				{
					remainingSizeForPartition = (totalSize - writtenSize) / (numPartitions - partitions.Count);
					partitions.Add((startIndex, index - startIndex));
					startIndex = index;
				}

				writtenSize += file.Length;
				remainingSizeForPartition -= file.Length;
			}

			partitions.Add((startIndex, files.Count - startIndex));
			return partitions;
		}

		static async ValueTask CreateLeafChunkNodesAsync(IBlobWriter writer, IReadOnlyList<FileInfo> files, LeafChunkedData[] leafChunks, int start, int count, CopyStats? copyStats, ChunkingOptions options, CancellationToken cancellationToken)
		{
			await using IBlobWriter writerFork = writer.Fork();
			for (int idx = start; idx < start + count; idx++)
			{
				FileInfo file = files[idx];
				using (Stream stream = file.OpenRead())
				{
					leafChunks[idx] = await LeafChunkedDataNode.CreateFromStreamAsync(writerFork, stream, options.LeafOptions, copyStats, cancellationToken);
				}
			}
			await writerFork.FlushAsync(cancellationToken);
		}

		/// <summary>
		/// Updates this tree of directory objects
		/// </summary>
		/// <param name="updates">Files to add</param>
		/// <param name="writer">Writer for new node data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public Task UpdateAsync(IEnumerable<FileUpdate> updates, IBlobWriter writer, CancellationToken cancellationToken = default)
		{
			DirectoryUpdate update = new DirectoryUpdate();
			update.AddFiles(updates);
			return UpdateAsync(update, writer, cancellationToken);
		}

		/// <summary>
		/// Updates this tree of directory objects
		/// </summary>
		/// <param name="update">Files to add</param>
		/// <param name="writer">Writer for new node data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task UpdateAsync(DirectoryUpdate update, IBlobWriter writer, CancellationToken cancellationToken = default)
		{
			foreach ((string name, DirectoryUpdate? directory) in update.Directories)
			{
				if (directory == null)
				{
					_nameToDirectoryEntry.Remove(name);
				}
				else
				{
					DirectoryNode? childNode = await TryOpenDirectoryAsync(name, cancellationToken);
					childNode ??= new DirectoryNode();
					await childNode.UpdateAsync(directory, writer, cancellationToken);
					IBlobRef<DirectoryNode> handle = await writer.WriteBlobAsync<DirectoryNode>(childNode, cancellationToken);
					_nameToDirectoryEntry[name] = new DirectoryEntry(name, childNode.Length, handle);
				}
			}
			foreach ((string name, FileUpdate? file) in update.Files)
			{
				if (file == null)
				{
					_nameToFileEntry.Remove(name);
				}
				else
				{
					await file.WriteInteriorNodesAsync(writer, new ChunkingOptions().InteriorOptions, cancellationToken);
					_nameToFileEntry[name] = new FileEntry(name, file.Flags, file.Length, file.StreamHash, file.Nodes[0], file.CustomData);
				}
			}
		}

		/// <summary>
		/// Copies entries from a zip file
		/// </summary>
		/// <param name="stream">Input stream</param>
		/// <param name="writer">Writer for new nodes</param>
		/// <param name="options"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task CopyFromZipStreamAsync(Stream stream, IBlobWriter writer, ChunkingOptions options, CancellationToken cancellationToken = default)
		{
			// Create all the leaf nodes
			List<(ZipArchiveEntry, LeafChunkedData)> entries = new List<(ZipArchiveEntry, LeafChunkedData)>();
			using (ZipArchive archive = new ZipArchive(stream, ZipArchiveMode.Read, true))
			{
				foreach (ZipArchiveEntry entry in archive.Entries)
				{
					if (entry.Name.Length > 0)
					{
						using (Stream entryStream = entry.Open())
						{
							LeafChunkedData leafChunkedData = await LeafChunkedDataNode.CreateFromStreamAsync(writer, entryStream, options.LeafOptions, cancellationToken);
							entries.Add((entry, leafChunkedData));
						}
					}
				}
			}

			// Create all the interior nodes
			List<FileUpdate> updates = new List<FileUpdate>();
			foreach ((ZipArchiveEntry entry, LeafChunkedData leafChunkedFile) in entries)
			{
				FileEntryFlags flags = FileEntryFlags.None;
				if ((entry.ExternalAttributes & (0b_001_001_001 << 16)) != 0)
				{
					flags |= FileEntryFlags.Executable;
				}

				ChunkedData chunkedFile = await InteriorChunkedDataNode.CreateTreeAsync(leafChunkedFile, options.InteriorOptions, writer, cancellationToken);
				updates.Add(new FileUpdate(entry.FullName, flags, entry.Length, chunkedFile));
			}

			// Update the tree
			await UpdateAsync(updates, writer, cancellationToken);
		}

		/// <summary>
		/// Utility function to allow extracting a packed directory to disk
		/// </summary>
		/// <param name="directoryInfo"></param>
		/// <param name="logger"></param>
		/// <param name="cancellationToken"></param>
		public Task CopyToDirectoryAsync(DirectoryInfo directoryInfo, ILogger logger, CancellationToken cancellationToken) => CopyToDirectoryAsync(directoryInfo, null, logger, cancellationToken);

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
		/// <param name="directoryInfo">Direcotry to write to</param>
		/// <param name="progress">Sink for progress updates</param>
		/// <param name="logger">Logger for output</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task CopyToDirectoryAsync(DirectoryInfo directoryInfo, IProgress<ICopyStats>? progress, ILogger logger, CancellationToken cancellationToken)
		{
			int numTasks = Math.Min(1 + (int)(Length / (16 * 1024 * 1024)), 16);
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
				tasks.Add(RunBackgroundTask(ctx => FindOutputChunksRootAsync(chunks.Writer, logger, ctx)));
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
				FileEntry.ApplyPermissions(fileInfo, file.FileEntry.Flags);
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

		async Task FindOutputChunksRootAsync(ChannelWriter<OutputChunk> chunks, ILogger logger, CancellationToken cancellationToken)
		{
			await FindOutputChunksAsync("", this, chunks, logger, cancellationToken);
			chunks.Complete();
		}

		static async Task FindOutputChunksAsync(string path, DirectoryNode node, ChannelWriter<OutputChunk> chunks, ILogger logger, CancellationToken cancellationToken)
		{
			OutputDir outputDir = new OutputDir(path, node);

			foreach (FileEntry fileEntry in node._nameToFileEntry.Values)
			{
				OutputFile outputFile = new OutputFile(outputDir, fileEntry);
				await FindOutputChunksAsync(outputFile, 0, fileEntry.Target, chunks, logger, cancellationToken);
			}

			foreach (DirectoryEntry directoryEntry in node._nameToDirectoryEntry.Values)
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

	class DirectoryNodeConverter : BlobConverter<DirectoryNode>
	{
		/// <summary>
		/// Type of serialized directory node blobs
		/// </summary>
		public static BlobType BlobType { get; } = new BlobType(DirectoryNode.BlobTypeGuid, 2);

		/// <inheritdoc/>
		public override DirectoryNode Read(IBlobReader reader, BlobSerializerOptions options)
		{
			DirectoryNode directoryNode = new DirectoryNode((DirectoryFlags)reader.ReadUnsignedVarInt());

			int fileCount = (int)reader.ReadUnsignedVarInt();
			for (int idx = 0; idx < fileCount; idx++)
			{
				IBlobRef<ChunkedDataNode> targetHandle = reader.ReadBlobRef<ChunkedDataNode>();

				ChunkedDataNodeType targetType = ChunkedDataNodeType.Unknown;
				if (reader.Version >= 2)
				{
					targetType = (ChunkedDataNodeType)reader.ReadUnsignedVarInt();
				}

				string name = reader.ReadString();
				FileEntryFlags flags = (FileEntryFlags)reader.ReadUnsignedVarInt();
				long length = (long)reader.ReadUnsignedVarInt();
				IoHash streamHash = reader.ReadIoHash();
				ChunkedDataNodeRef target = new ChunkedDataNodeRef(targetType, length, targetHandle);

				ReadOnlyMemory<byte> customData = default;
				if ((flags & FileEntryFlags.HasCustomData) != 0)
				{
					customData = reader.ReadVariableLengthBytes();
					flags &= ~FileEntryFlags.HasCustomData;
				}

				directoryNode.AddFile(new FileEntry(name, flags, length, streamHash, target, customData));
			}

			int directoryCount = (int)reader.ReadUnsignedVarInt();
			for (int idx = 0; idx < directoryCount; idx++)
			{
				IBlobRef<DirectoryNode> directoryHandle = reader.ReadBlobRef<DirectoryNode>();
				long length = (long)reader.ReadUnsignedVarInt();
				string name = reader.ReadString();

				directoryNode.AddDirectory(new DirectoryEntry(name, length, directoryHandle));
			}

			return directoryNode;
		}

		/// <inheritdoc/>
		public override BlobType Write(IBlobWriter writer, DirectoryNode value, BlobSerializerOptions options)
		{
			writer.WriteUnsignedVarInt((ulong)value.Flags);

			writer.WriteUnsignedVarInt(value.Files.Count);
			foreach (FileEntry fileEntry in value.Files)
			{
				writer.WriteBlobRef(fileEntry.Target.Handle);
				writer.WriteUnsignedVarInt((int)fileEntry.Target.Type);

				FileEntryFlags flags = (fileEntry.CustomData.Length > 0) ? (fileEntry.Flags | FileEntryFlags.HasCustomData) : (fileEntry.Flags & ~FileEntryFlags.HasCustomData);

				writer.WriteString(fileEntry.Name);
				writer.WriteUnsignedVarInt((ulong)flags);
				writer.WriteUnsignedVarInt((ulong)fileEntry.Length);
				writer.WriteIoHash(fileEntry.StreamHash);

				if ((flags & FileEntryFlags.HasCustomData) != 0)
				{
					writer.WriteVariableLengthBytes(fileEntry.CustomData.Span);
				}
			}

			writer.WriteUnsignedVarInt(value.Directories.Count);
			foreach (DirectoryEntry directoryEntry in value.Directories)
			{
				writer.WriteBlobRef(directoryEntry.Handle);
				writer.WriteUnsignedVarInt((ulong)directoryEntry.Length);
				writer.WriteString(directoryEntry.Name);
			}

			return BlobType;
		}
	}

	/// <summary>
	/// Describes an update to a file in a directory tree
	/// </summary>
	/// <param name="Path">Path to the file</param>
	/// <param name="Length">Length of the file data</param>
	/// <param name="Flags">Flags for the new file entry</param>
	/// <param name="StreamHash">Hash of the entire stream</param>
	/// <param name="Nodes">Chunked data for the file</param>
	/// <param name="CustomData"></param>
	public record class FileUpdate(string Path, FileEntryFlags Flags, long Length, IoHash StreamHash, List<ChunkedDataNodeRef> Nodes, ReadOnlyMemory<byte> CustomData = default)
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public FileUpdate(string path, FileEntryFlags flags, long length, ChunkedData chunkedData, ReadOnlyMemory<byte> customData = default)
			: this(path, flags, length, chunkedData.StreamHash, new List<ChunkedDataNodeRef> { chunkedData.Root }, customData)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public FileUpdate(string path, FileEntryFlags flags, long length, LeafChunkedData chunkedData, ReadOnlyMemory<byte> customData = default)
			: this(path, flags, length, chunkedData.Hash, chunkedData.LeafHandles, customData)
		{
		}

		/// <summary>
		/// Writes interior node data to the given writer
		/// </summary>
		public async ValueTask WriteInteriorNodesAsync(IBlobWriter writer, InteriorChunkedDataNodeOptions interiorNodeOptions, CancellationToken cancellationToken)
		{
			if (Nodes.Count > 1)
			{
				ChunkedDataNodeRef rootRef = await InteriorChunkedDataNode.CreateTreeAsync(Nodes, interiorNodeOptions, writer, cancellationToken);
				Nodes.Clear();
				Nodes.Add(rootRef);
			}
		}
	}

	/// <summary>
	/// Describes an update to a directory node
	/// </summary>
	public class DirectoryUpdate
	{
		/// <summary>
		/// Directories to be updated
		/// </summary>
		public SortedDictionary<string, DirectoryUpdate?> Directories { get; } = new SortedDictionary<string, DirectoryUpdate?>(StringComparer.Ordinal);

		/// <summary>
		/// Files to be updated
		/// </summary>
		public SortedDictionary<string, FileUpdate?> Files { get; } = new SortedDictionary<string, FileUpdate?>(StringComparer.Ordinal);

		/// <summary>
		/// Reset this instance
		/// </summary>
		public void Clear()
		{
			Directories.Clear();
			Files.Clear();
		}

		/// <summary>
		/// Adds a file by path to this object
		/// </summary>
		/// <param name="path">Path to add to</param>
		/// <param name="fileUpdate">Content for the file</param>
		public void AddFile(string path, FileUpdate? fileUpdate)
		{
			string[] fragments = path.Split(new char[] { '/', '\\' });

			DirectoryUpdate lastDir = this;
			for (int idx = 0; idx + 1 < fragments.Length; idx++)
			{
				DirectoryUpdate? nextDir;
				if (!lastDir.Directories.TryGetValue(fragments[idx], out nextDir) || nextDir == null)
				{
					if (fileUpdate == null)
					{
						return;
					}

					nextDir = new DirectoryUpdate();
					lastDir.Directories.Add(fragments[idx], nextDir);
				}
				lastDir = nextDir;
			}

			lastDir.Files[fragments[^1]] = fileUpdate;
		}

		/// <summary>
		/// Adds a file to the tree
		/// </summary>
		/// <param name="path">Path to the file</param>
		/// <param name="flags">Flags for the new file entry</param>
		/// <param name="length">Length of the file</param>
		/// <param name="chunkedData">Chunked data instance</param>
		/// <param name="customData"></param>
		public FileUpdate AddFile(string path, FileEntryFlags flags, long length, LeafChunkedData chunkedData, ReadOnlyMemory<byte> customData)
		{
			string name = path;
			for (int idx = path.Length - 1; idx >= 0; idx--)
			{
				if (path[idx] == Path.DirectorySeparatorChar || path[idx] == Path.AltDirectorySeparatorChar)
				{
					name = path.Substring(idx + 1);
					break;
				}
			}

			FileUpdate update = new FileUpdate(name, flags, length, chunkedData, customData);
			AddFile(path, update);
			return update;
		}

		/// <summary>
		/// Adds a filtered list of files from disk
		/// </summary>
		/// <param name="files">Files to add</param>
		public void AddFiles(IEnumerable<FileUpdate> files)
		{
			IEnumerator<FileUpdate> enumerator = files.GetEnumerator();
			if (enumerator.MoveNext())
			{
				AddFiles(String.Empty, enumerator);
			}
		}

		bool AddFiles(ReadOnlySpan<char> prefix, IEnumerator<FileUpdate> files)
		{
			for (; ; )
			{
				FileUpdate fileUpdate = files.Current;
				if (fileUpdate.Path.Length < prefix.Length || !fileUpdate.Path.AsSpan().StartsWith(prefix, FileReference.Comparison))
				{
					return true;
				}

				int nextDirLength = fileUpdate.Path.AsSpan(prefix.Length).IndexOfAny(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
				if (nextDirLength == -1)
				{
					string fileName = fileUpdate.Path.Substring(prefix.Length);
					Files[fileName] = fileUpdate;

					if (!files.MoveNext())
					{
						return false;
					}
				}
				else
				{
					string dirName = fileUpdate.Path.Substring(prefix.Length, nextDirLength);

					DirectoryUpdate? nextTree;
					if (!Directories.TryGetValue(dirName, out nextTree) || nextTree == null)
					{
						nextTree = new DirectoryUpdate();
						Directories[dirName] = nextTree;
					}

					ReadOnlySpan<char> nextPrefix = fileUpdate.Path.AsSpan(0, prefix.Length + nextDirLength + 1);
					if (!nextTree.AddFiles(nextPrefix, files))
					{
						return false;
					}
				}
			}
		}

		/// <summary>
		/// Writes interior node data to the given writer
		/// </summary>
		public async ValueTask WriteInteriorNodesAsync(IBlobWriter writer, InteriorChunkedDataNodeOptions interiorNodeOptions, CancellationToken cancellationToken)
		{
			foreach (DirectoryUpdate? directoryUpdate in Directories.Values)
			{
				if (directoryUpdate != null)
				{
					await directoryUpdate.WriteInteriorNodesAsync(writer, interiorNodeOptions, cancellationToken);
				}
			}
			foreach (FileUpdate? fileUpdate in Files.Values)
			{
				if (fileUpdate != null)
				{
					await fileUpdate.WriteInteriorNodesAsync(writer, interiorNodeOptions, cancellationToken);
				}
			}
		}
	}

	/// <summary>
	/// Extension methods for writing directory nodes
	/// </summary>
	public static class DirectoryNodeExtensions
	{
		/// <summary>
		/// Writes a tree of files to a storage writer
		/// </summary>
		public static async Task<IBlobRef<DirectoryNode>> WriteFilesAsync(this IBlobWriter writer, DirectoryReference baseDir, ChunkingOptions? options = null, IProgress<ICopyStats>? progress = null, CancellationToken cancellationToken = default)
		{
			DirectoryNode outputNode = new DirectoryNode();
			await outputNode.AddFilesAsync(baseDir, DirectoryReference.EnumerateFiles(baseDir, "*", SearchOption.AllDirectories), writer, options, progress, cancellationToken);
			return await writer.WriteBlobAsync(outputNode, cancellationToken: cancellationToken);
		}

		/// <summary>
		/// Writes a tree of files to a storage writer
		/// </summary>
		public static async Task<IBlobRef<DirectoryNode>> WriteFilesAsync(this IBlobWriter writer, DirectoryInfo baseDir, IReadOnlyList<FileInfo> files, ChunkingOptions? options = null, IProgress<ICopyStats>? progress = null, CancellationToken cancellationToken = default)
		{
			DirectoryNode outputNode = new DirectoryNode();
			await outputNode.AddFilesAsync(baseDir, files, writer, options, progress, cancellationToken);
			return await writer.WriteBlobAsync(outputNode, cancellationToken: cancellationToken);
		}

		/// <summary>
		/// Writes a tree of files to a storage writer
		/// </summary>
		public static async Task<IBlobRef<DirectoryNode>> WriteFilesAsync(this IBlobWriter writer, DirectoryReference baseDir, IReadOnlyList<FileReference> files, ChunkingOptions? options = null, IProgress<ICopyStats>? progress = null, CancellationToken cancellationToken = default)
		{
			DirectoryNode outputNode = new DirectoryNode();
			await outputNode.AddFilesAsync(baseDir, files, writer, options, progress, cancellationToken);
			return await writer.WriteBlobAsync(outputNode, cancellationToken: cancellationToken);
		}
	}
}
