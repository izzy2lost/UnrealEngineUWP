// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// Entry for a directory within a directory node
	/// </summary>
	public class DirectoryEntry : DirectoryNodeRef
	{
		/// <summary>
		/// Name of this directory
		/// </summary>
		public string Name { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public DirectoryEntry(string name, long length, HashedNodeRef<DirectoryNode> nodeRef)
			: base(length, nodeRef)
		{
			Name = name;
		}

		/// <summary>
		/// Deserializing constructor
		/// </summary>
		/// <param name="reader"></param>
		public DirectoryEntry(IBlobReader reader)
			: base(reader)
		{
			Name = reader.ReadString();
		}

		/// <summary>
		/// Serialize this directory entry to disk
		/// </summary>
		/// <param name="writer"></param>
		public override void Serialize(IBlobWriter writer)
		{
			base.Serialize(writer);

			writer.WriteString(Name);
		}

		/// <inheritdoc/>
		public override string ToString() => Name;
	}
}
