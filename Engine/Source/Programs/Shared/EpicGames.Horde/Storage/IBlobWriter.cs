// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Interface for a writer of node objects
	/// </summary>
	public interface IBlobWriter : IMemoryWriter
	{
		/// <summary>
		/// Adds a reference to another blob. This reference is stored out of band, and will not result in any bytes written to the output.
		/// </summary>
		/// <param name="reference">Referenced blob</param>
		void WriteBlobReference(IBlobHandle reference);
	}

	/// <summary>
	/// Writer for node objects, which tracks references to other nodes
	/// </summary>
	sealed class BlobWriter : IBlobWriter
	{
		readonly IStorageWriter _treeWriter;

		Memory<byte> _memory;
		readonly List<IBlobHandle> _refs = new List<IBlobHandle>();
		int _length;

		/// <summary>
		/// List of serialized references
		/// </summary>
		public IReadOnlyList<IBlobHandle> References => _refs;

		/// <inheritdoc/>
		public int Length => _length;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="treeWriter"></param>
		public BlobWriter(IStorageWriter treeWriter)
		{
			_treeWriter = treeWriter;
			_memory = treeWriter.GetOutputBuffer(0, 2 * 1024);
		}

		/// <summary>
		/// Adds a reference to another blob. This reference is stored out of band, and will not result in any bytes written to the output.
		/// </summary>
		/// <param name="reference">Referenced blob</param>
		public void WriteBlobReference(IBlobHandle reference) => _refs.Add(reference);

		/// <summary>
		/// Computes the hash of the written data
		/// </summary>
		public IoHash ComputeHash() => IoHash.Compute(_memory.Span.Slice(0, _length));

		/// <summary>
		/// Writes a handle to another node
		/// </summary>
		public void WriteHashedBlobHandle(IoHash hash, IBlobHandle target)
		{
			this.WriteIoHash(hash);
			WriteBlobReference(target);
		}

		/// <inheritdoc/>
		public Span<byte> GetSpan(int sizeHint = 0) => GetMemory(sizeHint).Span;

		/// <inheritdoc/>
		public Memory<byte> GetMemory(int sizeHint = 0)
		{
			int newLength = _length + Math.Max(sizeHint, 1);
			if (newLength > _memory.Length)
			{
				newLength = _length + Math.Max(sizeHint, 1024);
				_memory = _treeWriter.GetOutputBuffer(_length, Math.Max(_memory.Length * 2, newLength));
			}
			return _memory.Slice(_length);
		}

		/// <inheritdoc/>
		public void Advance(int length) => _length += length;
	}
}
