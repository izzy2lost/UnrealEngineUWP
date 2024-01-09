// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// Entry for a directory within a directory node
	/// </summary>
	[DebuggerDisplay("{Name}")]
	public record class DirectoryEntry(string Name, IoHash Hash, long Length, IBlobHandle Handle) : DirectoryNodeRef(Hash, Length, Handle)
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public DirectoryEntry(string name, long length, HashedNodeRef<DirectoryNode> target)
			: this(name, target.Hash, length, target.Handle)
		{
		}
	}
}
