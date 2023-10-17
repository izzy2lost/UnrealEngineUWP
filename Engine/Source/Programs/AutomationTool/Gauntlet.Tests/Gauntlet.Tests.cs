global using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Gauntlet.Tests
{
	[TestClass]
	public class TestFramework
	{
		[TestMethod]
		public void TestAssert()
		{
			Assert.IsTrue(1 + 1 == 2);
		}
	}
}