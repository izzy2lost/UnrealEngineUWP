// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;

namespace Horde.Server.Ddc
{
	class DdcRefService : IDdcRefService
	{
		public Task<bool> DeleteAsync(NamespaceId ns, BucketId bucket, RefId key)
		{
			throw new System.NotImplementedException();
		}

		public Task<long> DeleteBucketAsync(NamespaceId ns, BucketId bucket)
		{
			throw new System.NotImplementedException();
		}

		public Task<long> DropNamespaceAsync(NamespaceId ns)
		{
			throw new System.NotImplementedException();
		}

		public Task<bool> ExistsAsync(NamespaceId ns, BucketId bucket, RefId key)
		{
			throw new System.NotImplementedException();
		}

		public Task<(ContentId[], BlobId[])> FinalizeAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId blobHash)
		{
			throw new System.NotImplementedException();
		}

		public Task<(RefRecord, BlobContents?)> GetAsync(NamespaceId ns, BucketId bucket, RefId key, string[] fields, bool doLastAccessTracking = true)
		{
			throw new System.NotImplementedException();
		}

		public IAsyncEnumerable<NamespaceId> GetNamespacesAsync()
		{
			throw new System.NotImplementedException();
		}

		public Task<List<BlobId>> GetReferencedBlobsAsync(NamespaceId ns, BucketId bucket, RefId key)
		{
			throw new System.NotImplementedException();
		}

		public Task<(ContentId[], BlobId[])> PutAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId blobHash, CbObject payload)
		{
			throw new System.NotImplementedException();
		}
	}
}
