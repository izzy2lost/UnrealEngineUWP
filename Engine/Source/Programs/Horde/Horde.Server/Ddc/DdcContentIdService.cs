// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Storage;
using System;
using System.Threading.Tasks;

namespace Horde.Server.Ddc
{
	class ContentIdStore : IDdcContentIdService
	{
		public Task<BlobId[]?> ResolveAsync(NamespaceId ns, ContentId contentId, bool mustBeContentId = false)
		{
			throw new NotImplementedException();
		}

		public Task PutAsync(NamespaceId ns, ContentId contentId, BlobId blobIdentifier, int contentWeight)
		{
			throw new NotImplementedException();
		}
	}
}
