// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using EpicGames.Core;
using EpicGames.Horde.Storage;

namespace Horde.Server.Ddc
{
	/// <summary>
	/// A node containing ref data
	/// </summary>
	[BlobConverter(typeof(DdcRefNodeConverter))]
	public class DdcRefNode
	{
		/// <summary>
		/// Static accessor for the blob type guid
		/// </summary>
		public static Guid BlobTypeGuid { get; } = new Guid("{0C7E5F25-4B55-454B-63F4-4A9B74D00651}");
		
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
	}

	class DdcRefNodeConverter : BlobConverter<DdcRefNode>
	{
		static BlobType s_blobType = new BlobType(DdcRefNode.BlobTypeGuid, 1);

		public override DdcRefNode Read(IBlobReader reader, BlobSerializerOptions options)
		{
			IoHash rootHash = reader.ReadIoHash();
			List<IBlobHandle<object>> references = reader.ReadList(x => reader.ReadBlobHandle<object>());

			DdcRefNode refNode = new DdcRefNode(rootHash);
			refNode.References.AddRange(references.Select(x => (x.Hash, (IBlobHandle)x)));
			return refNode;
		}

		public override BlobType Write(IBlobWriter writer, DdcRefNode value, BlobSerializerOptions options)
		{
			writer.WriteIoHash(value.RootHash);
			writer.WriteList(value.References, x => writer.WriteBlobHandle(x.Handle.ForType<object>(x.Hash)));
			return s_blobType;
		}
	}
}
