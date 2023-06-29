// Copyright Epic Games, Inc. All Rights Reserved.

using System.Security.Claims;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;

namespace Jupiter.Controllers
{
    public interface IRequestHelper
    {
        public Task<ActionResult?> HasAccessToNamespace(ClaimsPrincipal user, HttpRequest request, NamespaceId ns, JupiterAclAction[] aclActions);
        public Task<ActionResult?> HasAccessForGlobalOperations(ClaimsPrincipal user, JupiterAclAction[] aclActions);
    }
}
