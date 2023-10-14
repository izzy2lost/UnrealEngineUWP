// Copyright Epic Games, Inc. All Rights Reserved.

using System.Linq;
using System.Security.Claims;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Horde.Server.Acls;
using Horde.Server.Server;
using Horde.Server.Storage;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace Horde.Server.Ddc
{
	class RequestHelper : IRequestHelper
	{
		readonly StorageService _storageService;
		readonly GlobalConfig _globalConfig;

		public RequestHelper(StorageService storageService, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_storageService = storageService;
			_globalConfig = globalConfig.Value;
		}

		public Task<ActionResult?> HasAccessToNamespaceAsync(ClaimsPrincipal user, HttpRequest request, NamespaceId ns, AclAction[] aclActions)
		{
			using IStorageClient? storageClient = _storageService.TryCreateClient(ns);
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
