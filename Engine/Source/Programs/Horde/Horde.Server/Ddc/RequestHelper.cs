// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using System.Security.Claims;
using System.Threading;
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

		public async Task<ActionResult?> HasAccessToNamespaceAsync(ClaimsPrincipal user, HttpRequest request, NamespaceId ns, AclAction[] aclActions)
		{
			StorageClient? storageClient = await _storageService.TryGetClientAsync(ns, CancellationToken.None);
			if (storageClient == null)
			{
				return new ForbidResult();
			}
			if (!aclActions.Any(x => !storageClient.Config.Authorize(x, user)))
			{
				return new ForbidResult();
			}
			return null;
		}
	}
}
