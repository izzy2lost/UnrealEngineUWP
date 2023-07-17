// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;

namespace Horde.Server.Ddc
{
	class DdcBlobService : IDdcBlobService
	{
		public Task DeleteNamespaceAsync(NamespaceId ns)
		{
			throw new NotImplementedException();
		}

		public Task DeleteObjectAsync(NamespaceId ns, BlobId blob)
		{
			throw new NotImplementedException();
		}

		public Task<bool> ExistsAsync(NamespaceId ns, BlobId blob, List<string>? storageLayers = null)
		{
			throw new NotImplementedException();
		}

		public Task<bool> ExistsInRootStore(NamespaceId ns, BlobId blob)
		{
			throw new NotImplementedException();
		}

		public Task<BlobId[]> FilterOutKnownBlobsAsync(NamespaceId ns, IEnumerable<BlobId> blobs)
		{
			throw new NotImplementedException();
		}

		public Task<BlobId[]> FilterOutKnownBlobsAsync(NamespaceId ns, IAsyncEnumerable<BlobId> blobs)
		{
			throw new NotImplementedException();
		}

		public Task<BlobContents> GetObjectAsync(NamespaceId ns, BlobId blob, List<string>? storageLayers = null, bool supportsRedirectUri = false)
		{
			throw new NotImplementedException();
		}

		public Task<BlobContents> GetObjectsAsync(NamespaceId ns, BlobId[] refRequestBlobReferences)
		{
			throw new NotImplementedException();
		}

		public Task<Uri?> GetObjectWithRedirectAsync(NamespaceId ns, BlobId blobIdentifier, List<string>? storageLayers = null)
		{
			throw new NotImplementedException();
		}

		public IAsyncEnumerable<(BlobId, DateTime)> ListObjects(NamespaceId ns)
		{
			throw new NotImplementedException();
		}

		public Task<Uri?> MaybePutObjectWithRedirectAsync(NamespaceId ns, BlobId identifier)
		{
			throw new NotImplementedException();
		}

		public Task<BlobId> PutObjectAsync(NamespaceId ns, BufferedPayload payload, BlobId identifier)
		{
			throw new NotImplementedException();
		}

		public Task<BlobId> PutObjectAsync(NamespaceId ns, byte[] payload, BlobId identifier)
		{
			throw new NotImplementedException();
		}

		public Task<BlobId> PutObjectKnownHashAsync(NamespaceId ns, BufferedPayload content, BlobId identifier)
		{
			throw new NotImplementedException();
		}

		public Task<BlobContents> ReplicateObjectAsync(NamespaceId ns, BlobId blob, bool force = false)
		{
			throw new NotImplementedException();
		}

		public bool ShouldFetchBlobOnDemand(NamespaceId ns)
		{
			throw new NotImplementedException();
		}

		public Task<IoHash> VerifyContentMatchesHashAsync(Stream content, IoHash identifier)
		{
			throw new NotImplementedException();
		}
	}
}
