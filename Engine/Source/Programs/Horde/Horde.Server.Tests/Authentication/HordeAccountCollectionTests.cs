// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Server.Server;
using Horde.Server.Users;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Authentication
{
	[TestClass]
	public class HordeAccountCollectionTests : DatabaseIntegrationTest
	{
		private readonly IHordeAccountCollection _hordeAccounts;
		private readonly IHordeAccount _hordeAccount;
		
		public HordeAccountCollectionTests()
		{
			MongoService mongoService = GetMongoServiceSingleton();
			_hordeAccounts = new HordeAccountCollection(mongoService);
			_hordeAccount = _hordeAccounts.AddAsync("myName", "myLogin",
				claims: new List<IUserClaim> { new UserClaim("myClaim", "myValue")},
				description: "myDesc").Result;
		}

		[TestMethod]
		public async Task AddAsync()
		{
			IHordeAccount sa = await _hordeAccounts.AddAsync("myName", "myLogin",
				secretToken: "addToken",
				claims: new List<IUserClaim> { new UserClaim("myClaim", "myValue")},
				description: "myDesc");
			Assert.AreEqual("addToken", sa.SecretToken);
			Assert.AreEqual(1, sa.GetClaims().Count);
			Assert.AreEqual("myValue", sa.GetClaims()[0].Value);
			Assert.IsTrue(sa.Enabled);
			Assert.AreEqual("myDesc", sa.Description);
		}
		
		[TestMethod]
		public async Task GetAsync()
		{
			IHordeAccount sa = (await _hordeAccounts.GetAsync(_hordeAccount.Id))!;
			Assert.AreEqual(_hordeAccount, sa);
		}
		
		[TestMethod]
		public async Task GetBySecretTokenAsync()
		{
			IHordeAccount sa = (await _hordeAccounts.GetBySecretTokenAsync(_hordeAccount.SecretToken!))!;
			Assert.AreEqual(_hordeAccount, sa);
		}
		
		[TestMethod]
		public async Task GetByLoginAsync()
		{
			IHordeAccount sa = (await _hordeAccounts.GetByLoginAsync(_hordeAccount.Login))!;
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
			IHordeAccount sa = (await _hordeAccounts.GetAsync(_hordeAccount.Id))!;
			
			Assert.AreEqual("newName", sa.Name);
			Assert.AreEqual("newLogin", sa.Login);
			Assert.AreEqual("newToken", sa.SecretToken);
			Assert.IsTrue(PasswordHasher.ValidatePassword("password12345", PasswordHasher.SaltFromString(sa.PasswordSalt), PasswordHasher.HashFromString(sa.PasswordHash)));
			Assert.IsFalse(PasswordHasher.ValidatePassword("password123456", PasswordHasher.SaltFromString(sa.PasswordSalt), PasswordHasher.HashFromString(sa.PasswordHash)));
			Assert.AreEqual(2, sa.GetClaims().Count);
			Assert.AreEqual("newValue1", sa.GetClaims()[0].Value);
			Assert.AreEqual("newValue2", sa.GetClaims()[1].Value);
			Assert.AreEqual(false, sa.Enabled);
			Assert.AreEqual("newDesc", sa.Description);
		}
		
		[TestMethod]
		public async Task DeleteAsync()
		{
			await _hordeAccounts.DeleteAsync(_hordeAccount.Id);
			IHordeAccount? result = await _hordeAccounts.GetAsync(_hordeAccount.Id);
			Assert.IsNull(result);
		}
	}
}