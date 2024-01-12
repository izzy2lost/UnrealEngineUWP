// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// Reference to a directory node, including the target hash and length
	/// </summary>
	/// <param name="Length">Sum total of all the file lengths in this directory tree</param>
	/// <param name="Handle">Handle to the target node</param>
	public record class DirectoryNodeRef(long Length, IBlobHandle<DirectoryNode> Handle);
}
