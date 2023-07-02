// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;

namespace Horde.Server.Ddc
{
	class BlobService : IBlobService
	{
		public Task DeleteNamespace(NamespaceId ns)
		{
			throw new NotImplementedException();
		}

		public Task DeleteObject(NamespaceId ns, BlobIdentifier blob)
		{
			throw new NotImplementedException();
		}

		public Task<bool> Exists(NamespaceId ns, BlobIdentifier blob, List<string>? storageLayers = null)
		{
			throw new NotImplementedException();
		}

		public Task<bool> ExistsInRootStore(NamespaceId ns, BlobIdentifier blob)
		{
			throw new NotImplementedException();
		}

		public Task<BlobIdentifier[]> FilterOutKnownBlobs(NamespaceId ns, IEnumerable<BlobIdentifier> blobs)
		{
			throw new NotImplementedException();
		}

		public Task<BlobIdentifier[]> FilterOutKnownBlobs(NamespaceId ns, IAsyncEnumerable<BlobIdentifier> blobs)
		{
			throw new NotImplementedException();
		}

		public Task<BlobContents> GetObject(NamespaceId ns, BlobIdentifier blob, List<string>? storageLayers = null, bool supportsRedirectUri = false)
		{
			throw new NotImplementedException();
		}

		public Task<BlobContents> GetObjects(NamespaceId ns, BlobIdentifier[] refRequestBlobReferences)
		{
			throw new NotImplementedException();
		}

		public Task<Uri?> GetObjectWithRedirect(NamespaceId ns, BlobIdentifier blobIdentifier, List<string>? storageLayers = null)
		{
			throw new NotImplementedException();
		}

		public IAsyncEnumerable<(BlobIdentifier, DateTime)> ListObjects(NamespaceId ns)
		{
			throw new NotImplementedException();
		}

		public Task<Uri?> MaybePutObjectWithRedirect(NamespaceId ns, BlobIdentifier identifier)
		{
			throw new NotImplementedException();
		}

		public Task<BlobIdentifier> PutObject(NamespaceId ns, IBufferedPayload payload, BlobIdentifier identifier)
		{
			throw new NotImplementedException();
		}

		public Task<BlobIdentifier> PutObject(NamespaceId ns, byte[] payload, BlobIdentifier identifier)
		{
			throw new NotImplementedException();
		}

		public Task<BlobIdentifier> PutObjectKnownHash(NamespaceId ns, IBufferedPayload content, BlobIdentifier identifier)
		{
			throw new NotImplementedException();
		}

		public Task<BlobContents> ReplicateObject(NamespaceId ns, BlobIdentifier blob, bool force = false)
		{
			throw new NotImplementedException();
		}

		public bool ShouldFetchBlobOnDemand(NamespaceId ns)
		{
			throw new NotImplementedException();
		}

		public Task<JupiterContentHash> VerifyContentMatchesHash(Stream content, JupiterContentHash identifier)
		{
			throw new NotImplementedException();
		}
	}
}
