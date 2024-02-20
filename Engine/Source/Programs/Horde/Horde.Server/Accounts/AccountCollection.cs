// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Accounts;
using Horde.Server.Acls;
using Horde.Server.Server;
using Horde.Server.Users;
using Horde.Server.Utilities;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Server.Accounts
{
	/// <summary>
	/// Password hasher
	/// </summary>
	public static class PasswordHasher
	{
		private const int SaltSize = 16; // 128 bit 
		private const int KeySize = 32; // 256 bit
		private const int Iterations = 100000; // Number of iterations

		/// <summary>
		/// Generate a new salt for use with password hash
		/// </summary>
		/// <returns>A salt</returns>
		public static byte[] GenerateSalt()
		{
			using RandomNumberGenerator rng = RandomNumberGenerator.Create();
			byte[] salt = new byte[SaltSize];
			rng.GetBytes(salt);
			return salt;
		}

		/// <summary>
		/// Create a hash for the given password
		/// </summary>
		/// <param name="password">Clear text password</param>
		/// <param name="salt">Salt to hash with</param>
		/// <returns></returns>
		public static byte[] HashPassword(string password, byte[] salt)
		{
			using Rfc2898DeriveBytes rfc2898DeriveBytes = new(password, salt, Iterations, HashAlgorithmName.SHA256);
			return rfc2898DeriveBytes.GetBytes(KeySize);
		}

		/// <summary>
		/// Validate a password
		/// </summary>
		/// <param name="password">Clear text password</param>
		/// <param name="salt">Salt to hash with</param>
		/// <param name="correctHash">Correct hash</param>
		/// <returns>True if password matches correct hash</returns>
		public static bool ValidatePassword(string password, byte[] salt, byte[] correctHash)
		{
			byte[] hash = HashPassword(password, salt);
			return hash.SequenceEqual(correctHash);
		}

		/// <summary>
		/// Convert a salt string to byte array
		/// </summary>
		/// <param name="saltString">Salt stored as a hex string</param>
		/// <returns>A byte array representation</returns>
		/// <exception cref="ArgumentException">If salt is invalid</exception>
		public static byte[] SaltFromString(string? saltString)
		{
			byte[] salt = Convert.FromHexString(saltString ?? "");
			if (salt.Length != 16)
			{
				throw new ArgumentException($"Invalid salt length: {salt.Length}");
			}
			return salt;
		}

		/// <summary>
		/// Convert a hash string to byte array
		/// </summary>
		/// <param name="hashString">Hash stored as a hex string</param>
		/// <returns>A byte array representation</returns>
		/// <exception cref="ArgumentException">If hash is invalid</exception>
		public static byte[] HashFromString(string? hashString)
		{
			byte[] hash = Convert.FromHexString(hashString ?? "");
			if (hash.Length != 32)
			{
				throw new ArgumentException($"Invalid hash length: {hash.Length}");
			}
			return hash;
		}
	}

	/// <summary>
	/// Collection of service account documents
	/// </summary>
	public class AccountCollection : IAccountCollection
	{
		record class ClaimDocument(string Type, string Value) : IUserClaim
		{
			public ClaimDocument(IUserClaim claim) : this(claim.Type, claim.Value)
			{ }

			public ClaimDocument(AclClaimConfig claim) : this(claim.Type, claim.Value)
			{ }
		}

		/// <summary>
		/// Concrete implementation of IHordeAccount
		/// </summary>
		private class AccountDocument : IAccount
		{
			/// <inheritdoc/>
			[BsonRequired, BsonId]
			public AccountId Id { get; set; }

			/// <inheritdoc/>
			public string Name { get; set; } = "";

			/// <inheritdoc/>
			public string Login { get; set; } = "";

			/// <inheritdoc/>
			public string? Email { get; set; }

			/// <inheritdoc/>
			public string? SecretToken { get; set; }

			/// <inheritdoc/>
			public string? PasswordHash { get; set; }

			/// <inheritdoc/>
			public string? PasswordSalt { get; set; }

			[BsonElement("Claims2")]
			public List<ClaimDocument> Claims { get; set; } = new List<ClaimDocument>();

			/// <inheritdoc/>
			public bool Enabled { get; set; }

			/// <inheritdoc/>
			public string Description { get; set; } = "";

			IReadOnlyList<IUserClaim> IAccount.Claims => Claims;

			[BsonConstructor]
			private AccountDocument()
			{
			}

			public AccountDocument(AccountId id, string name, string login)
			{
				Id = id;
				Name = name;
				Login = login;
			}

			protected bool Equals(AccountDocument other)
			{
				bool areClaimsEqual = !Claims.Except(other.Claims).Any();

				return Id.Equals(other.Id) && SecretToken == other.SecretToken && areClaimsEqual && Enabled == other.Enabled && Description == other.Description;
			}

			public override bool Equals(object? obj)
			{
				if (obj is null)
				{
					return false;
				}
				if (ReferenceEquals(this, obj))
				{
					return true;
				}
				if (obj.GetType() != GetType())
				{
					return false;
				}
				return Equals((AccountDocument)obj);
			}

			public override int GetHashCode()
			{
				return HashCode.Combine(Id, SecretToken, Claims, Enabled, Description);
			}
		}

		private static AccountId s_defaultAdminAccountId = AccountId.Parse("65d4f282ff286703e0609ccd");

		private bool _hasCreatedAdminAccount = false;
		private readonly IMongoCollection<AccountDocument> _accounts;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service</param>
		public AccountCollection(MongoService mongoService)
		{
			_accounts = mongoService.GetCollection<AccountDocument>("ServiceAccounts", keys => keys.Ascending(x => x.SecretToken));
		}

		/// <inheritdoc/>
		public async Task<IAccount> AddAsync(
			string name,
			string login,
			IReadOnlyList<IUserClaim>? claims,
			string? description,
			string? email,
			string? secretToken,
			string? password,
			bool? enabled,
			CancellationToken cancellationToken = default)
		{
			AccountDocument account = new(new AccountId(BinaryIdUtils.CreateNew()), name, login)
			{
				Email = email,
				SecretToken = secretToken,
				Description = description ?? "",
				Enabled = enabled ?? true
			};

			if (claims != null)
			{
				account.Claims = claims.ConvertAll(x => new ClaimDocument(x));
			}

			if (password != null)
			{
				(string passwordSalt, string passwordHash) = CreateSaltAndHashPassword(password);
				account.PasswordSalt = passwordSalt;
				account.PasswordHash = passwordHash;
			}

			await _accounts.InsertOneAsync(account, (InsertOneOptions?)null, cancellationToken);
			return account;
		}

		async ValueTask CreateAdminAccountAsync(CancellationToken cancellationToken)
		{
			if (!_hasCreatedAdminAccount)
			{
				UpdateDefinition<AccountDocument> update = Builders<AccountDocument>.Update
					.SetOnInsert(x => x.Name, "Admin")
					.SetOnInsert(x => x.Description, "Default administrator account")
					.SetOnInsert(x => x.Claims, new List<ClaimDocument> { new ClaimDocument(HordeClaims.AdminClaim) })
					.SetOnInsert(x => x.Enabled, true);

				await _accounts.UpdateOneAsync(x => x.Id == s_defaultAdminAccountId, update, new UpdateOptions { IsUpsert = true }, cancellationToken);
				_hasCreatedAdminAccount = true;
			}
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<IAccount>> FindAsync(int? index = null, int? count = null, CancellationToken cancellationToken = default)
		{
			await CreateAdminAccountAsync(cancellationToken);
			return await _accounts.Find(FilterDefinition<AccountDocument>.Empty).Range(index, count).ToListAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IAccount?> GetAsync(AccountId id, CancellationToken cancellationToken = default)
		{
			await CreateAdminAccountAsync(cancellationToken);
			return await _accounts.Find(x => x.Id == id).FirstOrDefaultAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IAccount?> GetBySecretTokenAsync(string secretToken, CancellationToken cancellationToken = default)
		{
			return await _accounts.Find(x => x.SecretToken == secretToken).FirstOrDefaultAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IAccount?> GetByLoginAsync(string login, CancellationToken cancellationToken = default)
		{
			return await _accounts.Find(x => x.Login == login).FirstOrDefaultAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public Task UpdateAsync(AccountId id,
			string? name,
			string? login,
			IReadOnlyList<IUserClaim>? claims,
			string? description,
			string? email,
			string? secretToken,
			string? password,
			bool? enabled,
			CancellationToken cancellationToken = default)
		{
			UpdateDefinitionBuilder<AccountDocument> update = Builders<AccountDocument>.Update;
			List<UpdateDefinition<AccountDocument>> updates = new List<UpdateDefinition<AccountDocument>>();

			if (name != null)
			{
				updates.Add(update.Set(x => x.Name, name));
			}
			if (login != null)
			{
				updates.Add(update.Set(x => x.Login, login));
			}
			if (email != null)
			{
				updates.Add(update.Set(x => x.Email, email));
			}
			if (secretToken != null)
			{
				updates.Add(update.Set(x => x.SecretToken, secretToken));
			}
			if (password != null)
			{
				(string salt, string hash) = CreateSaltAndHashPassword(password);
				updates.Add(update.Set(x => x.PasswordSalt, salt).Set(x => x.PasswordHash, hash));
			}
			if (claims != null)
			{
				updates.Add(update.Set(x => x.Claims, claims.ConvertAll(x => new ClaimDocument(x))));
			}
			if (enabled != null)
			{
				updates.Add(update.Set(x => x.Enabled, enabled));
			}
			if (description != null)
			{
				updates.Add(update.Set(x => x.Description, description));
			}

			return _accounts.FindOneAndUpdateAsync(x => x.Id == id, update.Combine(updates), cancellationToken: cancellationToken);
		}

		/// <inheritdoc/>
		public Task DeleteAsync(AccountId id, CancellationToken cancellationToken = default)
		{
			if (id == s_defaultAdminAccountId)
			{
				throw new InvalidOperationException("The default administrator account cannot be deleted.");
			}
			return _accounts.DeleteOneAsync(x => x.Id == id, cancellationToken);
		}

		static (string Salt, string Hash) CreateSaltAndHashPassword(string password)
		{
			byte[] salt = PasswordHasher.GenerateSalt();
			byte[] hashedPassword = PasswordHasher.HashPassword(password, salt);
			return (Convert.ToHexString(salt), Convert.ToHexString(hashedPassword));
		}
	}
}
