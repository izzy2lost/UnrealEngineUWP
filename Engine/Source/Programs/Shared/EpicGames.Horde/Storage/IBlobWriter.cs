// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Interface for a writer of node objects
	/// </summary>
	public interface IBlobWriter : IMemoryWriter, IAsyncDisposable
	{
		/// <summary>
		/// Accessor for the memory written to the current blob
		/// </summary>
		ReadOnlyMemory<byte> WrittenMemory { get; }

		/// <summary>
		/// Adds an alias to the blob currently being written
		/// </summary>
		/// <param name="name">Name of the alias</param>
		/// <param name="rank">Rank to use when finding blobs by alias</param>
		/// <param name="data">Inline data to store with the alias</param>
		void AddAlias(string name, int rank, ReadOnlyMemory<byte> data = default);

		/// <summary>
		/// Flush any pending nodes to storage
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task FlushAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Create another writer instance, allowing multiple threads to write in parallel.
		/// </summary>
		/// <returns>New writer instance</returns>
		IBlobWriter Fork();

		/// <summary>
		/// Finish writing a blob that has been written into the output buffer.
		/// </summary>
		/// <param name="type">Type of the node that was written</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the written node</returns>
		ValueTask<IBlobHandle> CompleteAsync(BlobType type, CancellationToken cancellationToken = default);

		/// <summary>
		/// Finish writing a blob that has been written into the output buffer.
		/// </summary>
		/// <param name="type">Type of the node that was written</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the written node</returns>
		ValueTask<IBlobHandle<T>> CompleteAsync<T>(BlobType type, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes a reference to another blob. The blob's hash is serialized to the output stream.
		/// </summary>
		/// <param name="handle">Referenced blob</param>
		void WriteBlobHandle<T>(IBlobHandle<T> handle);
	}

	/// <summary>
	/// Information about an alias to be added alongside a blob
	/// </summary>
	/// <param name="Name">Name of the alias</param>
	/// <param name="Rank">Rank of the alias</param>
	/// <param name="Data">Inline data to be stored for the alias</param>
	public record class AliasInfo(string Name, int Rank, ReadOnlyMemory<byte> Data);

	/// <summary>
	/// Base class for <see cref="IBlobWriter"/> implementations.
	/// </summary>
	public abstract class BlobWriter : IBlobWriter
	{
		Memory<byte> _memory;
		readonly List<AliasInfo> _aliases = new List<AliasInfo>();
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
				_memory = GetOutputBuffer(_length, Math.Max(_memory.Length * 2, newLength));
			}
			return _memory.Slice(_length);
		}

		/// <inheritdoc/>
		public void Advance(int length) => _length += length;

		/// <summary>
		/// Request a new buffer to write to
		/// </summary>
		/// <param name="usedSize">Size of data written to the current buffer</param>
		/// <param name="desiredSize">Desired size for the buffer</param>
		/// <returns>New buffer</returns>
		public abstract Memory<byte> GetOutputBuffer(int usedSize, int desiredSize);

		/// <summary>
		/// Write the current blob to storage
		/// </summary>
		/// <param name="type">Type of the blob</param>
		/// <param name="size">Size of the blob to write</param>
		/// <param name="references">References to other blobs</param>
		/// <param name="aliases">Aliases for the new blob</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>New buffer</returns>
		public abstract ValueTask<IBlobHandle> WriteBlobAsync(BlobType type, int size, IReadOnlyList<IBlobHandle> references, IReadOnlyList<AliasInfo> aliases, CancellationToken cancellationToken);

		/// <inheritdoc/>
		public void AddAlias(string name, int rank, ReadOnlyMemory<byte> data)
			=> _aliases.Add(new AliasInfo(name, rank, data));

		/// <inheritdoc/>
		public abstract Task FlushAsync(CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract IBlobWriter Fork();

		/// <inheritdoc/>
		public async ValueTask<IBlobHandle> CompleteAsync(BlobType type, CancellationToken cancellationToken = default)
		{
			IBlobHandle handle = await WriteBlobAsync(type, _length, _refs, _aliases, cancellationToken);

			_memory = default;
			_length = 0;
			_refs.Clear();
			_aliases.Clear();

			return handle;
		}

		/// <inheritdoc/>
		public async ValueTask<IBlobHandle<T>> CompleteAsync<T>(BlobType type, CancellationToken cancellationToken = default)
		{
			IoHash hash = IoHash.Compute(_memory.Span.Slice(0, _length));
			IBlobHandle handle = await CompleteAsync(type, cancellationToken);
			return handle.ForType<T>(hash);
		}

		/// <inheritdoc/>
		public abstract ValueTask DisposeAsync();
	}
}
