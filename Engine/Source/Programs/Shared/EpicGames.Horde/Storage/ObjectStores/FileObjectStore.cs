// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.ObjectStores
{
	/// <summary>
	/// Storage backend that utilizes the local filesystem
	/// </summary>
	public sealed class FileObjectStore : IObjectStore, IDisposable
	{
		// Item which has been opened from the cache using a memory mapped file
		class MappedFile : IDisposable
		{
			public ObjectKey Locator { get; }
			public LinkedListNode<MappedFile> ListNode { get; }

			readonly FileInfo _fileInfo;

			MemoryMappedFile? _memoryMappedFile;
			MemoryMappedViewAccessor? _memoryMappedViewAccessor;
			MemoryMappedView? _memoryMappedView;

			int _refCount = 1;
			ReadOnlyMemory<byte> _data;
			bool _deleteOnDispose;

			public int RefCount => _refCount;

			public ulong MappedSize => _memoryMappedViewAccessor?.SafeMemoryMappedViewHandle.ByteLength ?? 0UL;

			public MappedFile(ObjectKey locator, FileInfo fileInfo)
			{
				Locator = locator;
				ListNode = new LinkedListNode<MappedFile>(this);

				_fileInfo = fileInfo;

				try
				{
					_memoryMappedFile = MemoryMappedFile.CreateFromFile(fileInfo.FullName, FileMode.Open, null, 0, MemoryMappedFileAccess.Read);
					_memoryMappedViewAccessor = _memoryMappedFile.CreateViewAccessor(0, 0, MemoryMappedFileAccess.Read);
					_memoryMappedView = new MemoryMappedView(_memoryMappedViewAccessor);

					_data = _memoryMappedView.GetMemory(0, (int)fileInfo.Length);
				}
				catch
				{
					Dispose();
					throw;
				}
			}

			public void Dispose()
			{
				_data = ReadOnlyMemory<byte>.Empty;

				if (_memoryMappedView != null)
				{
					_memoryMappedView.Dispose();
					_memoryMappedView = null;
				}
				if (_memoryMappedViewAccessor != null)
				{
					_memoryMappedViewAccessor.Dispose();
					_memoryMappedViewAccessor = null;
				}
				if (_memoryMappedFile != null)
				{
					_memoryMappedFile.Dispose();
					_memoryMappedFile = null;
				}

				if (_deleteOnDispose)
				{
					try
					{
						_fileInfo.Delete();
					}
					catch { }
				}
			}

			public ReadOnlyMemory<byte> GetData(int offset, int? length)
			{
				if (length == null)
				{
					return _data.Slice(offset);
				}
				else
				{
					return _data.Slice(offset, Math.Min(length.Value, _data.Length - offset));
				}
			}

			public void AddRef()
			{
				Interlocked.Increment(ref _refCount);
			}

			public void Release()
			{
				if (Interlocked.Decrement(ref _refCount) == 0)
				{
					Dispose();
				}
			}

			public void DeleteOnDispose()
			{
				_deleteOnDispose = true;
			}

			public override string ToString() => Locator.ToString();
		}

		// Handle to a file in memory
		class MappedFileHandle : IReadOnlyMemoryOwner<byte>
		{
			MappedFile? _mappedFile;
			ReadOnlyMemory<byte> _data;

			public ReadOnlyMemory<byte> Memory => _data;

			public MappedFileHandle(MappedFile? mappedFile, ReadOnlyMemory<byte> data)
			{
				_mappedFile = mappedFile;
				_mappedFile?.AddRef();
				_data = data;
			}

			public MappedFileHandle Clone()
			{
				_mappedFile?.AddRef();
				return new MappedFileHandle(_mappedFile, _data);
			}

			public void Dispose()
			{
				if (_mappedFile != null)
				{
					_mappedFile.Release();
					_mappedFile = null!;
				}

				_data = ReadOnlyMemory<byte>.Empty;
			}
		}

		/// <summary>
		/// Base directory for log files
		/// </summary>
		private readonly DirectoryReference _baseDir;

		/// <inheritdoc/>
		public bool SupportsRedirects => false;

		readonly object _lockObject = new object();
		readonly Dictionary<ObjectKey, MappedFile> _pathToMappedFile = new Dictionary<ObjectKey, MappedFile>();
		readonly LinkedList<MappedFile> _mappedFiles = new LinkedList<MappedFile>();

		long _mappedSize;

		const long MaxMappedSize = 1024L * 1024 * 1024;
		const int MaxMappedCount = 128;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="baseDir">Base directory for the store</param>
		public FileObjectStore(DirectoryReference baseDir)
		{
			_baseDir = baseDir;
			DirectoryReference.CreateDirectory(_baseDir);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			lock (_lockObject)
			{
				UnmapFiles(0, 0);
			}
		}

		/// <summary>
		/// Gets the path for storing a file on disk
		/// </summary>
		FileReference GetBlobFile(ObjectKey locator) => FileReference.Combine(_baseDir, locator.Path.ToString());

		/// <summary>
		/// Finds an existing mapped file or adds a new one for the given path
		/// </summary>
		MappedFile FindOrAddMappedFile(ObjectKey locator)
		{
			MappedFile? mappedFile;
			if (!_pathToMappedFile.TryGetValue(locator, out mappedFile))
			{
				FileInfo fileInfo = GetBlobFile(locator).ToFileInfo();
				if (fileInfo.Length == 0)
				{
					throw new Exception($"Unable to map empty memory mapped file: {fileInfo.FullName}");
				}

				long maxSize = MaxMappedSize - fileInfo.Length;
				if (_mappedSize > maxSize || _mappedFiles.Count + 1 > MaxMappedCount)
				{
					UnmapFiles(maxSize, MaxMappedCount - 1);
				}

				mappedFile = new MappedFile(locator, fileInfo);
				_pathToMappedFile.Add(locator, mappedFile);
				_mappedFiles.AddFirst(mappedFile.ListNode);

				_mappedSize += (long)mappedFile.MappedSize;
			}
			return mappedFile;
		}

		/// <summary>
		/// Discard mapped files until only a certain size is mapped in memory
		/// </summary>
		/// <param name="maxMappedSize">Maximum mapped size</param>
		/// <param name="maxMappedCount">Maximum number of mapped files</param>
		void UnmapFiles(long maxMappedSize, int maxMappedCount)
		{
			for (LinkedListNode<MappedFile>? listNode = _mappedFiles.Last; listNode != null && (_mappedSize > maxMappedSize || _mappedFiles.Count > maxMappedCount);)
			{
				LinkedListNode<MappedFile>? nextListNode = listNode.Previous;
				if (listNode.Value.RefCount == 1)
				{
					_pathToMappedFile.Remove(listNode.Value.Locator);
					_mappedFiles.Remove(listNode);
					listNode.Value.Release();
				}
				listNode = nextListNode;
			}
		}

		/// <inheritdoc/>
		public async Task<Stream> OpenAsync(ObjectKey locator, int offset, int? length, CancellationToken cancellationToken)
		{
#pragma warning disable CA2000 // Dispose objects before losing scope
			IReadOnlyMemoryOwner<byte> storageObject = await ReadAsync(locator, offset, length, cancellationToken);
			return storageObject.AsStream();
#pragma warning restore CA2000 // Dispose objects before losing scope
		}

		/// <summary>
		/// Maps a file into memory for reading, and returns a handle to it
		/// </summary>
		/// <param name="locator">Path to the file</param>
		/// <param name="offset">Offset of the data to retrieve</param>
		/// <param name="length">Length of the data</param>
		/// <returns>Handle to the data. Must be disposed by the caller.</returns>
		public IReadOnlyMemoryOwner<byte> Read(ObjectKey locator, int offset, int? length)
		{
			lock (_lockObject)
			{
				MappedFile mappedFile = FindOrAddMappedFile(locator);
				return new MappedFileHandle(mappedFile, mappedFile.GetData(offset, length));
			}
		}

		/// <inheritdoc/>
		public Task<IReadOnlyMemoryOwner<byte>> ReadAsync(ObjectKey locator, int offset, int? length, CancellationToken cancellationToken = default)
		{
			return Task.FromResult(Read(locator, offset, length));
		}

		/// <inheritdoc/>
		public async Task WriteAsync(ObjectKey locator, Stream stream, CancellationToken cancellationToken = default)
		{
			FileReference finalLocation = GetBlobFile(locator);
			DirectoryReference.CreateDirectory(finalLocation.Directory);
			FileReference tempLocation = new FileReference($"{finalLocation}.tmp");

			using (Stream outputStream = FileReference.Open(tempLocation, FileMode.Create, FileAccess.Write, FileShare.Read))
			{
				await stream.CopyToAsync(outputStream, cancellationToken);
			}

			// Move the temp file into place
			try
			{
				FileReference.Move(tempLocation, finalLocation, true);
			}
			catch (IOException) // Already exists
			{
				if (FileReference.Exists(finalLocation))
				{
					FileReference.Delete(tempLocation);
				}
				else
				{
					throw;
				}
			}
		}

		/// <inheritdoc/>
		public Task<bool> ExistsAsync(ObjectKey locator, CancellationToken cancellationToken)
		{
			FileReference location = GetBlobFile(locator);
			return Task.FromResult(FileReference.Exists(location));
		}

		/// <summary>
		/// Delete a file from the store
		/// </summary>
		/// <param name="locator"></param>
		public void Delete(ObjectKey locator)
		{
			lock(_lockObject)
			{
				MappedFile? mappedFile;
				if (_pathToMappedFile.TryGetValue(locator, out mappedFile))
				{
					mappedFile.DeleteOnDispose();

					_pathToMappedFile.Remove(locator);
					_mappedFiles.Remove(mappedFile.ListNode);

					mappedFile.Release();
				}
				else
				{
					FileReference location = GetBlobFile(locator);
					FileReference.Delete(location);
				}
			}
		}

		/// <inheritdoc/>
		public Task DeleteAsync(ObjectKey locator, CancellationToken cancellationToken)
		{
			Delete(locator);
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public async IAsyncEnumerable<ObjectKey> EnumerateAsync([EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			Stack<IEnumerator<DirectoryInfo>> queue = new Stack<IEnumerator<DirectoryInfo>>();
			try
			{
				queue.Push(new List<DirectoryInfo> { _baseDir.ToDirectoryInfo() }.GetEnumerator());
				while (queue.Count > 0)
				{
					IEnumerator<DirectoryInfo> top = queue.Peek();
					if (!top.MoveNext())
					{
						top.Dispose();
						queue.Pop();
						continue;
					}

					DirectoryInfo current = top.Current;
					foreach (FileInfo fileInfo in current.EnumerateFiles("*"))
					{
						string path = fileInfo.FullName.Substring(_baseDir.FullName.Length + 1).Replace(Path.DirectorySeparatorChar, '/');
						yield return new ObjectKey(path.Substring(0, path.Length - 5));
					}

					queue.Push(current.EnumerateDirectories().GetEnumerator());

					cancellationToken.ThrowIfCancellationRequested();
					await Task.Yield();
				}
			}
			finally
			{
				while (queue.TryPop(out IEnumerator<DirectoryInfo>? enumerator))
				{
					enumerator.Dispose();
				}
			}
		}

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetReadRedirectAsync(ObjectKey locator, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetWriteRedirectAsync(ObjectKey locator, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public void GetStats(StorageStats stats) { }
	}
}

