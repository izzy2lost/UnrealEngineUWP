// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// Reference to a directory node, including the target hash and length
	/// </summary>
	/// <param name="Hash">Hash of the target node</param>
	/// <param name="Length">Sum total of all the file lengths in this directory tree</param>
	/// <param name="Handle">Handle to the target node</param>
	public record class DirectoryNodeRef(IoHash Hash, long Length, IBlobHandle Handle)
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public DirectoryNodeRef(long length, HashedNodeRef<DirectoryNode> target)
			: this(target.Hash, length, target.Handle)
		{
		}

		/// <summary>
		/// Get the target directory node
		/// </summary>
		public ValueTask<DirectoryNode> ExpandAsync(CancellationToken cancellationToken = default)
			=> Handle.ReadNodeAsync<DirectoryNode>(cancellationToken);
	}
}
