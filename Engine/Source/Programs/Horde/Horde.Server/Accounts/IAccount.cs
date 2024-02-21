// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Horde.Accounts;
using Horde.Server.Users;

namespace Horde.Server.Accounts
{
	/// <summary>
	/// An internal Horde account representing a user or service
	///
	/// Service-to-service authentication always use this for authentication (for example, Robomerge accessing Horde)
	/// When external authentication is enabled (such as OpenID Connect) users cannot be authenticated through this.
	/// </summary>
	public interface IAccount
	{
		/// <summary>
		/// Unique internal ID for this Horde account
		/// </summary>
		AccountId Id { get; }

		/// <summary>
		/// Full name of the user
		/// </summary>
		string Name { get; }

		/// <summary>
		/// A login ID or username
		/// </summary>
		string Login { get; }

		/// <summary>
		/// Email associated with account
		/// </summary>
		string? Email { get; }

		/// <summary>
		/// Secret token used for identifying API calls made as the account
		/// </summary>
		string? SecretToken { get; }

		/// <summary>
		/// Hashed password
		/// </summary>
		string? PasswordHash { get; }

		/// <summary>
		/// Salt for password hash (if PasswordHash is set)
		/// </summary>
		string? PasswordSalt { get; }

		/// <summary>
		/// If the account is active
		/// </summary>
		bool Enabled { get; }

		/// <summary>
		/// Description of the account (who is it for, is there an owner etc)
		/// </summary>
		string Description { get; }

		/// <summary>
		/// Get list of claims
		/// </summary>
		/// <returns>List of claims</returns>
		IReadOnlyList<IUserClaim> Claims { get; }
	}
}
