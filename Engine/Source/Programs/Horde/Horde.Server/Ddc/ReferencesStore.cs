// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;

namespace Horde.Server.Ddc
{
	class ReferencesStore : IReferencesStore
	{
		public Task<bool> Delete(NamespaceId ns, BucketId bucket, IoHashKey key)
		{
			throw new NotImplementedException();
		}

		public Task<long> DeleteBucket(NamespaceId ns, BucketId bucket)
		{
			throw new NotImplementedException();
		}

		public Task<long> DropNamespace(NamespaceId ns)
		{
			throw new NotImplementedException();
		}

		public Task Finalize(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier blobIdentifier)
		{
			throw new NotImplementedException();
		}

		public Task<ObjectRecord> Get(NamespaceId ns, BucketId bucket, IoHashKey key, IReferencesStore.FieldFlags flags)
		{
			throw new NotImplementedException();
		}

		public IAsyncEnumerable<NamespaceId> GetNamespaces()
		{
			throw new NotImplementedException();
		}

		public IAsyncEnumerable<(NamespaceId, BucketId, IoHashKey, DateTime)> GetRecords()
		{
			throw new NotImplementedException();
		}

		public Task Put(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier blobHash, byte[] blob, bool isFinalized)
		{
			throw new NotImplementedException();
		}

		public Task UpdateLastAccessTime(NamespaceId ns, BucketId bucket, IoHashKey key, DateTime newLastAccessTime)
		{
			throw new NotImplementedException();
		}
	}
}
