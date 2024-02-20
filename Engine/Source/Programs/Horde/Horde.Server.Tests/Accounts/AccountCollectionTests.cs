// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Server.Accounts;
using Horde.Server.Server;
using Horde.Server.Users;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Accounts
{
	[TestClass]
	public class HordeAccountCollectionTests : DatabaseIntegrationTest
	{
		private readonly IAccountCollection _hordeAccounts;
		private readonly IAccount _hordeAccount;
		
		public HordeAccountCollectionTests()
		{
			MongoService mongoService = GetMongoServiceSingleton();
			_hordeAccounts = new AccountCollection(mongoService);
			_hordeAccount = _hordeAccounts.AddAsync("myName", "myLogin",
				claims: new List<IUserClaim> { new UserClaim("myClaim", "myValue")},
				description: "myDesc").Result;
		}

		[TestMethod]
		public async Task AddAsync()
		{
			IAccount sa = await _hordeAccounts.AddAsync("myName", "myLogin",
				secretToken: "addToken",
				claims: new List<IUserClaim> { new UserClaim("myClaim", "myValue")},
				description: "myDesc");
			Assert.AreEqual("addToken", sa.SecretToken);
			Assert.AreEqual(1, sa.Claims.Count);
			Assert.AreEqual("myValue", sa.Claims[0].Value);
			Assert.IsTrue(sa.Enabled);
			Assert.AreEqual("myDesc", sa.Description);
		}
		
		[TestMethod]
		public async Task GetAsync()
		{
			IAccount sa = (await _hordeAccounts.GetAsync(_hordeAccount.Id))!;
			Assert.AreEqual(_hordeAccount, sa);
		}
		
		[TestMethod]
		public async Task GetBySecretTokenAsync()
		{
			IAccount sa = (await _hordeAccounts.GetBySecretTokenAsync(_hordeAccount.SecretToken!))!;
			Assert.AreEqual(_hordeAccount, sa);
		}
		
		[TestMethod]
		public async Task GetByLoginAsync()
		{
			IAccount sa = (await _hordeAccounts.GetByLoginAsync(_hordeAccount.Login))!;
			Assert.AreEqual(_hordeAccount.Id, sa.Id);
			Assert.AreEqual(_hordeAccount.Login, sa.Login);
			
			Assert.IsNull(await _hordeAccounts.GetByLoginAsync("does-not-exist"));
		}
		
		[TestMethod]
		public async Task UpdateAsync()
		{
			List<UserClaim> newClaims = new () {new UserClaim("newClaim1","newValue1"), new UserClaim("newClaim2","newValue2")};
			await _hordeAccounts.UpdateAsync(_hordeAccount.Id,
				name: "newName",
				login: "newLogin",
				claims: newClaims,
				email: "foo@bar.com",
				secretToken: "newToken",
				password: "password12345",
				enabled: false,
				description: "newDesc");
			IAccount sa = (await _hordeAccounts.GetAsync(_hordeAccount.Id))!;
			
			Assert.AreEqual("newName", sa.Name);
			Assert.AreEqual("newLogin", sa.Login);
			Assert.AreEqual("newToken", sa.SecretToken);
			Assert.IsTrue(PasswordHasher.ValidatePassword("password12345", PasswordHasher.SaltFromString(sa.PasswordSalt), PasswordHasher.HashFromString(sa.PasswordHash)));
			Assert.IsFalse(PasswordHasher.ValidatePassword("password123456", PasswordHasher.SaltFromString(sa.PasswordSalt), PasswordHasher.HashFromString(sa.PasswordHash)));
			Assert.AreEqual(2, sa.Claims.Count);
			Assert.AreEqual("newValue1", sa.Claims[0].Value);
			Assert.AreEqual("newValue2", sa.Claims[1].Value);
			Assert.AreEqual(false, sa.Enabled);
			Assert.AreEqual("newDesc", sa.Description);
		}
		
		[TestMethod]
		public async Task DeleteAsync()
		{
			await _hordeAccounts.DeleteAsync(_hordeAccount.Id);
			IAccount? result = await _hordeAccounts.GetAsync(_hordeAccount.Id);
			Assert.IsNull(result);
		}
	}
}