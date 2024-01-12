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
		/// Writes a reference to another blob. The blob's hash is serialized to the output stream.
		/// </summary>
		/// <param name="handle">Referenced blob</param>
		void WriteBlobHandle<T>(IBlobHandle<T> handle);
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
		/// Memory that has been written
		/// </summary>
		public ReadOnlyMemory<byte> WrittenMemory => _memory.Slice(0, _length);

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
		/// Computes the hash of the written data
		/// </summary>
		public IoHash ComputeHash() => IoHash.Compute(_memory.Span.Slice(0, _length));

		/// <summary>
		/// Writes a handle to another node
		/// </summary>
		public void WriteBlobHandle<T>(IBlobHandle<T> target)
		{
			this.WriteIoHash(target.Hash);
			_refs.Add(target);
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
