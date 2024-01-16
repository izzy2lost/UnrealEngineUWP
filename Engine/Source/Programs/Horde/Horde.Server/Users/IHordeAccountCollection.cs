// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using MongoDB.Bson;

namespace Horde.Server.Users
{
	/// <summary>
	/// Interface for a collection of service account documents
	/// </summary>
	public interface IHordeAccountCollection
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
		Task<IHordeAccount> AddAsync(
			string name,
			string login,
			List<IUserClaim>? claims = null,
			string? description = null,
			string? email = null,
			string? secretToken = null,
			string? password = null);

		/// <summary>
		/// Get service account via ID
		/// </summary>
		/// <param name="id">The unique service account id</param>
		/// <returns>The service account</returns>
		Task<IHordeAccount?> GetAsync(ObjectId id);
		
		/// <summary>
		/// Get service account via secret token
		/// </summary>
		/// <param name="secretToken">Secret token to use for searching</param>
		/// <returns>The service account</returns>
		Task<IHordeAccount?> GetBySecretTokenAsync(string secretToken);
		
		/// <summary>
		/// Get an account via login ID
		/// </summary>
		/// <param name="login">Login or username to use for searching</param>
		/// <returns>The service account</returns>
		Task<IHordeAccount?> GetByLogin(string login);

		/// <summary>
		/// Update an account from the collection
		/// </summary>
		/// <param name="id">Account ID</param>
		/// <param name="name">If set, name of account to update</param>
		/// <param name="login">If set, login ID/username to update</param>
		/// <param name="email">If set, email to update</param>
		/// <param name="secretToken">If set, secret token will be set</param>
		/// <param name="passwordHash">If set, password hash to update</param>
		/// <param name="passwordSalt">If set, password salt to update</param>
		/// <param name="claims">If set, claims to update</param>
		/// <param name="enabled">If set, enabled flag to update</param>
		/// <param name="description">If set, description to update</param>
		/// <returns>Async task</returns>
		Task UpdateAsync(
			ObjectId id,
			string? name = null,
			string? login = null,
			string? email = null,
			string? secretToken = null,
			string? passwordHash = null,
			string? passwordSalt = null,
			List<string>? claims = null,
			bool? enabled = null,
			string? description = null);

		/// <summary>
		/// Delete a service account from the collection
		/// </summary>
		/// <param name="id">The service account</param>
		/// <returns>Async task</returns>
		Task DeleteAsync(ObjectId id);

		/// <summary>
		/// Set the given password and generate a new corresponding salt
		/// </summary>
		/// <param name="id">Account ID</param>
		/// <param name="password">New password</param>
		/// <returns></returns>
		Task SetPasswordAsync(ObjectId id, string password);
	}
}
