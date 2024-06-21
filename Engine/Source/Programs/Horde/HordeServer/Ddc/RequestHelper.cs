// Copyright Epic Games, Inc. All Rights Reserved.

using System.Linq;
using System.Security.Claims;
using System.Threading.Tasks;
using EpicGames.Horde.Acls;
using EpicGames.Horde.Storage;
using HordeServer.Server;
using HordeServer.Storage;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace HordeServer.Ddc
{
	class RequestHelper : IRequestHelper
	{
		readonly IStorageClientFactory _storageClientFactory;
		readonly GlobalConfig _globalConfig;

		public RequestHelper(IStorageClientFactory storageClientFactory, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_storageClientFactory = storageClientFactory;
			_globalConfig = globalConfig.Value;
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
			if (!_globalConfig.Storage.TryGetNamespace(ns, out NamespaceConfig? namespaceConfig) || aclActions.Any(x => !namespaceConfig.Authorize(x, user)))
			{
				return Task.FromResult<ActionResult?>(new ForbidResult());
			}
			return Task.FromResult<ActionResult?>(null);
		}
	}
}
