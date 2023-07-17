// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Net.Mime;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.AspNet;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.DependencyInjection;
using OpenTelemetry.Trace;
using OpenTracing.Util;

#pragma warning disable CS1591

namespace Horde.Server.Ddc
{
	/// <summary>
	/// Interface for the DDC blobs service
	/// </summary>
	public interface IDdcBlobService
	{
		Task VerifyContentMatchesHashAsync(Stream stream, IoHash expectedHash, CancellationToken cancellationToken = default);
		Task<BlobId> PutObjectAsync(NamespaceId ns, BufferedPayload payload, BlobId identifier, CancellationToken cancellationToken = default);
		Task<BlobId> PutObjectAsync(NamespaceId ns, ReadOnlyMemory<byte> payload, BlobId identifier, CancellationToken cancellationToken = default);
		Task<BlobId> PutObjectKnownHashAsync(NamespaceId ns, BufferedPayload content, BlobId identifier, CancellationToken cancellationToken = default);
		Task<Uri?> MaybePutObjectWithRedirectAsync(NamespaceId ns, BlobId identifier, CancellationToken cancellationToken = default);

		Task<BlobContents> GetObjectAsync(NamespaceId ns, BlobId blob, List<string>? storageLayers = null, bool supportsRedirectUri = false, CancellationToken cancellationToken = default);

		Task<Uri?> GetObjectWithRedirectAsync(NamespaceId ns, BlobId blobIdentifier, List<string>? storageLayers = null, CancellationToken cancellationToken = default);

		Task<bool> ExistsAsync(NamespaceId ns, BlobId blob, List<string>? storageLayers = null, CancellationToken cancellationToken = default);

		Task<BlobId[]> FilterOutKnownBlobsAsync(NamespaceId ns, IEnumerable<BlobId> blobs, CancellationToken cancellationToken = default);
		Task<BlobContents> GetObjectsAsync(NamespaceId ns, BlobId[] refRequestBlobReferences, CancellationToken cancellationToken = default);
	}

	public class BlobNotFoundException : Exception
	{
		public NamespaceId Ns { get; }
		public BlobId Blob { get; }

		public BlobNotFoundException(NamespaceId ns, BlobId blob) : base($"No Blob in Namespace {ns} with id {blob}")
		{
			Ns = ns;
			Blob = blob;
		}

		public BlobNotFoundException(NamespaceId ns, BlobId blob, string message) : base(message)
		{
			Ns = ns;
			Blob = blob;
		}
	}

	public class BlobReplicationException : BlobNotFoundException
	{
		public BlobReplicationException(NamespaceId ns, BlobId blob, string message) : base(ns, blob, message)
		{
		}
	}

	public class BlobTooLargeException : Exception
	{
		public BlobId Blob { get; }

		public BlobTooLargeException(BlobId blob) : base($"Blob {blob} was to large to cache")
		{
			Blob = blob;
		}
	}

	public class ResourceHasToManyRequestsException : Exception
	{
		public ResourceHasToManyRequestsException(Exception originalException) : base($"To many requests to resource", originalException)
		{
		}
	}

	public class NamespaceNotFoundException : Exception
	{
		public NamespaceId Namespace { get; }

		public NamespaceNotFoundException(NamespaceId @namespace) : base($"Could not find namespace {@namespace}")
		{
			Namespace = @namespace;
		}

		public NamespaceNotFoundException(NamespaceId @namespace, string message) : base(message)
		{
			Namespace = @namespace;
		}
	}

