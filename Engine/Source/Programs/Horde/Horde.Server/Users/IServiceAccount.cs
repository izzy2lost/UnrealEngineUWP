// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using MongoDB.Bson;

namespace Horde.Server.Users
{
	/// <summary>
	/// A service account used for authenticating during server-to-server communication
	/// For example between Robomerge and Horde.
	/// </summary>
	public interface IServiceAccount
	{
		/// <summary>
		/// Unique internal ID for this Horde account
		/// </summary>
		public ObjectId Id { get; }
		
		/// <summary>
		/// Full name of the user
		/// </summary>
		public string Name { get; }

		/// <summary>
		/// A login ID or username
		/// </summary>
		public string Login { get; }

		/// <summary>
		/// Email associated with account
		/// </summary>
		public string? Email { get; }
		
		/// <summary>
		/// Secret token used for identifying API calls made as the account
		/// </summary>
		public string? SecretToken { get; }
		
		/// <summary>
		/// Hashed password
		/// </summary>
		public string? PasswordHash { get; }
		
		/// <summary>
		/// Salt for password hash (if PasswordHash is set)
		/// </summary>
		public string? PasswordSalt { get; }
		
		/// <summary>
		/// If the account is active
		/// </summary>
		public bool Enabled { get; }
		
		/// <summary>
		/// Description of the account (who is it for, is there an owner etc)
		/// </summary>
		public string Description { get; }
		
		/// <summary>
		/// Get list of claims
		/// </summary>
		/// <returns>List of claims</returns>
		public IReadOnlyList<IUserClaim> GetClaims();
	}
}
