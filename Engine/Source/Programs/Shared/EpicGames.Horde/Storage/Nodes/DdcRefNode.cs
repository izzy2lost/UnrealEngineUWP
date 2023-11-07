// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Core;
using EpicGames.Horde.Storage;

namespace Horde.Server.Ddc
{
	/// <summary>
	/// A node containing ref data
	/// </summary>
	[NodeType("{0C7E5F25-454B-4B55-9B4A-F4635106D074}", 1)]
	public class DdcRefNode : Node
	{
		/// <summary>
		/// Hash of the root node
		/// </summary>
		public IoHash RootHash { get; }

		/// <summary>
		/// References to attachments. We embed this in the ref node to ensure any aliased blobs have a hard reference from the root.
		/// </summary>
		public List<(IoHash Hash, IBlobHandle Handle)> References { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public DdcRefNode(IoHash rootHash)
		{
			RootHash = rootHash;
			References = new List<(IoHash, IBlobHandle)>();
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public DdcRefNode(INodeReader reader)
		{
			RootHash = reader.ReadIoHash();
			References = reader.ReadList(x => (reader.ReadIoHash(), reader.ReadBlobReference()));
		}

		/// <inheritdoc/>
		public override void Serialize(INodeWriter writer)
		{
			writer.WriteIoHash(RootHash);
			writer.WriteList(References, x => WriteReference(writer, x.Hash, x.Handle));
		}

		static void WriteReference(INodeWriter writer, IoHash hash, IBlobHandle handle)
		{
			writer.WriteIoHash(hash);
			writer.WriteBlobReference(handle);
		}
	}
}
