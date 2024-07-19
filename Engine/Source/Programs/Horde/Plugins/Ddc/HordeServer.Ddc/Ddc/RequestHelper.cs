// Copyright Epic Games, Inc. All Rights Reserved.

using System.Security.Claims;
using EpicGames.Horde.Acls;
using EpicGames.Horde.Storage;
using HordeServer.Storage;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace HordeServer.Ddc
{
	class RequestHelper : IRequestHelper
	{
		readonly IStorageClientFactory _storageClientFactory;
		readonly StorageConfig _storageConfig;

		public RequestHelper(IStorageClientFactory storageClientFactory, IOptionsSnapshot<StorageConfig> storageConfig)
		{
			_storageClientFactory = storageClientFactory;
			_storageConfig = storageConfig.Value;
		}

		public Task<ActionResult?> HasAccessForGlobalOperationsAsync(ClaimsPrincipal user, AclAction[] aclActions)
		{
			throw new System.NotImplementedException();
		}

		public Task<ActionResult?> HasAccessToNamespaceAsync(ClaimsPrincipal user, HttpRequest request, NamespaceId ns, AclAction[] aclActions)
		{
			using IStorageClient? storageClient = _storageClientFactory.TryCreateClient(ns);
			if (storageClient == null)
			{
				return Task.FromResult<ActionResult?>(new ForbidResult());
			}
			if (!_storageConfig.TryGetNamespace(ns, out NamespaceConfig? namespaceConfig) || aclActions.Any(x => !namespaceConfig.Authorize(x, user)))
			{
				return Task.FromResult<ActionResult?>(new ForbidResult());
			}
			return Task.FromResult<ActionResult?>(null);
		}
	}
}
