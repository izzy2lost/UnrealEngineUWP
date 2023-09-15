// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage;
using Horde.Server.Storage;
using System.Linq;
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

		static Utf8String GetAlias(BlobId blobId) => BlobService.GetAlias(blobId);
		static Utf8String GetAlias(ContentId contentId) => $"cid:{contentId}";

		public async Task<BlobId[]?> ResolveAsync(NamespaceId ns, ContentId contentId, bool mustBeContentId = false)
		{
			CancellationToken cancellationToken = CancellationToken.None;

			using IStorageClient storageClient = _storageService.CreateClient(ns);

			BlobHandle? blobHandle = await storageClient.FindAliasAsync(GetAlias(contentId), cancellationToken).FirstOrDefaultAsync(cancellationToken);
			if (blobHandle == null && !mustBeContentId)
			{
				blobHandle = await storageClient.FindAliasAsync(GetAlias(contentId.AsBlobIdentifier()), cancellationToken).FirstOrDefaultAsync(cancellationToken);
			}
			if (blobHandle == null)
			{
				return null;
			}

			return new[] { BlobId.FromIoHash(blobHandle.Hash) };
		}

		public async Task PutAsync(NamespaceId ns, ContentId contentId, BlobId blobId, int contentWeight)
		{
			CancellationToken cancellationToken = CancellationToken.None;

			using IStorageClient storageClient = _storageService.CreateClient(ns);

			BlobHandle? blobHandle = await storageClient.FindAliasAsync(GetAlias(blobId), cancellationToken).FirstOrDefaultAsync(cancellationToken);
			if (blobHandle == null)
			{
				throw new BlobNotFoundException(ns, blobId);
			}

			await storageClient.AddAliasAsync(GetAlias(contentId), blobHandle, -contentWeight, cancellationToken);
		}
	}
}
