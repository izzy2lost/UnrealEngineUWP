// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.Serialization;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;
using EpicGames.Horde.Storage.Bundles;

namespace EpicGames.Horde.Storage.Backends
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
		/// <param name="logger">Logger interface</param>
		public FileStorageClient(DirectoryReference rootDir, ILogger logger)
			: base(null, logger)
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
			return new FlushedNodeHandle(TreeReader, BundleNodeLocator.Parse(text));
		}

		FileReference GetRefFile(RefName name) => FileReference.Combine(_rootDir, name.ToString() + ".ref");
		FileReference GetBlobFile(BundleLocator id) => FileReference.Combine(_rootDir, id.Path.ToString() + ".blob");

		#region Blobs

		/// <inheritdoc/>
		public override async Task<Bundle> ReadBundleAsync(BundleLocator id, CancellationToken cancellationToken = default)
		{
			FileReference file = GetBlobFile(id);
			_logger.LogInformation("Reading {File}", file);

			byte[] data = await FileReference.ReadAllBytesAsync(file, cancellationToken);
			return new Bundle(data);
		}

		/// <inheritdoc/>
		public override async Task<ReadOnlyMemory<byte>> ReadBundleRangeAsync(BundleLocator id, int offset, int length, CancellationToken cancellationToken = default)
		{
			Bundle bundle = await ReadBundleAsync(id, cancellationToken);

			ReadOnlySequence<byte> sequence = bundle.AsSequence().Slice(offset);
			if (sequence.Length > length)
			{
				sequence = sequence.Slice(0, length);
			}

			return sequence.AsSingleSegment();
		}

		/// <inheritdoc/>
		public override async Task<BundleLocator> WriteBundleAsync(Bundle bundle, Utf8String prefix = default, CancellationToken cancellationToken = default)
		{
			BundleLocator id = BundleLocator.CreateUnique(prefix);
			FileReference file = GetBlobFile(id);
			DirectoryReference.CreateDirectory(file.Directory);
			_logger.LogInformation("Writing {File}", file);

			using (FileStream fileStream = FileReference.Open(file, FileMode.Create, FileAccess.Write, FileShare.ReadWrite))
			{
				foreach (ReadOnlyMemory<byte> segment in bundle.AsSequence())
				{
					await fileStream.WriteAsync(segment, cancellationToken);
				}
			}

			return id;
		}

		#endregion

		#region Aliases

		/// <inheritdoc/>
		public override Task AddAliasAsync(Utf8String name, BundleNodeHandle locator, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException("File storage client does not currently support aliases.");
		}

		/// <inheritdoc/>
		public override Task RemoveAliasAsync(Utf8String name, BundleNodeHandle locator, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException("File storage client does not currently support aliases.");
		}

		/// <inheritdoc/>
		public override IAsyncEnumerable<BundleNodeHandle> FindNodesAsync(Utf8String alias, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException("File storage client does not currently support aliases.");
		}

		#endregion

		#region Refs

		/// <inheritdoc/>
		public override Task DeleteRefAsync(RefName name, CancellationToken cancellationToken = default)
		{
			FileReference file = GetRefFile(name);
			FileReference.Delete(file);
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public override async Task<BundleNodeHandle?> TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			FileReference file = GetRefFile(name);
			if (!FileReference.Exists(file))
			{
				return null;
			}

			_logger.LogInformation("Reading {File}", file);
			string text = await FileReference.ReadAllTextAsync(file, cancellationToken);
			return new FlushedNodeHandle(TreeReader, BundleNodeLocator.Parse(text));
		}

		/// <inheritdoc/>
		public override async Task WriteRefTargetAsync(RefName name, BundleNodeHandle target, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			FileReference file = GetRefFile(name);
			DirectoryReference.CreateDirectory(file.Directory);
			_logger.LogInformation("Writing {File}", file);

			BundleNodeLocator locator = await target.FlushAsync(cancellationToken);
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
