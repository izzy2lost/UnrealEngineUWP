// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Accounts;
using Horde.Server.Server;
using Horde.Server.Utilities;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Server.Users
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
	public class HordeAccountCollection : IHordeAccountCollection
	{
		/// <summary>
		/// Concrete implementation of IHordeAccount
		/// </summary>
		private class HordeAccountDocument : IHordeAccount
		{
			public const string ClaimSeparator = "###";

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

			[BsonRequired]
			public List<string> Claims { get; set; } = new List<string>();

			/// <inheritdoc/>
			public bool Enabled { get; set; }

			/// <inheritdoc/>
			public string Description { get; set; } = "";

			[BsonConstructor]
			private HordeAccountDocument()
			{
			}

			public HordeAccountDocument(AccountId id, string name, string login)
			{
				Id = id;
				Name = name;
				Login = login;
			}

			/// <inheritdoc/>
			public IReadOnlyList<IUserClaim> GetClaims()
			{
				return Claims.Select(x =>
				{
					string[] split = x.Split(ClaimSeparator);
					return new UserClaim(split[0], split[1]);
				}).ToList();
			}

			protected bool Equals(HordeAccountDocument other)
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
				return Equals((HordeAccountDocument)obj);
			}

			public override int GetHashCode()
			{
				return HashCode.Combine(Id, SecretToken, Claims, Enabled, Description);
			}
		}

		/// <summary>
		/// Collection of session documents
		/// </summary>
		private readonly IMongoCollection<HordeAccountDocument> _serviceAccounts;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service</param>
		public HordeAccountCollection(MongoService mongoService)
		{
			_serviceAccounts = mongoService.GetCollection<HordeAccountDocument>("ServiceAccounts", keys => keys.Ascending(x => x.SecretToken));
		}

		static List<string> CreateClaims(IReadOnlyList<IUserClaim> claims)
		{
			return claims.Select(x => $"{x.Type}{HordeAccountDocument.ClaimSeparator}{x.Value}").ToList();
		}

		/// <inheritdoc/>
		public async Task<IHordeAccount> AddAsync(
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
			List<string> stringClaims = (claims == null) ? new List<string>() : CreateClaims(claims);

			HordeAccountDocument account = new(new AccountId(BinaryIdUtils.CreateNew()), name, login)
			{
				Email = email,
				SecretToken = secretToken,
				Description = description ?? "",
				Claims = stringClaims,
				Enabled = enabled ?? true
			};

			if (password != null)
			{
				(string passwordSalt, string passwordHash) = CreateSaltAndHashPassword(password);
				account.PasswordSalt = passwordSalt;
				account.PasswordHash = passwordHash;
			}

			await _serviceAccounts.InsertOneAsync(account, (InsertOneOptions?)null, cancellationToken);
			return account;
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<IHordeAccount>> FindAsync(int? index = null, int? count = null, CancellationToken cancellationToken = default)
		{
			return await _serviceAccounts.Find(FilterDefinition<HordeAccountDocument>.Empty).Range(index, count).ToListAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IHordeAccount?> GetAsync(AccountId id, CancellationToken cancellationToken = default)
		{
			return await _serviceAccounts.Find(x => x.Id == id).FirstOrDefaultAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IHordeAccount?> GetBySecretTokenAsync(string secretToken, CancellationToken cancellationToken = default)
		{
			return await _serviceAccounts.Find(x => x.SecretToken == secretToken).FirstOrDefaultAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IHordeAccount?> GetByLoginAsync(string login, CancellationToken cancellationToken = default)
		{
			return await _serviceAccounts.Find(x => x.Login == login).FirstOrDefaultAsync(cancellationToken);
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
			UpdateDefinitionBuilder<HordeAccountDocument> update = Builders<HordeAccountDocument>.Update;
			List<UpdateDefinition<HordeAccountDocument>> updates = new List<UpdateDefinition<HordeAccountDocument>>();

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
				updates.Add(update.Set(x => x.Claims, CreateClaims(claims)));
			}
			if (enabled != null)
			{
				updates.Add(update.Set(x => x.Enabled, enabled));
			}
			if (description != null)
			{
				updates.Add(update.Set(x => x.Description, description));
			}

			return _serviceAccounts.FindOneAndUpdateAsync(x => x.Id == id, update.Combine(updates), cancellationToken: cancellationToken);
		}

		/// <inheritdoc/>
		public Task DeleteAsync(AccountId sessionId, CancellationToken cancellationToken = default)
		{
			return _serviceAccounts.DeleteOneAsync(x => x.Id == sessionId, cancellationToken);
		}

		static (string Salt, string Hash) CreateSaltAndHashPassword(string password)
		{
			byte[] salt = PasswordHasher.GenerateSalt();
			byte[] hashedPassword = PasswordHasher.HashPassword(password, salt);
			return (Convert.ToHexString(salt), Convert.ToHexString(hashedPassword));
		}
	}
}
