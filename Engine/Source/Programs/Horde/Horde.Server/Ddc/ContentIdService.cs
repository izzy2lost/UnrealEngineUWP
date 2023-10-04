// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Storage;
using Horde.Server.Storage;
using System.Threading;
using System.Threading.Tasks;

namespace Horde.Server.Ddc
{
	class ContentIdService : IContentIdService
	{
		readonly StorageService _storageService;

		public ContentIdService(StorageService storageService)
		{
			_storageService = storageService;
		}

		static string GetAlias(BlobId blobId) => BlobService.GetAlias(blobId);
		static string GetAlias(ContentId contentId) => $"cid:{contentId}";

		public async Task<BlobId[]?> ResolveAsync(NamespaceId ns, ContentId contentId, bool mustBeContentId = false)
		{
			CancellationToken cancellationToken = CancellationToken.None;

			using IStorageClient storageClient = _storageService.CreateClient(ns);

			BlobAlias? blobAlias = await storageClient.FindAliasAsync(GetAlias(contentId), cancellationToken);
			if (blobAlias == null && !mustBeContentId)
			{
				blobAlias = await storageClient.FindAliasAsync(GetAlias(contentId.AsBlobIdentifier()), cancellationToken);
			}
			if (blobAlias == null)
			{
				return null;
			}

			return new[] { BlobId.FromIoHash(blobAlias.Target.Hash) };
		}

		public async Task PutAsync(NamespaceId ns, ContentId contentId, BlobId blobId, int contentWeight)
		{
			CancellationToken cancellationToken = CancellationToken.None;

			using IStorageClient storageClient = _storageService.CreateClient(ns);

			BlobAlias? blobAlias = await storageClient.FindAliasAsync(GetAlias(blobId), cancellationToken);
			if (blobAlias == null)
			{
				throw new BlobNotFoundException(ns, blobId);
			}

			await storageClient.AddAliasAsync(GetAlias(contentId), blobAlias.Target, -contentWeight, cancellationToken: cancellationToken);
		}
	}
}
