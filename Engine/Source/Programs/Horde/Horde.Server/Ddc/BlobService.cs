// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using Horde.Server.Storage;
using OpenTelemetry.Trace;

namespace Horde.Server.Ddc
{
	class BlobService : IBlobService
	{
		BlobType s_rawBlobType = new BlobType(new Guid("{03E6C37B-33C1-491F-8541-D3C401B8B8EF}"), 1);

		readonly StorageService _storageService;
		readonly Tracer _tracer;

		public BlobService(StorageService storageService, Tracer tracer)
		{
			_storageService = storageService;
			_tracer = tracer;
		}

		public static Utf8String GetAlias(BlobId blobId) => $"ddc:{blobId}";

		public async Task VerifyContentMatchesHashAsync(Stream stream, IoHash expectedHash, CancellationToken cancellationToken)
		{
			IoHash hash;
			using (TelemetrySpan _ = _tracer.StartActiveSpan("web.hash").SetAttribute("operation.name", "web.hash"))
			{
				hash = await IoHash.ComputeAsync(stream, cancellationToken);
			}
			if (hash != expectedHash)
			{
				throw new HashMismatchException(hash, expectedHash);
			}
		}

		public async Task<BlobId> PutObjectAsync(NamespaceId ns, BufferedPayload payload, BlobId identifier, CancellationToken cancellationToken)
		{
			using TelemetrySpan scope = _tracer.StartActiveSpan("put_blob")
				.SetAttribute("operation.name", "put_blob")
				.SetAttribute("resource.name", identifier.ToString())
				.SetAttribute("Content-Length", payload.Length.ToString());

			await using Stream hashStream = payload.GetStream();
			await VerifyContentMatchesHashAsync(hashStream, identifier.Hash, cancellationToken);

			await PutObjectKnownHashAsync(ns, payload, identifier, cancellationToken);
			return identifier;
		}

		public Task<BlobId> PutObjectAsync(NamespaceId ns, ReadOnlyMemory<byte> payload, BlobId identifier, CancellationToken cancellationToken)
		{
			using MemoryBufferedPayload bufferedPayload = new MemoryBufferedPayload(payload);
			return PutObjectAsync(ns, bufferedPayload, identifier, cancellationToken);
		}

		public async Task<bool> ExistsAsync(NamespaceId ns, BlobId blob, List<string>? storageLayers, CancellationToken cancellationToken)
		{
			IStorageClient storageClient = await _storageService.GetClientAsync(ns, cancellationToken);
			return await storageClient.FindAliasAsync(GetAlias(blob), cancellationToken).AnyAsync(cancellationToken);
		}

		public async Task<BlobId[]> FilterOutKnownBlobsAsync(NamespaceId ns, IEnumerable<BlobId> blobIds, CancellationToken cancellationToken)
		{
			IStorageClient storageClient = await _storageService.GetClientAsync(ns, cancellationToken);

			List<BlobId> unknownBlobIds = new List<BlobId>();
			foreach (BlobId blobId in blobIds)
			{
				if (!await storageClient.FindAliasAsync(GetAlias(blobId), cancellationToken).AnyAsync())
				{
					unknownBlobIds.Add(blobId);
				}
			}

			return unknownBlobIds.ToArray();
		}

		public async Task<BlobContents> GetObjectAsync(NamespaceId ns, BlobId blob, List<string>? storageLayers, bool supportsRedirectUri, CancellationToken cancellationToken)
		{
			IStorageClient storageClient = await _storageService.GetClientAsync(ns, cancellationToken);

			BlobHandle? handle = await storageClient.FindAliasAsync(GetAlias(blob), cancellationToken).FirstOrDefaultAsync(cancellationToken);
			if (handle == null)
			{
				throw new BlobNotFoundException(ns, blob);
			}

			BlobData data = await handle.ReadAsync(cancellationToken);
			return new BlobContents(data.Data.ToArray());
		}

		public async Task<BlobContents> GetObjectsAsync(NamespaceId ns, BlobId[] blobs, CancellationToken cancellationToken)
		{
			using TelemetrySpan _ = _tracer.StartActiveSpan("blob.combine").SetAttribute("operation.name", "blob.combine");
			Task<BlobContents>[] tasks = new Task<BlobContents>[blobs.Length];
			for (int i = 0; i < blobs.Length; i++)
			{
				tasks[i] = GetObjectAsync(ns, blobs[i], storageLayers: null, supportsRedirectUri: false, cancellationToken);
			}

			MemoryStream ms = new MemoryStream();
			foreach (Task<BlobContents> task in tasks)
			{
				BlobContents blob = await task;
				await using Stream s = blob.Stream;
				await s.CopyToAsync(ms, cancellationToken);
			}

			ms.Seek(0, SeekOrigin.Begin);

			return new BlobContents(ms, ms.Length);
		}

		public Task<Uri?> GetObjectWithRedirectAsync(NamespaceId ns, BlobId blobIdentifier, List<string>? storageLayers, CancellationToken cancellationToken)
		{
			return Task.FromResult<Uri?>(null);
		}

		public Task<Uri?> MaybePutObjectWithRedirectAsync(NamespaceId ns, BlobId identifier, CancellationToken cancellationToken)
		{
			return Task.FromResult<Uri?>(null);
		}

		public async Task<BlobId> PutObjectKnownHashAsync(NamespaceId ns, BufferedPayload content, BlobId identifier, CancellationToken cancellationToken)
		{
			IStorageClient storageClient = await _storageService.GetClientAsync(ns, cancellationToken);

			BlobHandle blobHandle;
			await using (IStorageWriter writer = storageClient.CreateWriter())
			{
				Memory<byte> memory = writer.GetOutputBuffer(0, (int)content.Length);

				using Stream stream = content.GetStream();
				await stream.ReadAsync(memory, cancellationToken);

				blobHandle = await writer.WriteNodeAsync(memory.Length, Array.Empty<BlobHandle>(), s_rawBlobType, cancellationToken);
				await writer.FlushAsync(cancellationToken);
			}

			await storageClient.AddAliasAsync(GetAlias(identifier), blobHandle, cancellationToken);
			return identifier;
		}
	}
}
