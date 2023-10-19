// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using Azure;
using Azure.Storage.Blobs;
using Azure.Storage.Blobs.Models;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using System.Threading;
using System.Runtime.CompilerServices;
using Azure.Storage.Sas;
using Microsoft.Extensions.Logging;
using OpenTelemetry.Trace;

namespace Horde.Server.Storage.Backends
{
	/// <summary>
	/// Exception wrapper for S3 requests
	/// </summary>
	public sealed class AzureException : Exception
	{
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="message">Message for the exception</param>
		/// <param name="innerException">Inner exception data</param>
		public AzureException(string? message, Exception? innerException)
			: base(message, innerException)
		{
		}
	}

	/// <summary>
	/// Options for Azure
	/// </summary>
	public interface IAzureStorageOptions
	{
		/// <summary>
		/// Connection string for Azure
		/// </summary>
		public string? AzureConnectionString { get; }

		/// <summary>
		/// Name of the container
		/// </summary>
		public string? AzureContainerName { get; }
	}

	/// <summary>
	/// Storage backend using Azure
	/// </summary>
	public sealed class AzureStorageBackend : IStorageBackend
	{
		private readonly ILogger _logger;
		private readonly Tracer _tracer;
		private readonly BlobContainerClient _blobContainer;

		/// <inheritdoc/>
		public bool SupportsRedirects => true;

		/// <summary>
		/// Constructor
		/// </summary>
		public AzureStorageBackend(IAzureStorageOptions options, ILogger<AzureStorageBackend> logger, Tracer tracer)
		{
			if (options.AzureConnectionString == null)
			{
				throw new AzureException($"Missing {nameof(IAzureStorageOptions.AzureConnectionString)} setting for Azure storage backend", null);
			}
			if (options.AzureContainerName == null)
			{
				throw new AzureException($"Missing {nameof(IAzureStorageOptions.AzureContainerName)} setting for Azure storage backend", null);
			}

			_logger = logger;
			_tracer = tracer;
			_blobContainer = new BlobContainerClient(options.AzureConnectionString, options.AzureContainerName);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
		}

		/// <inheritdoc/>
		public async Task<Stream> OpenAsync(string path, int offset, int? length, CancellationToken cancellationToken = default)
		{
			if (!await _blobContainer.ExistsAsync(cancellationToken))
			{
				throw new InvalidOperationException($"Container {_blobContainer.Name} did not exist");
			}

			try
			{
				BlobClient blob = _blobContainer.GetBlobClient(path);
				Response<BlobDownloadInfo> blobInfo = await blob.DownloadAsync(cancellationToken);
				return blobInfo.Value.Content;
			}
			catch (RequestFailedException ex)
			{
				throw new AzureException($"Unable to read {path} from Azure", ex);
			}
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyMemoryOwner<byte>> ReadAsync(string path, int offset, int? length, CancellationToken cancellationToken = default)
		{
			using Stream stream = await OpenAsync(path, offset, length, cancellationToken);
			return ReadOnlyMemoryOwner.Create(await stream.ReadAllBytesAsync(cancellationToken));
		}

		/// <inheritdoc/>
		public async Task<string> WriteAsync(Stream inputStream, string? prefix = null, CancellationToken cancellationToken = default)
		{
			string path = StorageHelpers.CreateUniqueName(prefix);
			await WriteExplicitPathAsync(path, inputStream, cancellationToken);
			return path;
		}

		/// <inheritdoc/>
		public async Task WriteExplicitPathAsync(string path, Stream stream, CancellationToken cancellationToken = default)
		{
			await _blobContainer.CreateIfNotExistsAsync(cancellationToken: cancellationToken);

			_logger.LogDebug("Fetching blob reference with name {ObjectName}", path);
			try
			{
				await _blobContainer.GetBlobClient(path).UploadAsync(stream, cancellationToken);
			}
			catch (Exception ex)
			{
				throw new AzureException("Unable to upload blob to Azure", ex);
			}
		}

		/// <inheritdoc/>
		public async Task<bool> ExistsAsync(string path, CancellationToken cancellationToken)
		{
			if (!await _blobContainer.ExistsAsync(cancellationToken))
			{
				return false;
			}

			BlobClient blob = _blobContainer.GetBlobClient(path);
			return await blob.ExistsAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task DeleteAsync(string path, CancellationToken cancellationToken)
		{
			if (!await _blobContainer.ExistsAsync(cancellationToken))
			{
				throw new InvalidOperationException($"Container {_blobContainer.Name} does not exist");
			}

			await _blobContainer.DeleteBlobAsync(path, cancellationToken: cancellationToken);
		}

		/// <inheritdoc/>
		public async IAsyncEnumerable<string> EnumerateAsync([EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			bool exists = await _blobContainer.ExistsAsync(cancellationToken);
			if (exists)
			{
				await foreach (BlobItem? item in _blobContainer.GetBlobsAsync(BlobTraits.Metadata, cancellationToken: cancellationToken))
				{
					yield return item.Name;
				}
			}
		}

		/// <inheritdoc/>
		public async ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default)
		{
			if (!await _blobContainer.ExistsAsync(cancellationToken))
			{
				throw new InvalidOperationException($"Container {_blobContainer.Name} did not exist");
			}

			return TryGetPresignedUrl(path, BlobSasPermissions.Read);
		}

		/// <inheritdoc/>
		public async ValueTask<(string, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default)
		{
			if (!await _blobContainer.ExistsAsync(cancellationToken))
			{
				throw new InvalidOperationException($"Container {_blobContainer.Name} did not exist");
			}

			string path = StorageHelpers.CreateUniqueName(prefix);

			Uri? uri = TryGetPresignedUrl(path, BlobSasPermissions.Write);
			if (uri != null)
			{
				return (path, uri);
			}
			else
			{
				return null;
			}
		}

		/// <summary>
		/// Helper method to generate a presigned URL for a request
		/// </summary>
		Uri? TryGetPresignedUrl(string path, BlobSasPermissions permissions)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan("azure.BuildPresignedUrl").SetAttribute("Path", path);

			try
			{
				BlobClient blob = _blobContainer.GetBlobClient(path);
				return blob.GenerateSasUri(permissions, DateTimeOffset.Now.AddHours(1.0));
			}
			catch (RequestFailedException e)
			{
				if (e.Status == 404)
				{
					return null;
				}

				throw;
			}
		}

		/// <inheritdoc/>
		public void GetStats(StorageStats stats)
		{
		}
	}
}
