// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;

namespace Horde.Server.Ddc
{
	class RefService : IRefService
	{
		readonly IStorageClientFactory _storageClientFactory;
		readonly IReferenceResolver _referenceResolver;
		readonly IBlobService _blobService;

		public RefService(IStorageClientFactory storageService, IReferenceResolver referenceResolver, IBlobService blobService)
		{
			_storageClientFactory = storageService;
			_referenceResolver = referenceResolver;
			_blobService = blobService;
		}

		static RefName GetRefName(BucketId bucketId, RefId refId) => $"{bucketId}/{refId}";

		public async Task<bool> DeleteAsync(NamespaceId ns, BucketId bucket, RefId key)
		{
			using IStorageClient storageClient = _storageClientFactory.CreateClient(ns);
			return await storageClient.DeleteRefAsync(GetRefName(bucket, key));
		}

		public async Task<bool> ExistsAsync(NamespaceId ns, BucketId bucket, RefId key)
		{
			using IStorageClient storageClient = _storageClientFactory.CreateClient(ns);
			return await storageClient.RefExistsAsync(GetRefName(bucket, key));
		}

		public async Task<(ContentId[], BlobId[])> FinalizeAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId blobHash)
		{
			using IStorageClient storageClient = _storageClientFactory.CreateClient(ns);

			BlobAlias? blobAlias = await storageClient.FindAliasAsync(BlobService.GetAlias(blobHash));
			if (blobAlias == null)
			{
				throw new BlobNotFoundException(ns, blobHash);
			}

			IBlobHandle blobHandle = blobAlias.Target;
			using BlobData blobContents = await blobHandle.ReadBlobDataAsync();
			CbObject payload = new CbObject(blobContents.Data);

			BlobId[] referencedBlobs = Array.Empty<BlobId>();

			ContentId[] missingReferences = Array.Empty<ContentId>();
			BlobId[] missingBlobs = Array.Empty<BlobId>();
			bool hasReferences = HasAttachments(payload);
			if (hasReferences)
			{
				try
				{
					referencedBlobs = await _referenceResolver.GetReferencedBlobs(ns, payload).ToArrayAsync();
				}
				catch (PartialReferenceResolveException e)
				{
					missingReferences = e.UnresolvedReferences.ToArray();
				}
				catch (ReferenceIsMissingBlobsException e)
				{
					missingBlobs = e.MissingBlobs.ToArray();
				}
			}

			if (missingReferences.Length == 0 && missingBlobs.Length == 0)
			{
				// TODO: We resolved all these blobs above... Need to just have GetReferencedBlobs just return the appropriate handles directly.
				RefName refName = GetRefName(bucket, key);

				IBlobRef<DdcRefNode> refNodeRef;
				await using (IBlobWriter writer = storageClient.CreateBlobWriter(refName))
				{
					DdcRefNode refNode = new DdcRefNode(blobHash.AsIoHash());
					refNode.References.Add(BlobRef.Create(blobHash.AsIoHash(), blobHandle));
					foreach (BlobId referencedBlob in referencedBlobs)
					{
						BlobAlias? alias = await storageClient.FindAliasAsync(BlobService.GetAlias(referencedBlob));
						refNode.References.Add(BlobRef.Create(referencedBlob.AsIoHash(), alias!.Target));
					}
					refNodeRef = await writer.WriteBlobAsync(refNode);
				}

				await storageClient.WriteRefAsync(refName, refNodeRef);
			}

			return (missingReferences, missingBlobs);
		}

		private bool HasAttachments(CbObject payload)
		{
			bool FieldHasAttachments(CbField field)
			{
				if (field.IsObject())
				{
					bool hasAttachment = HasAttachments(field.AsObject());
					if (hasAttachment)
					{
						return true;
					}
				}

				if (field.IsArray())
				{
					foreach (CbField subField in field.AsArray())
					{
						bool hasAttachment = FieldHasAttachments(subField);
						if (hasAttachment)
						{
							return true;
						}
					}
				}

				return field.IsAttachment();
			}

			return payload.Any(FieldHasAttachments);
		}

		public async Task<(RefRecord, BlobContents?)> GetAsync(NamespaceId ns, BucketId bucket, RefId key, string[] fields, bool doLastAccessTracking)
		{
			using IStorageClient storageClient = _storageClientFactory.CreateClient(ns);

			DdcRefNode? node = await storageClient.TryReadRefTargetAsync<DdcRefNode>(GetRefName(bucket, key));
			if (node == null)
			{
				throw new RefNotFoundException(ns, bucket, key);
			}

			BlobData data = await node.References.First(x => x.Hash == node.RootHash).ReadBlobDataAsync();
			BlobContents contents = new BlobContents(data.Data.ToArray());

			RefRecord record = new RefRecord(ns, bucket, key, DateTime.UtcNow, null, BlobId.FromIoHash(node.RootHash), true);
			return (record, contents);
		}

		public async Task<List<BlobId>> GetReferencedBlobsAsync(NamespaceId ns, BucketId bucket, RefId key)
		{
			using IStorageClient storageClient = _storageClientFactory.CreateClient(ns);

			DdcRefNode? node = await storageClient.TryReadRefTargetAsync<DdcRefNode>(GetRefName(bucket, key));
			if (node == null)
			{
				throw new RefNotFoundException(ns, bucket, key);
			}

			return node.References.Select(x => BlobId.FromIoHash(x.Hash)).ToList();
		}

		public async Task<(ContentId[], BlobId[])> PutAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId blobHash, CbObject payload)
		{
			await _blobService.PutObjectAsync(ns, payload.GetView().ToArray(), blobHash);
			return await FinalizeAsync(ns, bucket, key, blobHash);
		}

		public IAsyncEnumerable<NamespaceId> GetNamespacesAsync()
		{
			throw new NotImplementedException();
		}

		public Task<long> DropNamespaceAsync(NamespaceId ns)
		{
			throw new NotImplementedException();
		}

		public Task<long> DeleteBucketAsync(NamespaceId ns, BucketId bucket)
		{
			throw new NotImplementedException();
		}

		public Task<List<BlobId>> GetReferencedBlobsAsync(NamespaceId ns, BucketId bucket, RefId key, bool ignoreMissingBlobs)
		{
			throw new NotImplementedException();
		}
	}
}
