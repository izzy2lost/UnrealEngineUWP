// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// Reference to a directory node, including the target hash and length
	/// </summary>
	public class DirectoryNodeRef : HashedNodeRef<DirectoryNode>
	{
		/// <summary>
		/// Length of this directory tree
		/// </summary>
		public long Length { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public DirectoryNodeRef(long length, HashedNodeRef<DirectoryNode> nodeRef)
			: base(nodeRef)
		{
			Length = length;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public DirectoryNodeRef(IBlobReader reader)
			: base(reader)
		{
			Length = (long)reader.ReadUnsignedVarInt();
		}

		/// <summary>
		/// Serialize this directory entry to disk
		/// </summary>
		/// <param name="writer"></param>
		public override void Serialize(IBlobWriter writer)
		{
			base.Serialize(writer);

			writer.WriteUnsignedVarInt((ulong)Length);
		}
	}
}
