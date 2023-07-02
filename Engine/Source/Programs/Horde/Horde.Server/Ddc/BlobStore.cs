// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;

namespace Horde.Server.Ddc
{
	class BlobStore : IBlobStore
	{
		public Task DeleteNamespace(NamespaceId ns)
		{
			throw new NotImplementedException();
		}

		public Task DeleteObject(NamespaceId ns, BlobIdentifier blob)
		{
			throw new NotImplementedException();
		}

		public Task<bool> Exists(NamespaceId ns, BlobIdentifier blob, bool forceCheck = false)
		{
			throw new NotImplementedException();
		}

		public Task<BlobContents> GetObject(NamespaceId ns, BlobIdentifier blob, LastAccessTrackingFlags flags = LastAccessTrackingFlags.DoTracking, bool supportsRedirectUri = false)
		{
			throw new NotImplementedException();
		}

		public Task<Uri?> GetObjectByRedirect(NamespaceId ns, BlobIdentifier blob)
		{
			throw new NotImplementedException();
		}

		public IAsyncEnumerable<(BlobIdentifier, DateTime)> ListObjects(NamespaceId ns)
		{
			throw new NotImplementedException();
		}

		public Task<BlobIdentifier> PutObject(NamespaceId ns, byte[] blob, BlobIdentifier identifier)
		{
			throw new NotImplementedException();
		}

		public Task<BlobIdentifier> PutObject(NamespaceId ns, ReadOnlyMemory<byte> blob, BlobIdentifier identifier)
		{
			throw new NotImplementedException();
		}

		public Task<BlobIdentifier> PutObject(NamespaceId ns, Stream content, BlobIdentifier identifier)
		{
			throw new NotImplementedException();
		}

		public Task<Uri?> PutObjectWithRedirect(NamespaceId ns, BlobIdentifier identifier)
		{
			throw new NotImplementedException();
		}
	}
}
