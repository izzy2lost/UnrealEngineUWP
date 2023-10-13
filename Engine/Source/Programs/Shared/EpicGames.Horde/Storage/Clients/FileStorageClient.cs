// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage.Backends;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Storage.Clients
{
	/// <summary>
	/// Implementation of <see cref="IStorageClient"/> which writes data to files on disk.
	/// </summary>
	public class FileStorageClient : BundleStorageClient
	{
		readonly DirectoryReference _rootDir;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="rootDir">Root directory for storing blobs</param>
		/// <param name="cache">Memory cache for read data</param>
		/// <param name="logger">Logger interface</param>
		public FileStorageClient(DirectoryReference rootDir, BundleReaderCache cache, ILogger logger)
			: base(new FileStorageBackend(rootDir), cache, logger)
		{
			_rootDir = rootDir;
			_logger = logger;

			DirectoryReference.CreateDirectory(_rootDir);
		}

		/// <summary>
		/// Reads a ref from a file on disk
		/// </summary>
		public async ValueTask<BlobHandle> ReadRefAsync(FileReference file)
		{
			string text = await FileReference.ReadAllTextAsync(file);
			return CreateBlobHandle(new BlobLocator(text));
		}

		FileReference GetRefFile(RefName name) => FileReference.Combine(_rootDir, name.ToString() + ".ref");

		#region Aliases

		/// <inheritdoc/>
		public override Task AddAliasAsync(string name, BlobHandle handle, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException("File storage client does not currently support aliases.");
		}

		/// <inheritdoc/>
		public override Task RemoveAliasAsync(string name, BlobHandle handle, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException("File storage client does not currently support aliases.");
		}

		/// <inheritdoc/>
		public override Task<BlobAlias[]> FindAliasesAsync(string alias, int? maxResults = null, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException("File storage client does not currently support aliases.");
		}

		#endregion

		#region Refs

		/// <inheritdoc/>
		public override Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default)
		{
			FileInfo file = GetRefFile(name).ToFileInfo();
			if (file.Exists)
			{
				file.Delete();
				return Task.FromResult(true);
			}
			return Task.FromResult(false);
		}

		/// <inheritdoc/>
		public override async Task<RefValue?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			FileReference file = GetRefFile(name);
			if (!FileReference.Exists(file))
			{
				return null;
			}

			_logger.LogInformation("Reading {File}", file);
			string[] lines = await FileReference.ReadAllLinesAsync(file, cancellationToken);

			BlobHandle handle = CreateBlobHandle(new BlobLocator(lines[0].Trim()));
			ReadOnlyMemory<byte> data = ReadOnlyMemory<byte>.Empty;
			if (lines.Length >= 2)
			{
				data = Convert.FromBase64String(lines[1].Trim());
			}

			return new RefValue(handle, data);
		}

		/// <inheritdoc/>
		public override async Task WriteRefAsync(RefName name, BlobHandle target, ReadOnlyMemory<byte> data, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			await target.FlushAsync(cancellationToken);
			BlobLocator locator = target.GetLocator();

			FileReference file = GetRefFile(name);
			DirectoryReference.CreateDirectory(file.Directory);
			_logger.LogInformation("Writing {File}", file);

			for (int attempt = 0; ; attempt++)
			{
				try
				{
					await FileReference.WriteAllTextAsync(file, locator.ToString());
					break;
				}
				catch (IOException ex) when (attempt < 3)
				{
					_logger.LogDebug(ex, "Unable to write to {File}; retrying...", file);
					await Task.Delay(100 * attempt, cancellationToken);
				}
			}
		}

		#endregion
	}
}
