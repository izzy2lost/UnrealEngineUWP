// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;

namespace Jupiter.Implementation
{
	public interface IObjectService
	{
		Task<(ObjectRecord, BlobContents?)> Get(NamespaceId ns, BucketId bucket, IoHashKey key, string[] fields, bool doLastAccessTracking = true);
		Task<(ContentId[], BlobId[])> Put(NamespaceId ns, BucketId bucket, IoHashKey key, BlobId blobHash, CbObject payload);
		Task<(ContentId[], BlobId[])> Finalize(NamespaceId ns, BucketId bucket, IoHashKey key, BlobId blobHash);

		IAsyncEnumerable<NamespaceId> GetNamespaces();

		Task<bool> Delete(NamespaceId ns, BucketId bucket, IoHashKey key);
		Task<long> DropNamespace(NamespaceId ns);
		Task<long> DeleteBucket(NamespaceId ns, BucketId bucket);

		Task<bool> Exists(NamespaceId ns, BucketId bucket, IoHashKey key);
		Task<List<BlobId>> GetReferencedBlobs(NamespaceId ns, BucketId bucket, IoHashKey key);
	}

	public class ObjectHashMismatchException : Exception
	{
		public ObjectHashMismatchException(NamespaceId ns, BucketId bucket, IoHashKey name, BlobId suppliedHash, BlobId actualHash) : base($"Object {name} in bucket {bucket} and namespace {ns} did not reference hash {suppliedHash} was referencing {actualHash}")
		{
		}
	}
}
