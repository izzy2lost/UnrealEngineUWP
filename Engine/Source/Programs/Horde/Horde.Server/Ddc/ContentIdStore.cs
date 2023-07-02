// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Storage;
using System;
using System.Threading.Tasks;

namespace Horde.Server.Ddc
{
	class ContentIdStore : IContentIdStore
	{
		public Task<BlobIdentifier[]?> Resolve(NamespaceId ns, ContentId contentId, bool mustBeContentId = false)
		{
			throw new NotImplementedException();
		}

		public Task Put(NamespaceId ns, ContentId contentId, BlobIdentifier blobIdentifier, int contentWeight)
		{
			throw new NotImplementedException();
		}
	}
}
