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
		/// Get service account via ID
		/// </summary>
		/// <param name="id">The unique service account id</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The service account</returns>
		Task<IAccount?> GetAsync(AccountId id, CancellationToken cancellationToken = default);

		/// <summary>
		/// Get service account via secret token
		/// </summary>
		/// <param name="secretToken">Secret token to use for searching</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The service account</returns>
		Task<IAccount?> GetBySecretTokenAsync(string secretToken, CancellationToken cancellationToken = default);

		/// <summary>
		/// Get an account via login ID
		/// </summary>
		/// <param name="login">Login or username to use for searching</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The service account</returns>
		Task<IAccount?> GetByLoginAsync(string login, CancellationToken cancellationToken = default);

		/// <summary>
		/// Update an account from the collection
		/// </summary>
		/// <param name="id">Account ID</param>
		/// <param name="name">If set, name of account to update</param>
		/// <param name="login">If set, login ID/username to update</param>
		/// <param name="claims">If set, claims to update</param>
		/// <param name="description">If set, description to update</param>
		/// <param name="email">If set, email to update</param>
		/// <param name="secretToken">If set, secret token will be set</param>
		/// <param name="password">If set, password hash to update</param>
		/// <param name="enabled">If set, enabled flag to update</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task UpdateAsync(
			AccountId id,
			string? name = null,
			string? login = null,
			IReadOnlyList<IUserClaim>? claims = null,
			string? description = null,
			string? email = null,
			string? secretToken = null,
			string? password = null,
			bool? enabled = null,
			CancellationToken cancellationToken = default);

		/// <summary>
		/// Delete a service account from the collection
		/// </summary>
		/// <param name="id">The service account</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task DeleteAsync(AccountId id, CancellationToken cancellationToken = default);
	}
}
