// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Storage;
using System;
using System.Threading.Tasks;

namespace Horde.Server.Ddc
{
	class ContentIdService : IContentIdService
	{
		public Task<BlobId[]?> ResolveAsync(NamespaceId ns, ContentId contentId, bool mustBeContentId = false)
		{
			return Task.FromResult<BlobId[]?>(null);
		}

		public Task PutAsync(NamespaceId ns, ContentId contentId, BlobId blobIdentifier, int contentWeight)
		{
			return Task.CompletedTask;
		}
	}
}
