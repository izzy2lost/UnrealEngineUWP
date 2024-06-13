// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Core.Tests
{
	[TestClass]
	public class JsonSerializerUtilsTests
	{
		private struct TestObject : IEquatable<TestObject>
		{
			public string _stringFieldPublic;
			public string StringPropertyPublic { get; set; }

			public TestObject(int testData)
			{
				_stringFieldPublic = testData.ToString();
				StringPropertyPublic = (testData + 1).ToString();
			}

			public bool Equals(TestObject other) => _stringFieldPublic == other._stringFieldPublic && StringPropertyPublic == other.StringPropertyPublic;

			public override bool Equals(object? obj) => obj is TestObject objTyped && Equals(objTyped);

			public override int GetHashCode() => HashCode.Combine(_stringFieldPublic, StringPropertyPublic);
		}

		private static DirectoryReference s_tempDirectory = null!;

		public JsonSerializerUtilsTests()
		{
			s_tempDirectory = CreateTempDir();
		}

		[TestCleanup]
		public void RemoveTempDir()
		{
			if (DirectoryReference.Exists(s_tempDirectory))
			{
				DirectoryReference.Delete(s_tempDirectory, true);
			}
		}

		[TestInitialize]
		public void InitializeTest()
		{
		}

		[TestMethod]
		public void Save()
		{
			FileReference path = FileReference.Combine(s_tempDirectory, "save.json");
			JsonSerializerUtils.Save(path, new TestObject(Int32.MaxValue));
			Assert.IsTrue(FileReference.Exists(path));
		}

		[TestMethod]
		public void SaveIfDifferent()
		{
			FileReference path = FileReference.Combine(s_tempDirectory, "saveIfDifferent.json");
			JsonSerializerUtils.Save(path, new TestObject(0));
			Assert.IsTrue(FileReference.Exists(path));
			FileInfo initial = path.ToFileInfo();
			JsonSerializerUtils.SaveIfDifferent(path, new TestObject(Int32.MaxValue));
			FileInfo same = path.ToFileInfo();
			Assert.IsTrue(initial.LastWriteTime.Equals(same.LastWriteTime));
		}

		[TestMethod]
		public void Load()
		{
			FileReference path = FileReference.Combine(s_tempDirectory, "load.json");
			TestObject obj = new TestObject(Int32.MaxValue);
			JsonSerializerUtils.Save(path, obj);
			Assert.IsTrue(FileReference.Exists(path));
			TestObject obj2 = JsonSerializerUtils.Load<TestObject>(path);
			Assert.IsTrue(obj.Equals(obj2));
		}

		private static DirectoryReference CreateTempDir()
		{
			string tempDir = Path.Join(Path.GetTempPath(), "epicgames-core-tests-" + Guid.NewGuid().ToString()[..8]);
			Directory.CreateDirectory(tempDir);
			return new DirectoryReference(tempDir);
		}
	}
}
