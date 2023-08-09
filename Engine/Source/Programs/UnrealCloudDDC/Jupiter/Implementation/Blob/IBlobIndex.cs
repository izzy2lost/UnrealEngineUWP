// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;

namespace Jupiter.Implementation.Blob
{

	public interface IBlobIndex
	{
		Task AddBlobToIndex(NamespaceId ns, BlobId id, string? region = null);

		Task RemoveBlobFromRegion(NamespaceId ns, BlobId id, string? region = null);

		Task<bool> BlobExistsInRegion(NamespaceId ns, BlobId blobIdentifier, string? region = null);
		IAsyncEnumerable<(NamespaceId, BlobId)> GetAllBlobs();

		IAsyncEnumerable<BaseBlobReference> GetBlobReferences(NamespaceId ns, BlobId id);
		Task AddRefToBlobs(NamespaceId ns, BucketId bucket, RefId key, BlobId[] blobs);

		Task RemoveReferences(NamespaceId ns, BlobId id, List<BaseBlobReference> referencesToRemove);
		Task<List<string>> GetBlobRegions(NamespaceId ns, BlobId blob);
		Task AddBlobReferences(NamespaceId ns, BlobId sourceBlob, BlobId targetBlob);
	}

	public abstract class BaseBlobReference
	{

	}

	public class RefBlobReference : BaseBlobReference
	{
		public RefBlobReference(BucketId bucket, RefId key)
		{
			Bucket = bucket;
			Key = key;
		}

		public BucketId Bucket { get; set; }
		public RefId Key { get; set;}
	}

	public class BlobToBlobReference : BaseBlobReference
	{
		public BlobToBlobReference(BlobId blob)
		{
			Blob = blob;
		}

		public BlobId Blob { get; set; }
	}
}
