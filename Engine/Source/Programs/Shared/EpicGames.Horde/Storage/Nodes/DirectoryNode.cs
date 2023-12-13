// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Microsoft.Extensions.Logging;
using System;
using System.Buffers;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.IO.Compression;
using System.IO.Pipelines;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

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
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public CopyStatsLogger(ILogger logger) 
			=> _logger = logger;

		/// <inheritdoc/>
		public void Report(ICopyStats stats)
			=> _logger.LogInformation("Copied {NumFiles:n0} files ({Size:n1}mb, {Rate:n1}mb/s)", stats.Count, stats.Size / (1024.0 * 1024.0), stats.Rate / (1024.0 * 1024.0));
	}

	/// <summary>
	/// Progress logger for writing copy stats
	/// </summary>
	public class CopyStatsLoggerWithTotals : IProgress<ICopyStats>
	{
		readonly int _totalCount;
		readonly long _totalSize;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public CopyStatsLoggerWithTotals(int totalCount, long totalSize, ILogger logger)
		{
			_totalCount = totalCount;
			_totalSize = totalSize;
			_logger = logger;
		}

		/// <inheritdoc/>
		public void Report(ICopyStats stats)
			=> _logger.LogInformation("Copied {NumFiles:n0}/{TotalFiles:n0} files ({Size:n1}/{TotalSize:n1}mb, {Rate:n1}mb/s, {Pct}%)", stats.Count, _totalCount, stats.Size / (1024.0 * 1024.0), _totalSize / (1024.0 * 1024.0), stats.Rate / (1024.0 * 1024.0), (int)((Math.Max(stats.Size, 1) * 100) / Math.Max(_totalSize, 1)));
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
	[BlobType("{0714EC11-4D07-291A-8AE7-7F86799980D6}", 1)]
	public class DirectoryNode : Node
	{
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
		/// Deserialization constructor
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		public DirectoryNode(IBlobReader reader)
		{
			Flags = (DirectoryFlags)reader.ReadUnsignedVarInt();

			int fileCount = (int)reader.ReadUnsignedVarInt();
			for (int idx = 0; idx < fileCount; idx++)
			{
				FileEntry entry = new FileEntry(reader);
				_nameToFileEntry[entry.Name] = entry;
			}

			int directoryCount = (int)reader.ReadUnsignedVarInt();
			for (int idx = 0; idx < directoryCount; idx++)
			{
				DirectoryEntry entry = new DirectoryEntry(reader);
				_nameToDirectoryEntry[entry.Name] = entry;
			}
		}

		/// <inheritdoc/>
		public override void Serialize(IBlobWriter writer)
		{
			writer.WriteUnsignedVarInt((ulong)Flags);

			writer.WriteUnsignedVarInt(Files.Count);
			foreach (FileEntry fileEntry in _nameToFileEntry.Values)
			{
				writer.WriteHashedNodeRef(fileEntry);
			}

			writer.WriteUnsignedVarInt(Directories.Count);
			foreach (DirectoryEntry directoryEntry in _nameToDirectoryEntry.Values)
			{
				writer.WriteHashedNodeRef(directoryEntry);
			}
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

				directoryNode = await directoryEntry.ExpandAsync(cancellationToken);
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
				return await entry.ExpandAsync(cancellationToken);
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

		/// <inheritdoc cref="AddFilesAsync(DirectoryReference, IEnumerable{FileInfo}, IStorageWriter, ChunkingOptions?, IProgress{ICopyStats}?, CancellationToken)"/>
		public async Task AddFilesAsync(DirectoryInfo directoryInfo, IStorageWriter writer, ChunkingOptions? options = null, IProgress<ICopyStats>? progress = null, CancellationToken cancellationToken = default)
		{
			await AddFilesAsync(new DirectoryReference(directoryInfo), directoryInfo.EnumerateFiles("*", SearchOption.AllDirectories).ToList(), writer, options, progress, cancellationToken);
		}

		/// <inheritdoc cref="AddFilesAsync(DirectoryReference, IEnumerable{FileInfo}, IStorageWriter, ChunkingOptions?, IProgress{ICopyStats}?, CancellationToken)"/>
		public Task AddFilesAsync(DirectoryReference baseDir, IEnumerable<FileReference> files, IStorageWriter writer, ChunkingOptions? options = null, IProgress<ICopyStats>? progress = null, CancellationToken cancellationToken = default)
		{
			return AddFilesAsync(baseDir, files.Select(x => x.ToFileInfo()).ToList(), writer, options, progress, cancellationToken);
		}

		/// <inheritdoc cref="AddFilesAsync(DirectoryReference, IEnumerable{FileInfo}, IStorageWriter, ChunkingOptions?, IProgress{ICopyStats}?, CancellationToken)"/>
		public Task AddFilesAsync(DirectoryInfo baseDir, IEnumerable<FileInfo> files, IStorageWriter writer, ChunkingOptions? options = null, IProgress<ICopyStats>? progress = null, CancellationToken cancellationToken = default)
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
		public async Task AddFilesAsync(DirectoryReference baseDir, IEnumerable<FileInfo> files, IStorageWriter writer, ChunkingOptions? options = null, IProgress<ICopyStats>? progress = null, CancellationToken cancellationToken = default)
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

					// Create interior nodes for all the leaf chunks
					ChunkedData[] chunkedFiles = await CreateInteriorChunkNodesAsync(leafChunkedFiles, options.InteriorOptions, writer, cancellationToken);

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

						FileEntry entry = new FileEntry(file.Name, flags, file.Length, chunkedFiles[idx]);
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

		static async ValueTask CreateLeafChunkNodesAsync(IStorageWriter writer, IReadOnlyList<FileInfo> files, LeafChunkedData[] leafChunks, int start, int count, CopyStats? copyStats, ChunkingOptions options, CancellationToken cancellationToken)
		{
			await using IStorageWriter writerFork = writer.Fork();
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

		static async Task<ChunkedData[]> CreateInteriorChunkNodesAsync(LeafChunkedData[] leafChunkedFiles, InteriorChunkedDataNodeOptions options, IStorageWriter writer, CancellationToken cancellationToken)
		{
			ChunkedData[] chunkedFiles = new ChunkedData[leafChunkedFiles.Length];
			for (int idx = 0; idx < leafChunkedFiles.Length; idx++)
			{
				chunkedFiles[idx] = await InteriorChunkedDataNode.CreateTreeAsync(leafChunkedFiles[idx], options, writer, cancellationToken);
			}
			return chunkedFiles;
		}

		/// <summary>
		/// Updates this tree of directory objects
		/// </summary>
		/// <param name="updates">Files to add</param>
		/// <param name="writer">Writer for new node data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public Task UpdateAsync(IEnumerable<FileUpdate> updates, IStorageWriter writer, CancellationToken cancellationToken = default)
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
		public async Task UpdateAsync(DirectoryUpdate update, IStorageWriter writer, CancellationToken cancellationToken = default)
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
					HashedNodeRef<DirectoryNode> nodeRef = await writer.WriteHashedNodeAsync(childNode, cancellationToken);
					_nameToDirectoryEntry[name] = new DirectoryEntry(name, childNode.Length, nodeRef);
				}
			}
			foreach ((string name, FileEntry? file) in update.Files)
			{
				if (file == null)
				{
					_nameToFileEntry.Remove(name);
				}
				else
				{
					_nameToFileEntry[name] = file;
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
		public async Task CopyFromZipStreamAsync(Stream stream, IStorageWriter writer, ChunkingOptions options, CancellationToken cancellationToken = default)
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

		/// <summary>
		/// Utility function to allow extracting a packed directory to disk
		/// </summary>
		/// <param name="directoryInfo">Direcotry to write to</param>
		/// <param name="progress">Sink for progress updates</param>
		/// <param name="logger">Logger for output</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task CopyToDirectoryAsync(DirectoryInfo directoryInfo, IProgress<ICopyStats>? progress, ILogger logger, CancellationToken cancellationToken)
		{
			int numTasks = Math.Min(1 + (int)(Length / (16 * 1024 * 1024)), 128);
			logger.LogInformation("Splitting read into {NumThreads} threads", numTasks);

			CopyStats? copyStats = null;
			if (progress != null)
			{
				copyStats = new CopyStats(progress);
			}

			List<Task> tasks = new List<Task>();
			try
			{
				long offset = 0;
				for (int taskIdx = 0; taskIdx < numTasks; taskIdx++)
				{
					long minOffset = offset;
					long maxOffset = (Length * (taskIdx + 1)) / numTasks;

					// Entries may be zero-length, so need to make sure the last window will include everything
					if (taskIdx == numTasks - 1)
					{
						maxOffset++;
					}

					tasks.Add(Task.Run(() => CopyToDirectoryInternalAsync(directoryInfo, minOffset, maxOffset - minOffset, copyStats, logger, cancellationToken), cancellationToken));
					offset = maxOffset;
				}
			}
			finally
			{
				await Task.WhenAll(tasks);
				copyStats?.Flush();
			}
		}

		async Task CopyToDirectoryInternalAsync(DirectoryInfo directoryInfo, long windowOffset, long windowLength, CopyStats? copyStats, ILogger logger, CancellationToken cancellationToken)
		{
			directoryInfo.Create();

			foreach (FileEntry fileEntry in _nameToFileEntry.Values)
			{
				// Extract any file that starts within the window (window starts before this file, window ends after the start of the file)
				if (windowOffset <= 0 && windowOffset + windowLength > 0)
				{
					FileInfo fileInfo = new FileInfo(Path.Combine(directoryInfo.FullName, fileEntry.Name.ToString()));
					await fileEntry.CopyToFileAsync(fileInfo, cancellationToken);
					copyStats?.Update(1, fileEntry.Length);
				}
				windowOffset -= fileEntry.Length;
			}

			foreach (DirectoryEntry directoryEntry in _nameToDirectoryEntry.Values)
			{
				// Traverse into any directory that overlaps with the window (window starts before end of the directory, and window ends at or beyond the start of the directory)
				if (windowOffset < directoryEntry.Length && windowOffset + windowLength >= 0)
				{
					DirectoryInfo subDirectoryInfo = directoryInfo.CreateSubdirectory(directoryEntry.Name.ToString());
					DirectoryNode subDirectoryNode = await directoryEntry.ExpandAsync(cancellationToken);
					await subDirectoryNode.CopyToDirectoryInternalAsync(subDirectoryInfo, windowOffset, windowLength, copyStats, logger, cancellationToken);
				}
				windowOffset -= directoryEntry.Length;
			}
		}

		/// <summary>
		/// Returns a stream containing the zipped contents of this directory
		/// </summary>
		/// <param name="filter">Filter for files to include in the zip</param>
		/// <param name="logger">Logger for diagnostic output</param>
		/// <returns>Stream containing zipped archive data</returns>
		public Stream AsZipStream(FileFilter? filter = null, ILogger? logger = null) => new DirectoryNodeZipStream(this, filter, logger);
	}

	/// <summary>
	/// Describes an update to a file in a directory tree
	/// </summary>
	/// <param name="Path">Path to the file</param>
	/// <param name="Length">Length of the file data</param>
	/// <param name="Flags">Flags for the new file entry</param>
	/// <param name="Data">Chunked data for the file</param>
	public record class FileUpdate(string Path, FileEntryFlags Flags, long Length, ChunkedData Data);

	/// <summary>
	/// Describes an update to a directory node
	/// </summary>
	public class DirectoryUpdate
	{
		/// <summary>
		/// Directories to be updated
		/// </summary>
		public Dictionary<string, DirectoryUpdate?> Directories { get; } = new Dictionary<string, DirectoryUpdate?>();

		/// <summary>
		/// Files to be updated
		/// </summary>
		public Dictionary<string, FileEntry?> Files { get; } = new Dictionary<string, FileEntry?>();

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
		/// <param name="fileEntry">Content for the file</param>
		public void AddFile(string path, FileEntry? fileEntry)
		{
			string[] fragments = path.Split(new char[] { '/', '\\' });

			DirectoryUpdate lastDir = this;
			for (int idx = 0; idx + 1 < fragments.Length; idx++)
			{
				DirectoryUpdate? nextDir;
				if (!lastDir.Directories.TryGetValue(fragments[idx], out nextDir) || nextDir == null)
				{
					if (fileEntry == null)
					{
						return;
					}

					nextDir = new DirectoryUpdate();
					lastDir.Directories.Add(fragments[idx], nextDir);
				}
				lastDir = nextDir;
			}

			lastDir.Files[fragments[^1]] = fileEntry;
		}

		/// <summary>
		/// Adds a file to the tree
		/// </summary>
		/// <param name="path">Path to the file</param>
		/// <param name="flags">Flags for the new file entry</param>
		/// <param name="length">Length of the file</param>
		/// <param name="chunkedData">Chunked data instance</param>
		public FileEntry AddFile(string path, FileEntryFlags flags, long length, ChunkedData chunkedData)
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

			FileEntry entry = new FileEntry(name, flags, length, chunkedData);
			AddFile(path, entry);
			return entry;
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
					Files[fileName] = new FileEntry(fileName, fileUpdate.Flags, fileUpdate.Length, fileUpdate.Data);

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
	}

	/// <summary>
	/// Stream which zips a directory node tree dynamically
	/// </summary>
	class DirectoryNodeZipStream : Stream
	{
		/// <inheritdoc/>
		public override bool CanRead => true;

		/// <inheritdoc/>
		public override bool CanSeek => false;

		/// <inheritdoc/>
		public override bool CanWrite => false;

		/// <inheritdoc/>
		public override long Length => throw new NotImplementedException();

		/// <inheritdoc/>
		public override long Position { get => _position; set => throw new NotImplementedException(); }

		readonly Pipe _pipe;
		readonly ILogger? _logger;
		readonly BackgroundTask _backgroundTask;

		long _position;
		ReadOnlySequence<byte> _current = ReadOnlySequence<byte>.Empty;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="node">Root node to copy from</param>
		/// <param name="filter">Filter for files to include in the zip</param>
		/// <param name="logger">Optional logger for debug tracing</param>
		public DirectoryNodeZipStream(DirectoryNode node, FileFilter? filter, ILogger? logger)
		{
			_pipe = new Pipe();
			_backgroundTask = BackgroundTask.StartNew(ctx => CopyToPipeAsync(node, filter, _pipe.Writer, logger, ctx));
			_logger = logger;
		}

		/// <inheritdoc/>
		protected override void Dispose(bool disposing)
		{
			if (disposing)
			{
				_backgroundTask.DisposeAsync().AsTask().Wait();
			}

			base.Dispose(disposing);
		}

		/// <inheritdoc/>
		public override async ValueTask DisposeAsync()
		{
			await _backgroundTask.DisposeAsync();

			await base.DisposeAsync();
		}

		/// <inheritdoc/>
		public override async ValueTask<int> ReadAsync(Memory<byte> buffer, CancellationToken cancellationToken = default)
		{
			while (_current.Length == 0)
			{
				ReadResult result = await _pipe.Reader.ReadAsync(cancellationToken);
				_current = result.Buffer;

				if (result.IsCompleted && _current.Length == 0)
				{
					// Wait for the background thread to finish; it may error/have errored, and we want to re-throw on this thread before completing the read.
					await _backgroundTask.StopAsync(cancellationToken);
					_logger?.LogInformation("Zip file pipe was read to end");
					return 0;
				}
			}

			int initialSize = buffer.Length;
			while (buffer.Length > 0 && _current.Length > 0)
			{
				int copy = Math.Min(buffer.Length, _current.First.Length);
				_current.First.Slice(0, copy).CopyTo(buffer);
				_current = _current.Slice(copy);
				buffer = buffer.Slice(copy);
			}

			if (_current.Length == 0)
			{
				_pipe.Reader.AdvanceTo(_current.End);
			}

			int length = initialSize - buffer.Length;
			_position += length;
			return length;
		}

		static async Task CopyToPipeAsync(DirectoryNode node, FileFilter? filter, PipeWriter writer, ILogger? logger, CancellationToken cancellationToken)
		{
			using Stream outputStream = writer.AsStream();
			using ZipArchive archive = new ZipArchive(outputStream, ZipArchiveMode.Create);
			await CopyFilesAsync(node, "", filter, archive, logger, cancellationToken);
		}

		static async Task CopyFilesAsync(DirectoryNode directory, string prefix, FileFilter? filter, ZipArchive archive, ILogger? logger, CancellationToken cancellationToken)
		{
			int numDirs = directory.Directories.Count;
			int numFiles = directory.Files.Count;

			int numCopiedDirs = 0;
			foreach (DirectoryEntry directoryEntry in directory.Directories)
			{
				string directoryPath = $"{prefix}{directoryEntry.Name}/";
				if (filter == null || filter.PossiblyMatches(directoryPath))
				{
					DirectoryNode node = await directoryEntry.ExpandAsync(cancellationToken);
					await CopyFilesAsync(node, directoryPath, filter, archive, logger, cancellationToken);
				}
				numCopiedDirs++;
			}

			int numCopiedFiles = 0;
			foreach (FileEntry fileEntry in directory.Files)
			{
				string filePath = $"{prefix}{fileEntry}";
				if (filter == null || filter.Matches(filePath))
				{
					ZipArchiveEntry entry = archive.CreateEntry(filePath);

					if ((fileEntry.Flags & FileEntryFlags.Executable) != 0)
					{
						entry.ExternalAttributes |= 0b_111_111_101 << 16; // rwx rwx r-x
					}
					else
					{
						entry.ExternalAttributes |= 0b_110_110_100 << 16; // rw- rw- r--
					}

					using Stream entryStream = entry.Open();
					await fileEntry.CopyToStreamAsync(entryStream, cancellationToken);
				}
				numCopiedFiles++;
			}

			logger?.LogInformation("Zip file path {Prefix} has {NumCopiedDirectories}/{NumDirectories} directories, {NumCopiedFiles}/{NumFiles} files", prefix, numCopiedDirs, numDirs, numCopiedFiles, numFiles);
		}

		/// <inheritdoc/>
		public override void Flush()
		{
		}

		/// <inheritdoc/>
		public override int Read(byte[] buffer, int offset, int count) => ReadAsync(buffer.AsMemory(offset, count)).AsTask().Result;

		/// <inheritdoc/>
		public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();

		/// <inheritdoc/>
		public override void SetLength(long value) => throw new NotSupportedException();

		/// <inheritdoc/>
		public override void Write(byte[] buffer, int offset, int count) => throw new NotSupportedException();
	}

	/// <summary>
	/// Extension methods for writing directory nodes
	/// </summary>
	public static class DirectoryNodeExtensions
	{
		/// <summary>
		/// Writes a tree of files to a storage writer
		/// </summary>
		public static async Task<HashedNodeRef<DirectoryNode>> WriteFilesAsync(this IStorageWriter writer, DirectoryReference baseDir, ChunkingOptions? options = null, IProgress<ICopyStats>? progress = null, CancellationToken cancellationToken = default)
		{
			DirectoryNode outputNode = new DirectoryNode();
			await outputNode.AddFilesAsync(baseDir, DirectoryReference.EnumerateFiles(baseDir, "*", SearchOption.AllDirectories), writer, options, progress, cancellationToken);
			return await writer.WriteHashedNodeAsync(outputNode, cancellationToken);
		}

		/// <summary>
		/// Writes a tree of files to a storage writer
		/// </summary>
		public static async Task<HashedNodeRef<DirectoryNode>> WriteFilesAsync(this IStorageWriter writer, DirectoryInfo baseDir, IReadOnlyList<FileInfo> files, ChunkingOptions? options = null, IProgress<ICopyStats>? progress = null, CancellationToken cancellationToken = default)
		{
			DirectoryNode outputNode = new DirectoryNode();
			await outputNode.AddFilesAsync(baseDir, files, writer, options, progress, cancellationToken);
			return await writer.WriteHashedNodeAsync(outputNode, cancellationToken);
		}

		/// <summary>
		/// Writes a tree of files to a storage writer
		/// </summary>
		public static async Task<HashedNodeRef<DirectoryNode>> WriteFilesAsync(this IStorageWriter writer, DirectoryReference baseDir, IReadOnlyList<FileReference> files, ChunkingOptions? options = null, IProgress<ICopyStats>? progress = null, CancellationToken cancellationToken = default)
		{
			DirectoryNode outputNode = new DirectoryNode();
			await outputNode.AddFilesAsync(baseDir, files, writer, options, progress, cancellationToken);
			return await writer.WriteHashedNodeAsync(outputNode, cancellationToken);
		}
	}
}
