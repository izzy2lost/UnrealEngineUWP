// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IdentityModel.Tokens.Jwt;
using System.Linq;
using System.Security.Claims;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Sessions;
using HordeServer.Server;
using HordeServer.Utilities;
using Microsoft.IdentityModel.Tokens;
using MongoDB.Driver;

namespace HordeServer.Acls
{
	/// <summary>
	/// Wraps functionality for manipulating permissions
	/// </summary>
	public class AclService
	{
		private readonly GlobalsService _globalsService;

		/// <summary>
		/// Constructor
		/// </summary>
		public AclService(GlobalsService globalsService)
		{
			_globalsService = globalsService;
		}

		/// <summary>
		/// Issues a bearer token with the given roles
		/// </summary>
		/// <param name="claims">List of claims to include</param>
		/// <param name="expiry">Time that the token expires</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>JWT security token with a claim for creating new agents</returns>
		public async ValueTask<string> IssueBearerTokenAsync(IEnumerable<AclClaimConfig> claims, TimeSpan? expiry, CancellationToken cancellationToken = default)
		{
			return await IssueBearerTokenAsync(claims.Select(x => new Claim(x.Type, x.Value)), expiry, cancellationToken);
		}

		/// <summary>
		/// Issues a bearer token with the given claims
		/// </summary>
		/// <param name="claims">List of claims to include</param>
		/// <param name="expiry">Time that the token expires</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>JWT security token with a claim for creating new agents</returns>
		public async ValueTask<string> IssueBearerTokenAsync(IEnumerable<Claim> claims, TimeSpan? expiry, CancellationToken cancellationToken = default)
		{
			IGlobals globals = await _globalsService.GetAsync(cancellationToken);
			SigningCredentials signingCredentials = new(globals.JwtSigningKey, SecurityAlgorithms.HmacSha256);

			JwtSecurityToken token = new(globals.JwtIssuer, null, claims.DistinctBy(x => (x.Type, x.Value)), null, DateTime.UtcNow + expiry, signingCredentials);
			return new JwtSecurityTokenHandler().WriteToken(token);
		}

		/// <summary>
		/// Gets the agent id associated with a particular user
		/// </summary>
		/// <param name="user"></param>
		/// <returns></returns>
		public static AgentId? GetAgentId(ClaimsPrincipal user)
		{
			Claim? claim = user.Claims.FirstOrDefault(x => x.Type == HordeClaimTypes.Agent);
			if (claim == null)
			{
				return null;
			}
			else
			{
				return new AgentId(claim.Value);
			}
		}
	}

	internal static class ClaimExtensions
	{
		public static bool HasAdminClaim(this ClaimsPrincipal user)
		{
			return user.HasClaim(HordeClaims.AdminClaim.Type, HordeClaims.AdminClaim.Value);
		}

		public static bool HasAgentClaim(this ClaimsPrincipal user, AgentId agentId)
		{
			return user.HasClaim(HordeClaimTypes.Agent, agentId.ToString());
		}

		public static bool HasLeaseClaim(this ClaimsPrincipal user, LeaseId leaseId)
		{
			return user.HasClaim(HordeClaimTypes.Lease, leaseId.ToString());
		}

		public static bool HasSessionClaim(this ClaimsPrincipal user, SessionId sessionId)
		{
			return user.HasClaim(HordeClaimTypes.AgentSessionId, sessionId.ToString());
		}

		public static LeaseId? GetLeaseClaim(this ClaimsPrincipal user)
		{
			Claim? claim = user.FindFirst(HordeClaimTypes.Lease);
			if (claim == null || !LeaseId.TryParse(claim.Value, out LeaseId leaseIdValue))
			{
				return null;
			}
			else
			{
				return leaseIdValue;
			}
		}

		public static SessionId? GetSessionClaim(this ClaimsPrincipal user)
		{
			Claim? claim = user.FindFirst(HordeClaimTypes.AgentSessionId);
			if (claim == null || !SessionId.TryParse(claim.Value, out SessionId sessionIdValue))
			{
				return null;
			}
			else
			{
				return sessionIdValue;
			}
		}

		public static string GetSessionClaimsAsString(this ClaimsPrincipal user)
		{
			return String.Join(",", user.Claims
				.Where(c => c.Type == HordeClaimTypes.AgentSessionId)
				.Select(c => c.Value));
		}
	}
}
