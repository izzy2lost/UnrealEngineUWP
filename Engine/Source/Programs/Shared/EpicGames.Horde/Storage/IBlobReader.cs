// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Interface for reading nodes from storage
	/// </summary>
	public interface IBlobReader : IMemoryReader
	{
		/// <summary>
		/// Type to deserialize
		/// </summary>
		BlobType Type { get; }

		/// <summary>
		/// Version of the current node, as specified via <see cref="BlobType"/>
		/// </summary>
		int Version { get; }

		/// <summary>
		/// Locations of all referenced nodes. These handles do not have valid hashes.
		/// </summary>
		IReadOnlyList<IBlobHandle> References { get; }

		/// <summary>
		/// Gets the next serialized blob handle
		/// </summary>
		IBlobHandle<T> ReadBlobHandle<T>();
	}

	/// <summary>
	/// Reader for blob objects
	/// </summary>
	public sealed class BlobReader : MemoryReader, IBlobReader
	{
		/// <summary>
		/// Type to deserialize
		/// </summary>
		public BlobType Type => _blobData.Type;

		/// <summary>
		/// Version of the current node, as specified via <see cref="BlobType"/>
		/// </summary>
		public int Version => Type.Version;

		/// <summary>
		/// Total length of the data in this node
		/// </summary>
		public int Length => _blobData.Data.Length;

		/// <summary>
		/// Amount of data remaining to be read
		/// </summary>
		public int RemainingLength => RemainingMemory.Length;

		/// <summary>
		/// Raw data for this blob
		/// </summary>
		public ReadOnlyMemory<byte> Data => _blobData.Data;

		/// <summary>
		/// Locations of all referenced nodes.
		/// </summary>
		public IReadOnlyList<IBlobHandle> References => _blobData.Refs;

		readonly BlobData _blobData;
		int _refIdx;

		/// <summary>
		/// Constructor
		/// </summary>
		public BlobReader(BlobData blobData)
			: base(blobData.Data)
		{
			_blobData = blobData;
		}

		/// <summary>
		/// Gets the next serialized blob handle
		/// </summary>
		public IBlobHandle<T> ReadBlobHandle<T>()
		{
			IBlobHandle handle = References[_refIdx++];
			IoHash hash = this.ReadIoHash();
			return handle.ForType<T>(hash);
		}
	}
}
