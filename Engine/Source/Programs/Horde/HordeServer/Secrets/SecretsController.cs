// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Secrets;
using Horde.Server.Server;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace Horde.Server.Secrets
{
	/// <summary>
	/// Controller for the /api/v1/secrets endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class SecretsController : HordeControllerBase
	{
		readonly ISecretCollection _secretCollection;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public SecretsController(ISecretCollection secretCollection, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_secretCollection = secretCollection;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Query all the secrets available for the current user
		/// </summary>
		[HttpGet]
		[Route("/api/v1/secrets")]
		public ActionResult<GetSecretsResponse> GetSecrets()
		{
			List<SecretId> secretIds = new List<SecretId>();
			foreach (SecretConfig secret in _globalConfig.Value.Secrets)
			{
				if (secret.Authorize(SecretAclAction.ViewSecret, User))
				{
					secretIds.Add(secret.Id);
				}
			}
			return new GetSecretsResponse(secretIds);
		}

		/// <summary>
		/// Retrieve information about a specific secret
		/// </summary>
		/// <param name="secretId">Id of the secret to retrieve</param>
		/// <param name="filter">Filter for properties to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about the requested secret</returns>
		[HttpGet]
		[Route("/api/v1/secrets/{secretId}")]
		[ProducesResponseType(typeof(GetSecretResponse), 200)]
		public async Task<ActionResult<object>> GetSecretAsync(SecretId secretId, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			ISecret? secret = await _secretCollection.GetAsync(secretId, cancellationToken);
			if (secret == null)
			{
				return NotFound(secretId);
			}
			if (!_globalConfig.Value.Authorize(secret.Id, SecretAclAction.ViewSecret, User))
			{
				return Forbid(SecretAclAction.ViewSecret, secretId);
			}

			return new GetSecretResponse(secret.Id, secret.Data.ToDictionary(x => x.Key, x => x.Value)).ApplyFilter(filter);
		}
	}
}
