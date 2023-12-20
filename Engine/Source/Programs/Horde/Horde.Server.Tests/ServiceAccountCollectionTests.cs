// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Server.Server;
using Horde.Server.Users;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests
{
	[TestClass]
	public class ServiceAccountCollectionTests : DatabaseIntegrationTest
	{
		private readonly IServiceAccountCollection _serviceAccounts;
		private readonly IServiceAccount _serviceAccount;
		
		public ServiceAccountCollectionTests()
		{
			MongoService mongoService = GetMongoServiceSingleton();
			_serviceAccounts = new ServiceAccountCollection(mongoService);
			_serviceAccount = _serviceAccounts.AddAsync("myName", "myLogin",
				claims: new List<IUserClaim> { new UserClaim("myClaim", "myValue")},
				description: "myDesc").Result;
		}

		[TestMethod]
		public async Task AddAsync()
		{
			IServiceAccount sa = await _serviceAccounts.AddAsync("myName", "myLogin",
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
			IServiceAccount sa = (await _serviceAccounts.GetAsync(_serviceAccount.Id))!;
			Assert.AreEqual(_serviceAccount, sa);
		}
		
		[TestMethod]
		public async Task GetBySecretTokenAsync()
		{
			IServiceAccount sa = (await _serviceAccounts.GetBySecretTokenAsync(_serviceAccount.SecretToken!))!;
			Assert.AreEqual(_serviceAccount, sa);
		}
		
		[TestMethod]
		public async Task GetByLoginAsync()
		{
			IServiceAccount sa = (await _serviceAccounts.GetByLogin(_serviceAccount.Login))!;
			Assert.AreEqual(_serviceAccount.Id, sa.Id);
			Assert.AreEqual(_serviceAccount.Login, sa.Login);
			
			Assert.IsNull(await _serviceAccounts.GetByLogin("does-not-exist"));
		}
		
		[TestMethod]
		public async Task UpdateAsync()
		{
			List<string> newClaims = new () {"newClaim1###newValue1", "newClaim2###newValue2"};
			await _serviceAccounts.UpdateAsync(_serviceAccount.Id,
				name: "newName",
				login: "newLogin",
				email: "foo@bar.com",
				secretToken: "newToken",
				passwordHash: "newHash",
				passwordSalt: "newSalt",
				claims: newClaims,
				enabled: false,
				description: "newDesc");
			IServiceAccount sa = (await _serviceAccounts.GetAsync(_serviceAccount.Id))!;
			
			Assert.AreEqual("newName", sa.Name);
			Assert.AreEqual("newLogin", sa.Login);
			Assert.AreEqual("newToken", sa.SecretToken);
			Assert.AreEqual("newHash", sa.PasswordHash);
			Assert.AreEqual("newSalt", sa.PasswordSalt);
			Assert.AreEqual(2, sa.GetClaims().Count);
			Assert.AreEqual("newValue1", sa.GetClaims()[0].Value);
			Assert.AreEqual("newValue2", sa.GetClaims()[1].Value);
			Assert.AreEqual(false, sa.Enabled);
			Assert.AreEqual("newDesc", sa.Description);
		}
		
		[TestMethod]
		public async Task DeleteAsync()
		{
			await _serviceAccounts.DeleteAsync(_serviceAccount.Id);
			IServiceAccount? result = await _serviceAccounts.GetAsync(_serviceAccount.Id);
			Assert.IsNull(result);
		}
	}
}