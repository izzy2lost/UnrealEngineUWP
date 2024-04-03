// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.OIDC;

#pragma warning disable CA1721 // Property names should not match get methods

/// <summary>
/// A fake implementation of OidcTokenManager for testing
/// Assumes just a single provider.
/// </summary>
public class FakeOidcTokenManager : IOidcTokenManager
{
	/// <summary>
	/// Refresh token in use
	/// </summary>
	public string? RefreshToken { get; set; }
	
	/// <summary>
	/// Latest access token
	/// </summary>
	public string? AccessToken { get; set; }
	
	/// <summary>
	/// Expiry time for access token
	/// </summary>
	public DateTimeOffset AccessTokenExpiry { get; set; } = DateTimeOffset.UnixEpoch;
	
	private int _refreshCounter = 1;
	private int _accessCounter = 1;
	
	/// <inheritdoc/>
	public Task<OidcTokenInfo> LoginAsync(string providerIdentifier, CancellationToken cancellationToken = default)
	{
		Console.WriteLine("FakeOidcTokenManager.LoginAsync");
		RefreshToken = "fakeRefreshToken-" + _refreshCounter++;
		RefreshAccessToken();
		return Task.FromResult(GetOidcTokenInfo());
	}

	/// <inheritdoc/>
	public Task<OidcTokenInfo> GetAccessToken(string providerIdentifier, CancellationToken cancellationToken = default)
	{
		throw new NotImplementedException("Method not in use");
	}

	/// <inheritdoc/>
	public Task<OidcTokenInfo?> TryGetAccessToken(string providerIdentifier, CancellationToken cancellationToken = default)
	{
		Console.WriteLine("FakeOidcTokenManager.TryGetAccessToken");
		if (String.IsNullOrEmpty(RefreshToken))
		{
			throw new NotLoggedInException();
		}
		
		bool tokenValid = AccessTokenExpiry.AddMinutes(2) > DateTime.Now || AccessTokenExpiry == DateTimeOffset.MinValue;
		if (!String.IsNullOrEmpty(AccessToken) && tokenValid)
		{
			return Task.FromResult(GetOidcTokenInfo())!;
		}
		
		// Refresh access token with provider
		RefreshAccessToken();
		
		// Assume the updated access token is valid
		return Task.FromResult(GetOidcTokenInfo())!;
	}

	/// <inheritdoc/>
	public OidcStatus GetStatusForProvider(string providerIdentifier)
	{
		Console.WriteLine("FakeOidcTokenManager.GetStatusForProvider");
		return OidcTokenClient.GetStatus(RefreshToken, AccessToken, AccessTokenExpiry);
	}

	private OidcTokenInfo GetOidcTokenInfo()
	{
		return new OidcTokenInfo { RefreshToken = RefreshToken, AccessToken = AccessToken, TokenExpiry = AccessTokenExpiry };
	}

	/// <summary>
	/// Perform fake refresh of the access token
	/// </summary>
	private void RefreshAccessToken()
	{
		AccessToken = "fakeAccessToken-" + _accessCounter++;;
		AccessTokenExpiry = DateTimeOffset.UtcNow + TimeSpan.FromHours(1);
	}
}
