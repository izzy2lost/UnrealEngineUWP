// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// Reference to a directory node, including the target hash and length
	/// </summary>
	public class DirectoryNodeRef
	{
		/// <summary>
		/// Length of this directory tree
		/// </summary>
		public long Length { get; }

		/// <summary>
		/// Target node
		/// </summary>
		public HashedNodeRef<DirectoryNode> Target { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public DirectoryNodeRef(long length, HashedNodeRef<DirectoryNode> target)
		{
			Length = length;
			Target = target;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public DirectoryNodeRef(IBlobReader reader)
		{
			Target = new HashedNodeRef<DirectoryNode>(reader);
			Length = (long)reader.ReadUnsignedVarInt();
		}

		/// <summary>
		/// Serialize this directory entry to disk
		/// </summary>
		/// <param name="writer"></param>
		public virtual void Serialize(IBlobWriter writer)
		{
			writer.WriteHashedNodeRef(Target);
			writer.WriteUnsignedVarInt((ulong)Length);
		}
	}

	/// <summary>
	/// Extension methods for <see cref="DirectoryNodeRef"/>
	/// </summary>
	public static class DirectoryNodeRefExtensions
	{
		/// <summary>
		/// Serialize a directory node ref to a blob writer
		/// </summary>
		public static void WriteDirectoryNodeRef(this IBlobWriter writer, DirectoryNodeRef nodeRef)
		{
			nodeRef.Serialize(writer);
		}
	}
}
