// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Perforce.Managed;
using Horde.Agent.Utility;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Agent.Tests;

using MWO = ManagedWorkspaceOptions;

[TestClass]
public class WorkspaceInfoTest
{
	[TestMethod]
	public void ShouldUseHaveTable()
	{
		Assert.IsTrue(WorkspaceInfo.ShouldUseHaveTable(null));
		Assert.IsTrue(WorkspaceInfo.ShouldUseHaveTable(""));
		Assert.IsTrue(WorkspaceInfo.ShouldUseHaveTable("name=managedWorkspace"));
		Assert.IsTrue(WorkspaceInfo.ShouldUseHaveTable("name=managedWorkspace$#@!@#"));
		Assert.IsTrue(WorkspaceInfo.ShouldUseHaveTable("name=managedWorkspace$#@!@#"));
		Assert.IsTrue(WorkspaceInfo.ShouldUseHaveTable("name=managedWorkspace&useHaveTable=true"));
		
		Assert.IsFalse(WorkspaceInfo.ShouldUseHaveTable("name=managedWorkspace&useHaveTable=false"));
		Assert.IsFalse(WorkspaceInfo.ShouldUseHaveTable("name=ManagedWorkspace&useHaveTable=FalsE"));
	}

	[TestMethod]
	public void GetManagedWorkspaceOptions()
	{
		ManagedWorkspaceOptions defaultOptions = new ();
		Assert.AreEqual(defaultOptions, WorkspaceInfo.GetMwOptions(null));
		Assert.AreEqual(defaultOptions, WorkspaceInfo.GetMwOptions(""));
		Assert.AreEqual(defaultOptions, WorkspaceInfo.GetMwOptions("name=somethingElse&numParallelSyncThreads=111"));
		Assert.AreEqual(new MWO { NumParallelSyncThreads = 111 }, WorkspaceInfo.GetMwOptions("name=managedWorkspace&numParallelSyncThreads=111"));
		Assert.AreEqual(new MWO { MaxFileConcurrency = 222 }, WorkspaceInfo.GetMwOptions("name=managedWorkspace&maxFileConcurrency=222"));
		Assert.AreEqual(new MWO { MinScratchSpace = 333 }, WorkspaceInfo.GetMwOptions("name=managedWorkspace&minScratchSpace=333"));
		Assert.AreEqual(new MWO { UseHaveTable = false }, WorkspaceInfo.GetMwOptions("name=managedWorkspace&useHaveTable=false"));
		Assert.AreEqual(new MWO { UseHaveTable = false }, WorkspaceInfo.GetMwOptions("name=managedWorkspace&useHaveTable=FalSe"));
		Assert.AreEqual(new MWO { UseHaveTable = true }, WorkspaceInfo.GetMwOptions("name=managedWorkspace&useHaveTable=true"));
		Assert.AreEqual(new MWO { UseHaveTable = true }, WorkspaceInfo.GetMwOptions("name=managedWorkspace&useHaveTable=TrUE"));
	}
}
