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
	class RefNode : Node
	{
		public IoHash RootHash { get; }
		public List<(IoHash Hash, BlobHandle Handle)> References { get; }

		public RefNode(IoHash rootHash)
		{
			RootHash = rootHash;
			References = new List<(IoHash, BlobHandle)>();
		}

		public RefNode(NodeReader reader)
		{
			RootHash = reader.ReadIoHash();
			References = reader.ReadList(x => (reader.ReadIoHash(), reader.ReadNodeHandle()));
		}

		/// <inheritdoc/>
		public override void Serialize(NodeWriter writer)
		{
			writer.WriteIoHash(RootHash);
			writer.WriteList(References, x => WriteReference(writer, x.Hash, x.Handle));
		}

		static void WriteReference(NodeWriter writer, IoHash hash, BlobHandle handle)
		{
			writer.WriteIoHash(hash);
			writer.WriteNodeHandle(handle);
		}
	}
}
