// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using Horde.Server.Issues;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Issues
{
	[TestClass]
	public class IssueTagTests
	{
		[TestMethod]
		public void IssueTag()
		{
			Assert.IsTrue(IssueTagService.ParseTags("#horde", "hello\n#horde 123 ").SequenceEqual(new[] { 123 }));
			Assert.IsTrue(IssueTagService.ParseTags("#horde", "hello\n#horde 123 456").SequenceEqual(new[] { 123, 456 }));
			Assert.IsTrue(IssueTagService.ParseTags("#horde", "hello\n #horde 123 ").SequenceEqual(new[] { 123 }));
			Assert.IsTrue(IssueTagService.ParseTags("#horde", "hello\n #horde 123 \n#horde 456").SequenceEqual(new[] { 123, 456 }));
			Assert.IsTrue(IssueTagService.ParseTags("#horde", "hello\n #horde 123 \n #ROBOMERGE-SOURCE foo").SequenceEqual(Array.Empty<int>()));
		}
	}
}
