// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Backends
{
	/// <summary>
	/// Storage backend that utilizes the local filesystem
	/// </summary>
	public sealed class FileStorageBackend : IStorageBackend
	{
		/// <summary>
		/// Base directory for log files
		/// </summary>
		private readonly DirectoryReference _baseDir;

		/// <inheritdoc/>
		public bool SupportsRedirects => false;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="baseDir">Base directory for the store</param>
		public FileStorageBackend(DirectoryReference baseDir)
		{
			_baseDir = baseDir;
			DirectoryReference.CreateDirectory(_baseDir);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
		}

		/// <summary>
		/// Gets the path for storing a file on disk
		/// </summary>
		FileReference GetBlobFile(string path) => FileReference.Combine(_baseDir, $"{path}.blob");
	
		/// <inheritdoc/>
		public Task<Stream> OpenAsync(string path, int offset, int? length, CancellationToken cancellationToken)
		{
			FileReference location = GetBlobFile(path);
			Stream stream = FileReference.Open(location, FileMode.Open, FileAccess.Read, FileShare.Read);
			stream.Seek(offset, SeekOrigin.Begin);
			return Task.FromResult(stream);
		}

		/// <inheritdoc/>
		public async Task<string> WriteAsync(Stream stream, string? prefix = null, CancellationToken cancellationToken = default)
		{
			string path = StorageHelpers.CreateUniqueName(prefix);
			await WriteExplicitPathAsync(path, stream, cancellationToken);
			return path;
		}

		/// <inheritdoc/>
		public async Task WriteExplicitPathAsync(string path, Stream stream, CancellationToken cancellationToken = default)
		{
			FileReference finalLocation = GetBlobFile(path);
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
		public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken)
		{
			FileReference location = GetBlobFile(path);
			return Task.FromResult(FileReference.Exists(location));
		}

		/// <inheritdoc/>
		public Task DeleteAsync(string path, CancellationToken cancellationToken)
		{
			FileReference location = GetBlobFile(path);
			FileReference.Delete(location);
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public async IAsyncEnumerable<string> EnumerateAsync([EnumeratorCancellation] CancellationToken cancellationToken = default)
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
					foreach (FileInfo fileInfo in current.EnumerateFiles("*.blob"))
					{
						string path = fileInfo.FullName.Substring(_baseDir.FullName.Length + 1).Replace(Path.DirectorySeparatorChar, '/');
						yield return path.Substring(0, path.Length - 5);
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
		public ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public ValueTask<(string, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default) => default;
	}
}
