// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;

namespace EpicGames.Horde.Accounts
{
	/// <summary>
	/// Creates a new user account
	/// </summary>
	/// <param name="Name">Name of the user</param>
	/// <param name="Login">Perforce login identifier</param>
	/// <param name="Claims">Claims for the user</param>
	/// <param name="Description">Description for the account</param>
	/// <param name="Email">User's email address</param>
	/// <param name="SecretToken">Optional secret token for API access</param>
	/// <param name="Password">Password for the user</param>
	/// <param name="Enabled">Whether the account is enabled</param>
	public record class CreateAccountRequest(string Name, string Login, List<AccountClaimMessage> Claims, string? Description, string? Email, string? SecretToken, string? Password, bool? Enabled);

	/// <summary>
	/// Response from the request to create a new user account
	/// </summary>
	/// <param name="Id">The created account id</param>
	public record class CreateAccountResponse(AccountId Id);

	/// <summary>
	/// Update request for a user account
	/// </summary>
	/// <param name="Name">Name of the user</param>
	/// <param name="Login">Perforce login identifier</param>
	/// <param name="Claims">Claims for the user</param>
	/// <param name="Description">Description for the account</param>
	/// <param name="Email">User's email address</param>
	/// <param name="SecretToken">Optional secret token for API access</param>
	/// <param name="Password">Password for the user</param>
	/// <param name="Enabled">Whether the account is enabled</param>
	public record class UpdateAccountRequest(string? Name, string? Login, List<AccountClaimMessage>? Claims, string? Description, string? Email, string? SecretToken, string? Password, bool? Enabled);

	/// <summary>
	/// Update request for the current user account
	/// </summary>
	/// <param name="Password">Password for the user</param>
	public record class UpdateCurrentAccountRequest(string? Password);

	/// <summary>
	/// Creates a new user account
	/// </summary>
	/// <param name="Name">Name of the user</param>
	/// <param name="Login">Perforce login identifier</param>
	/// <param name="Claims">Claims for the user</param>
	/// <param name="Description">Description for the account</param>
	/// <param name="Email">User's email address</param>
	/// <param name="Enabled">Whether the account is enabled</param>
	public record class GetAccountResponse(string Name, string Login, List<AccountClaimMessage> Claims, string? Description, string? Email, bool Enabled);

	/// <summary>
	/// Message describing a claim for an account
	/// </summary>
	/// <param name="Type">Claim type</param>
	/// <param name="Value">Value of the claim</param>
	public record class AccountClaimMessage(string Type, string Value);
}
