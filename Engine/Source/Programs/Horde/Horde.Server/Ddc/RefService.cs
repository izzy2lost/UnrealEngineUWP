// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Horde.Server.Storage;

namespace Horde.Server.Ddc
{
	class RefService : IRefService
	{
		readonly StorageService _storageService;
		readonly IBlobService _blobService;

		public RefService(StorageService storageService, IBlobService blobService)
		{
			_storageService = storageService;
			_blobService = blobService;
		}

		static RefName GetRefName(BucketId bucketId, RefId refId) => $"{bucketId}/{refId}";

		public async Task<bool> DeleteAsync(NamespaceId ns, BucketId bucket, RefId key, CancellationToken cancellationToken)
		{
			IStorageClient storageClient = await _storageService.GetClientAsync(ns, cancellationToken);
			await storageClient.DeleteRefAsync(GetRefName(bucket, key), cancellationToken);
			return true;
		}

		public async Task<bool> ExistsAsync(NamespaceId ns, BucketId bucket, RefId key, CancellationToken cancellationToken)
		{
			IStorageClient storageClient = await _storageService.GetClientAsync(ns, cancellationToken);
			BlobHandle? handle = await storageClient.TryReadRefTargetAsync(GetRefName(bucket, key), cancellationToken: cancellationToken);
			return handle != null;
		}

		public async Task<(ContentId[], BlobId[])> FinalizeAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId blobHash, CancellationToken cancellationToken)
		{
			HashSet<BlobId> missingBlobIds = new HashSet<BlobId>();

			// Read all the blobs back in
			List<BlobId> blobIds = new List<BlobId>();
			blobIds.Add(blobHash);

			HashSet<BlobId> addedBlobIds = new HashSet<BlobId>();
			for (int idx = 0; idx < blobIds.Count; idx++)
			{
				try
				{
					using BlobContents contents = await _blobService.GetObjectAsync(ns, blobIds[idx], storageLayers: null, supportsRedirectUri: false, cancellationToken: cancellationToken);
					byte[] data = await contents.Stream.ReadAllBytesAsync(cancellationToken);

					CbObject obj = new CbObject(data);
					obj.IterateAttachments(x =>
					{
						BlobId blobId = new BlobId(x.AsAttachment());
						if (addedBlobIds.Add(blobId))
						{
							blobIds.Add(blobId);
						}
					});
				}
				catch (BlobNotFoundException)
				{
					missingBlobIds.Add(blobIds[idx]);
				}
			}

			if (missingBlobIds.Count == 0)
			{
				IStorageClient storageClient = await _storageService.GetClientAsync(ns, cancellationToken);

				BlobHandle? handle = await storageClient.FindAliasAsync(BlobService.GetAlias(blobHash), cancellationToken).FirstOrDefaultAsync(cancellationToken);
				if (handle == null)
				{
					throw new BlobNotFoundException(ns, blobHash);
				}

				await storageClient.WriteRefTargetAsync(GetRefName(bucket, key), handle, cancellationToken: cancellationToken);
			}

			return (Array.Empty<ContentId>(), missingBlobIds.ToArray());
		}

		public async Task<(RefRecord, BlobContents?)> GetAsync(NamespaceId ns, BucketId bucket, RefId key, string[] fields, bool doLastAccessTracking, CancellationToken cancellationToken)
		{
			IStorageClient storageClient = await _storageService.GetClientAsync(ns, cancellationToken);
			
			RefNode? node = await storageClient.TryReadNodeAsync<RefNode>(GetRefName(bucket, key), cancellationToken: cancellationToken);
			if (node == null)
			{
				throw new RefNotFoundException(ns, bucket, key);
			}

			RefRecord record = new RefRecord(ns, bucket, key, DateTime.UtcNow, null, new BlobId(node.RootHash), false);
			return (record, null);
		}

		public async Task<List<BlobId>> GetReferencedBlobsAsync(NamespaceId ns, BucketId bucket, RefId key, CancellationToken cancellationToken)
		{
			IStorageClient storageClient = await _storageService.GetClientAsync(ns, cancellationToken);

			RefNode? node = await storageClient.TryReadNodeAsync<RefNode>(GetRefName(bucket, key), cancellationToken: cancellationToken);
			if (node == null)
			{
				throw new RefNotFoundException(ns, bucket, key);
			}

			return node.References.Select(x => new BlobId(x.Hash)).ToList();
		}

		public async Task<(ContentId[], BlobId[])> PutAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId blobHash, CbObject payload, CancellationToken cancellationToken)
		{
			await _blobService.PutObjectAsync(ns, payload.GetView(), blobHash, cancellationToken);
			return await FinalizeAsync(ns, bucket, key, blobHash, cancellationToken);
		}
	}
}
