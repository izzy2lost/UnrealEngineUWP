// Copyright Epic Games, Inc. All Rights Reserved.

using System.Linq;
using System.Security.Claims;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Horde.Server.Acls;
using Horde.Server.Storage;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;

namespace Horde.Server.Ddc
{
	class RequestHelper : IRequestHelper
	{
		readonly StorageService _storageService;

		public RequestHelper(StorageService storageService)
		{
			_storageService = storageService;
		}

		public Task<ActionResult?> HasAccessToNamespaceAsync(ClaimsPrincipal user, HttpRequest request, NamespaceId ns, AclAction[] aclActions)
		{
			using IServerStorageClient? storageClient = _storageService.TryCreateClient(ns);
			if (storageClient == null)
			{
				return Task.FromResult<ActionResult?>(new ForbidResult());
			}
			if (aclActions.Any(x => !storageClient.Authorize(x, user)))
			{
				return Task.FromResult<ActionResult?>(new ForbidResult());
			}
			return Task.FromResult<ActionResult?>(null);
		}
	}
}
