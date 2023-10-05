// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Data for an individual node. Must be disposed after use.
	/// </summary>
	public class BlobData : IDisposable
	{
		/// <summary>
		/// Type of the blob
		/// </summary>
		public BlobType Type { get; }

		/// <summary>
		/// Hash of the node data
		/// </summary>
		public IoHash Hash { get; }

		/// <summary>
		/// Raw data for the blob. Lifetime of this data is tied to the lifetime of the <see cref="BlobData"/> object; consumers must not retain references to it.
		/// </summary>
		public ReadOnlyMemory<byte> Data { get; }

		/// <summary>
		/// Handles to referenced blobs
		/// </summary>
		public IReadOnlyList<BlobHandle> Refs { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public BlobData(BlobType type, IoHash hash, ReadOnlyMemory<byte> data, IReadOnlyList<BlobHandle> refs)
		{
			Type = type;
			Hash = hash;
			Data = data;
			Refs = refs;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		/// <summary>
		/// Overridable dispose method
		/// </summary>
		/// <param name="disposing">True if derived instances should dispose managed resources. False when called from a finalizer.</param>
		protected virtual void Dispose(bool disposing)
		{
		}
	}
}
