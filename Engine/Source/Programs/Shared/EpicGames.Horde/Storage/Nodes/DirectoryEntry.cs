// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// Entry for a directory within a directory node
	/// </summary>
	[DebuggerDisplay("{Name}")]
	public record class DirectoryEntry(string Name, long Length, IBlobRef<DirectoryNode> Handle) : DirectoryNodeRef(Length, Handle)
	{
	}
}
