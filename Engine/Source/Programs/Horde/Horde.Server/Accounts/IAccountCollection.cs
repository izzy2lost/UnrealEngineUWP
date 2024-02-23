// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Accounts;
using Horde.Server.Users;

namespace Horde.Server.Accounts
{
	/// <summary>
	/// Interface for a collection of accounts
	/// </summary>
	public interface IAccountCollection
	{
		/// <summary>
		/// Adds a new account to the collection
		/// </summary>
		/// <param name="name">Name of the account (for example a given full name or name of service)</param>
		/// <param name="login">Login ID or username</param>
		/// <param name="claims">Optional list of claims</param>
		/// <param name="description">Optional description</param>
		/// <param name="email">Optional e-mail address</param>
		/// <param name="secretToken">Optional secret token to authenticate for API based auth</param>
		/// <param name="password">Optional password for interactive login</param>
		/// <param name="enabled">Whether the account should be enabled</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task<IAccount> AddAsync(
			string name,
			string login,
			IReadOnlyList<IUserClaim>? claims = null,
			string? description = null,
			string? email = null,
			string? secretToken = null,
			string? password = null,
			bool? enabled = null,
			CancellationToken cancellationToken = default);

		/// <summary>
		/// Searches service accounts
		/// </summary>
		/// <param name="index">Index of the first account</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The service account</returns>
		Task<IReadOnlyList<IAccount>> FindAsync(int? index = null, int? count = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Get service account via secret token
		/// </summary>
		/// <param name="secretToken">Secret token to use for searching</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The service account</returns>
		Task<IAccount?> FindBySecretTokenAsync(string secretToken, CancellationToken cancellationToken = default);

		/// <summary>
		/// Get an account via login ID
		/// </summary>
		/// <param name="login">Login or username to use for searching</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The service account</returns>
		Task<IAccount?> FindByLoginAsync(string login, CancellationToken cancellationToken = default);

		/// <summary>
		/// Get an account via Username
		/// </summary>
		/// <param name="username">Username to use for searching</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The service account</returns>
		Task<IAccount?> FindByUsernameAsync(string username, CancellationToken cancellationToken = default);

		/// <summary>
		/// Get service account via ID
		/// </summary>
		/// <param name="id">The unique service account id</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The service account</returns>
		Task<IAccount?> GetAsync(AccountId id, CancellationToken cancellationToken = default);

		/// <summary>
		/// Delete a service account from the collection
		/// </summary>
		/// <param name="id">The service account</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task DeleteAsync(AccountId id, CancellationToken cancellationToken = default);
	}
}
