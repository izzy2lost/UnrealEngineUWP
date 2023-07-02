// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Jupiter.Implementation;

namespace Horde.Server.Ddc
{
	class ObjectService : IObjectService
	{
		public Task<bool> Delete(NamespaceId ns, BucketId bucket, IoHashKey key)
		{
			throw new System.NotImplementedException();
		}

		public Task<long> DeleteBucket(NamespaceId ns, BucketId bucket)
		{
			throw new System.NotImplementedException();
		}

		public Task<long> DropNamespace(NamespaceId ns)
		{
			throw new System.NotImplementedException();
		}

		public Task<bool> Exists(NamespaceId ns, BucketId bucket, IoHashKey key)
		{
			throw new System.NotImplementedException();
		}

		public Task<(ContentId[], BlobIdentifier[])> Finalize(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier blobHash)
		{
			throw new System.NotImplementedException();
		}

		public Task<(ObjectRecord, BlobContents?)> Get(NamespaceId ns, BucketId bucket, IoHashKey key, string[] fields, bool doLastAccessTracking = true)
		{
			throw new System.NotImplementedException();
		}

		public IAsyncEnumerable<NamespaceId> GetNamespaces()
		{
			throw new System.NotImplementedException();
		}

		public Task<List<BlobIdentifier>> GetReferencedBlobs(NamespaceId ns, BucketId bucket, IoHashKey key)
		{
			throw new System.NotImplementedException();
		}

		public Task<(ContentId[], BlobIdentifier[])> Put(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier blobHash, CbObject payload)
		{
			throw new System.NotImplementedException();
		}
	}
}