	public static class BlobServiceExtensions
	{
		public static async Task<ContentId> PutCompressedObject(this IDdcBlobService blobService, NamespaceId ns, BufferedPayload payload, ContentId? id, IServiceProvider provider)
		{
			IDdcContentIdService contentIdStore = provider.GetService<IDdcContentIdService>()!;
			CompressedBufferUtils compressedBufferUtils = provider.GetService<CompressedBufferUtils>()!;
			Tracer tracer = provider.GetService<Tracer>()!;

			// decompress the content and generate a identifier from it to verify the identifier we got
			await using Stream decompressStream = payload.GetStream();
			// TODO: we should add a overload for decompress content that can work on streams, otherwise we are still limited to 2GB compressed blobs
			byte[] decompressedContent = compressedBufferUtils.DecompressContent(await decompressStream.ToByteArray());

			IoHash decompressedHash;
			using (TelemetrySpan _ = tracer.StartActiveSpan("web.hash").SetAttribute("operation.name", "web.hash"))
			{
				decompressedHash = IoHash.Compute(decompressedContent);
			}
			if (id != null && id.Value.Hash != decompressedHash)
			{
				throw new HashMismatchException(decompressedHash, id.Value.Hash);
			}

			ContentId identifierDecompressedPayload = new ContentId(decompressedHash);

			BlobId identifierCompressedPayload;
			{
				using TelemetrySpan _ = tracer.StartActiveSpan("web.hash").SetAttribute("operation.name", "web.hash");
				await using Stream hashStream = payload.GetStream();
				identifierCompressedPayload = new BlobId(await IoHash.ComputeAsync(hashStream));
			}

			// commit the mapping from the decompressed hash to the compressed hash, we run this in parallel with the blob store submit
			// TODO: let users specify weight of the blob compared to previously submitted content ids
			int contentIdWeight = (int)payload.Length;
			Task contentIdStoreTask = contentIdStore.PutAsync(ns, identifierDecompressedPayload, identifierCompressedPayload, contentIdWeight);

			// we still commit the compressed buffer to the object store using the hash of the compressed content
			{
				await blobService.PutObjectKnownHashAsync(ns, payload, identifierCompressedPayload);
			}

			await contentIdStoreTask;

			return identifierDecompressedPayload;
		}

		public static async Task<(BlobContents, string)> GetCompressedObject(this IDdcBlobService blobService, NamespaceId ns, ContentId contentId, IServiceProvider provider, bool supportsRedirectUri = false)
		{
			IDdcContentIdService contentIdStore = provider.GetService<IDdcContentIdService>()!;
			Tracer tracer = provider.GetService<Tracer>()!;

			BlobId[]? chunks = await contentIdStore.ResolveAsync(ns, contentId, mustBeContentId: false);
			if (chunks == null || chunks.Length == 0)
			{
				throw new ContentIdResolveException(contentId);
			}

			// single chunk, we just return that chunk
			if (chunks.Length == 1)
			{
				BlobId blobToReturn = chunks[0];
				string mimeType = CustomMediaTypeNames.UnrealCompressedBuffer;
				if (contentId.Equals(blobToReturn))
				{
					// this was actually the unmapped blob, meaning its not a compressed buffer
					mimeType = MediaTypeNames.Application.Octet;
				}

				return (await blobService.GetObjectAsync(ns, blobToReturn, supportsRedirectUri: supportsRedirectUri), mimeType);
			}

			// chunked content, combine the chunks into a single stream
			using TelemetrySpan _ = tracer.StartActiveSpan("blob.combine").SetAttribute("operation.name", "blob.combine");
			Task<BlobContents>[] tasks = new Task<BlobContents>[chunks.Length];
			for (int i = 0; i < chunks.Length; i++)
			{
				// even if it was requested to support redirect, since we need to combine the chunks using redirects is not possible
				tasks[i] = blobService.GetObjectAsync(ns, chunks[i], supportsRedirectUri: false);
			}

			MemoryStream ms = new MemoryStream();
			foreach (Task<BlobContents> task in tasks)
			{
				BlobContents blob = await task;
				await using Stream s = blob.Stream;
				await s.CopyToAsync(ms);
			}

			ms.Seek(0, SeekOrigin.Begin);

			// chunking could not have happened for a non compressed buffer so assume it is compressed
			return (new BlobContents(ms, ms.Length), CustomMediaTypeNames.UnrealCompressedBuffer);
		}
	}
}
