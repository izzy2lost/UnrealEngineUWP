// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Security.Claims;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Horde.Server.Acls;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;

namespace Horde.Server.Ddc
{
	class RequestHelper : IRequestHelper
	{
		public Task<ActionResult?> HasAccessForGlobalOperations(ClaimsPrincipal user, AclAction[] aclActions)
		{
			throw new NotImplementedException();
		}

		public Task<ActionResult?> HasAccessToNamespace(ClaimsPrincipal user, HttpRequest request, NamespaceId ns, AclAction[] aclActions)
		{
			throw new NotImplementedException();
		}
	}
}
