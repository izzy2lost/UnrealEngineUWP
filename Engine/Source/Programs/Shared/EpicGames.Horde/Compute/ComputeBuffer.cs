// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// In-process buffer used to store compute messages
	/// </summary>
	public abstract class ComputeBuffer : IDisposable
	{
		/// <summary>
		/// Maximum number of chunks in a buffer
		/// </summary>
		public const int MaxChunks = 16;

		/// <summary>
		/// Maximum number of readers
		/// </summary>
		public const int MaxReaders = 16;

		internal ComputeBufferDetail _detail;

		/// <summary>
		/// Reader for this buffer
		/// </summary>
		public ComputeBufferReader Reader { get; }

		/// <summary>
		/// Writer for this buffer
		/// </summary>
		public ComputeBufferWriter Writer { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="detail">Resources shared between instances of the buffer</param>
		internal ComputeBuffer(ComputeBufferDetail detail)
		{
			_detail = detail;
			Reader = new ComputeBufferReader(detail, 0);
			Writer = new ComputeBufferWriter(detail);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		/// <summary>
		/// Overriable dispose method
		/// </summary>
		protected virtual void Dispose(bool disposing)
		{
			if (_detail != null)
			{
				_detail.Release();
				_detail = null!;
			}
		}

		/// <summary>
		/// Creates a new reference to the underlying buffer. The underlying resources will only be destroyed once all instances are disposed of.
		/// </summary>
		public abstract ComputeBuffer AddRef();
	}

	/// <summary>
	/// Read interface for a compute buffer
	/// </summary>
	public sealed class ComputeBufferReader : IDisposable
	{
		ComputeBufferDetail _detail;
		readonly int _readerIdx;

		internal ComputeBufferDetail Detail => _detail;

		internal ComputeBufferReader(ComputeBufferDetail detail, int readerIdx)
		{
			_detail = detail;
			_readerIdx = readerIdx;
		}

		/// <summary>
		/// Create a new reader instance using the same underlying buffer
		/// </summary>
		public ComputeBufferReader AddRef()
		{
			_detail.AddRef();
			return new ComputeBufferReader(_detail, _readerIdx);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			if (_detail != null)
			{
				_detail.Release();
				_detail = null!;
			}
		}

		/// <summary>
		/// Whether this buffer is complete (no more data will be added)
		/// </summary>
		public bool IsComplete => _detail.IsReadingComplete(_readerIdx);

		/// <summary>
		/// Updates the read position
		/// </summary>
		/// <param name="length">Size of data that was read</param>
		public void AdvanceReadPosition(int length) => _detail.AdvanceReadPosition(_readerIdx, length);

		/// <summary>
		/// Gets the next data to read
		/// </summary>
		/// <returns>Memory to read from</returns>
		public ReadOnlyMemory<byte> GetReadBuffer() => _detail.GetReadBuffer(_readerIdx);

		/// <summary>
		/// Read from a buffer into another buffer
		/// </summary>
		/// <param name="buffer">Memory to receive the read data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Number of bytes read</returns>
		public async ValueTask<int> ReadAsync(Memory<byte> buffer, CancellationToken cancellationToken = default)
		{
			for (; ; )
			{
				ReadOnlyMemory<byte> readMemory = GetReadBuffer();
				if (IsComplete || readMemory.Length > 0)
				{
					int length = Math.Min(readMemory.Length, buffer.Length);
					readMemory.Slice(0, length).CopyTo(buffer);
					AdvanceReadPosition(length);
					return length;
				}
				await WaitToReadAsync(1, cancellationToken);
			}
		}

		/// <summary>
		/// Wait for data to be available, or for the buffer to be marked as complete
		/// </summary>
		/// <param name="minLength">Minimum amount of data to read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if new data is available, false if the buffer is complete</returns>
		public ValueTask<bool> WaitToReadAsync(int minLength, CancellationToken cancellationToken = default) => _detail.WaitToReadAsync(_readerIdx, minLength, cancellationToken);
	}

	/// <summary>
	/// Buffer that can receive data from a remote machine.
	/// </summary>
	public sealed class ComputeBufferWriter : IDisposable
	{
		ComputeBufferDetail _detail;

		internal ComputeBufferDetail Detail => _detail;

		internal ComputeBufferWriter(ComputeBufferDetail detail)
		{
			_detail = detail;
		}

		/// <summary>
		/// Create a new writer instance using the same underlying buffer
		/// </summary>
		public ComputeBufferWriter AddRef()
		{
			_detail.AddRef();
			return new ComputeBufferWriter(_detail);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			if (_detail != null)
			{
				_detail.Release();
				_detail = null!;
			}
		}

		/// <inheritdoc/>
		public void AdvanceWritePosition(int size) => _detail.AdvanceWritePosition(size);

		/// <summary>
		/// Gets memory to write to
		/// </summary>
		/// <returns>Memory to be written to</returns>
		public Memory<byte> GetWriteBuffer() => _detail.GetWriteBuffer();

		/// <summary>
		/// Mark the output to this buffer as complete
		/// </summary>
		/// <returns>Whether the writer was marked as complete. False if the writer has already been marked as complete.</returns>
		public bool MarkComplete() => _detail.MarkComplete();

		/// <summary>
		/// Writes data into a buffer from a memory block
		/// </summary>
		/// <param name="buffer">The data to write</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async ValueTask WriteAsync(ReadOnlyMemory<byte> buffer, CancellationToken cancellationToken = default)
		{
			while (buffer.Length > 0)
			{
				Memory<byte> writeMemory = GetWriteBuffer();
				if (writeMemory.Length >= buffer.Length)
				{
					buffer.CopyTo(writeMemory);
					AdvanceWritePosition(buffer.Length);
					break;
				}
				await WaitToWriteAsync(writeMemory.Length, cancellationToken);
			}
		}

		/// <summary>
		/// Gets memory to write to
		/// </summary>
		/// <param name="minLength">Minimum size of the desired write buffer</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Memory to be written to</returns>
		public ValueTask WaitToWriteAsync(int minLength, CancellationToken cancellationToken = default) => _detail.WaitToWriteAsync(minLength, cancellationToken);
	}

	/// <summary>
	/// State shared between buffer instances
	/// </summary>
	internal abstract class ComputeBufferDetail : IDisposable
	{
		internal const int HeaderSize = (2 + ComputeBuffer.MaxChunks + ComputeBuffer.MaxReaders) * sizeof(ulong);

		/// <summary>
		/// Tracked state of the buffer
		/// </summary>
		protected internal readonly unsafe struct HeaderPtr
		{
			readonly ulong* _data;

			public HeaderPtr(ulong* data) => _data = data;

			public HeaderPtr(ulong* data, int numReaders, int numChunks, int chunkLength)
			{
				_data = data;

				data[0] = ((ulong)(uint)numChunks << 32) | (uint)numReaders;
				data[1] = (uint)chunkLength;

				GetChunkStatePtr(0).StartWriting(numReaders);
			}

			public int NumReaders => (int)_data[0];
			public int NumChunks => (int)(_data[0] >> 32);
			public int ChunkLength => (int)_data[1];
			public int WriteChunkIdx
			{
				get => (int)(_data[1] >> 32);
				set => _data[1] = ((ulong)value << 32) | (uint)ChunkLength;
			}

			public ChunkStatePtr GetChunkStatePtr(int chunkIdx) => new ChunkStatePtr(_data + 2 + chunkIdx);

			public ReaderStatePtr GetReaderStatePtr(int readerIdx) => new ReaderStatePtr(_data + 2 + ComputeBuffer.MaxChunks + readerIdx);
		}

		/// <summary>
		/// Write state for a chunk
		/// </summary>
		protected internal enum WriteState
		{
			/// <summary>
			/// Writer has moved to the next chunk
			/// </summary>
			MovedToNext = 0,

			/// <summary>
			/// Chunk is still being appended to
			/// </summary>
			Writing = 2,

			/// <summary>
			/// This chunk marks the end of the stream
			/// </summary>
			Complete = 3,
		}

		/// <summary>
		/// Stores the state of a chunk in a 64-bit value, which can be updated atomically
		/// </summary>
		protected internal record struct ChunkState(ulong Value)
		{
			// Written length of this chunk
			public int Length => (int)(Value & 0x7fffffff);

			// Set of flags which are set for each reader that still has to read from a chunk
			public int ReaderFlags => (int)((Value >> 31) & 0x7fffffff);

			// State of the writer
			public WriteState WriteState => (WriteState)(Value >> 62);

			// Constructor
			public ChunkState(WriteState writerState, int readerFlags, int length) : this(((ulong)writerState << 62) | ((ulong)readerFlags << 31) | (uint)length) { }

			// Test whether a particular reader is still referencing the chunk
			public bool HasReaderFlag(int readerIdx) => (Value & (1UL << (readerIdx + 31))) != 0;

			/// <inheritdoc/>
			public override string ToString() => $"{WriteState}, Length: {Length}, Readers: {ReaderFlags}";
		}

		/// <summary>
		/// Wraps a pointer to the state of a chunk
		/// </summary>
		protected internal readonly unsafe struct ChunkStatePtr
		{
			readonly ulong* _data;

			public ChunkStatePtr(ulong* data) => _data = data;

			// Current value of the chunk state
			public ChunkState Value => new ChunkState(Interlocked.CompareExchange(ref *_data, 0, 0));

			// Written length of this chunk
			public int Length => Value.Length;

			// Set of flags which are set for each reader that still has to read from a chunk
			public int ReaderFlags => Value.ReaderFlags;

			// State of the writer
			public WriteState WriteState => Value.WriteState;

			// Append data to the chunk
			public void Append(int length) => Interlocked.Add(ref *_data, (uint)length);

			// Mark the chunk as being written to
			public void StartWriting(int numReaders) => Interlocked.Exchange(ref *_data, new ChunkState(WriteState.Writing, (1 << numReaders) - 1, 0).Value);

			// Move to the next chunk
			public void MarkComplete() => Interlocked.Or(ref *_data, (ulong)WriteState.Complete << 62);

			// Clear the reader flag
			public void FinishReading(int readerIdx) => Interlocked.And(ref *_data, ~(1UL << (readerIdx + 31)));

			// Move to the next chunk
			public void FinishWriting() => Interlocked.And(ref *_data, ~((ulong)WriteState.Writing << 62));

			/// <inheritdoc/>
			public override string ToString() => Value.ToString();
		}

		/// <summary>
		/// Encodes the state of a reader in a 64-bit value
		/// </summary>
		protected internal record struct ReaderState(ulong Value)
		{
			public ReaderState(int chunkIdx, int offset) : this(((ulong)chunkIdx << 32) | (uint)offset) { }

			public int ChunkIdx => (int)(Value >> 32);
			public int Offset => (int)Value;

			public override string ToString() => $"Chunk {ChunkIdx}, Offset {Offset}";
		}

		/// <summary>
		/// Pointer to a reader state value
		/// </summary>
		protected internal readonly unsafe struct ReaderStatePtr
		{
			readonly ulong* _data;

			public ReaderStatePtr(ulong* data) => _data = data;

			public ReaderState Value
			{
				get => new ReaderState(Interlocked.CompareExchange(ref *_data, 0, 0));
				set => Interlocked.Exchange(ref *_data, value.Value);
			}

			public int ChunkIdx => Value.ChunkIdx;
			public int Offset => Value.Offset;

			public void Advance(int length) => Interlocked.Add(ref *_data, (ulong)length);

			public override string ToString() => Value.ToString();
		}

		readonly HeaderPtr _headerPtr;
		readonly Memory<byte>[] _chunks;
		int _refCount = 1;

		/// <summary>
		/// Constructor
		/// </summary>
		protected ComputeBufferDetail(HeaderPtr headerPtr, Memory<byte>[] chunks)
		{
			_headerPtr = headerPtr;
			_chunks = chunks;
		}

		/// <summary>
		/// Increment the reference count on this object
		/// </summary>
		public void AddRef()
		{
			Interlocked.Increment(ref _refCount);
		}

		/// <summary>
		/// Decrement the reference count on this object, and dispose of it once it reaches zero
		/// </summary>
		public void Release()
		{
			if (Interlocked.Decrement(ref _refCount) == 0)
			{
				Dispose();
			}
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
		protected abstract void Dispose(bool disposing);

		/// <summary>
		/// Signals a read event
		/// </summary>
		public abstract void SetReadEvent(int readerIdx);

		/// <summary>
		/// Signals read events for every reader
		/// </summary>
		public void SetAllReadEvents()
		{
			for (int readerIdx = 0; readerIdx < _headerPtr.NumReaders; readerIdx++)
			{
				SetReadEvent(readerIdx);
			}
		}

		/// <summary>
		/// Resets a read event
		/// </summary>
		public abstract void ResetReadEvent(int readerIdx);

		/// <summary>
		/// Waits for a read event to be signalled
		/// </summary>
		public abstract Task WaitForReadEvent(int readerIdx, CancellationToken cancellationToken);

		/// <summary>
		/// Signals the write event
		/// </summary>
		public abstract void SetWriteEvent();

		/// <summary>
		/// Resets the write event
		/// </summary>
		public abstract void ResetWriteEvent();

		/// <summary>
		/// Waits for the write event to be signalled
		/// </summary>
		public abstract Task WaitForWriteEvent(CancellationToken cancellationToken);

#pragma warning disable IDE0051 // Remove unused private members
		// For debugging purposes only
		ChunkState[] ChunkStates => Enumerable.Range(0, _headerPtr.NumChunks).Select(x => _headerPtr.GetChunkStatePtr(x).Value).ToArray();
		ReaderState[] ReaderStates => Enumerable.Range(0, _headerPtr.NumReaders).Select(x => _headerPtr.GetReaderStatePtr(x).Value).ToArray();
		int WriteChunkIdx => _headerPtr.WriteChunkIdx;
#pragma warning restore IDE0051 // Remove unused private members

		public bool IsReadingComplete(int readerIdx)
		{
			ReaderState readerState = _headerPtr.GetReaderStatePtr(readerIdx).Value;
			ChunkState chunkState = _headerPtr.GetChunkStatePtr(readerState.ChunkIdx).Value;
			return chunkState.WriteState == WriteState.Complete && readerState.Offset == chunkState.Length;
		}

		/// <inheritdoc/>
		public void AdvanceReadPosition(int readerIdx, int length)
		{
			ReaderStatePtr readerStatePtr = _headerPtr.GetReaderStatePtr(readerIdx);
			readerStatePtr.Advance(length);
		}

		/// <inheritdoc/>
		public ReadOnlyMemory<byte> GetReadBuffer(int readerIdx)
		{
			ReaderStatePtr readerStatePtr = _headerPtr.GetReaderStatePtr(readerIdx);
			ReaderState readerState = readerStatePtr.Value;

			ChunkStatePtr chunkStatePtr = _headerPtr.GetChunkStatePtr(readerState.ChunkIdx);
			ChunkState chunkState = chunkStatePtr.Value;

			if (chunkState.HasReaderFlag(readerIdx))
			{
				return _chunks[readerState.ChunkIdx].Slice(readerState.Offset, chunkState.Length - readerState.Offset);
			}
			else
			{
				return ReadOnlyMemory<byte>.Empty;
			}
		}

		/// <inheritdoc/>
		public async ValueTask<bool> WaitToReadAsync(int readerIdx, int minLength, CancellationToken cancellationToken = default)
		{
			for (; ; )
			{
				ReaderStatePtr readerStatePtr = _headerPtr.GetReaderStatePtr(readerIdx);
				ReaderState readerState = readerStatePtr.Value;

				ChunkStatePtr chunkStatePtr = _headerPtr.GetChunkStatePtr(readerState.ChunkIdx);
				ChunkState chunkState = chunkStatePtr.Value;

				if (!chunkState.HasReaderFlag(readerIdx))
				{
					// Wait until the current chunk is readable
					ResetReadEvent(readerIdx);
					if (!chunkState.HasReaderFlag(readerIdx))
					{
						await WaitForReadEvent(readerIdx, cancellationToken);
					}
				}
				else if (readerState.Offset + minLength <= chunkState.Length)
				{
					// We have enough data in the chunk to be able to read a message
					return true;
				}
				else if (chunkState.WriteState == WriteState.Writing)
				{
					// Wait until there is more data in the chunk
					ResetReadEvent(readerIdx);
					if (_headerPtr.GetChunkStatePtr(readerState.ChunkIdx).Value == chunkState)
					{
						await WaitForReadEvent(readerIdx, cancellationToken);
					}
				}
				else if (readerState.Offset < chunkState.Length || chunkState.WriteState == WriteState.Complete)
				{
					// Cannot read the requested amount of data from this chunk.
					return false;
				}
				else if (chunkState.WriteState == WriteState.MovedToNext)
				{
					// Move to the next chunk
					chunkStatePtr.FinishReading(readerIdx);
					SetWriteEvent();

					int chunkIdx = readerStatePtr.ChunkIdx + 1;
					if (chunkIdx == _chunks.Length)
					{
						chunkIdx = 0;
					}

					readerStatePtr.Value = new ReaderState(chunkIdx, 0);
				}
				else
				{
					throw new NotImplementedException($"Invalid write state for buffer: {chunkState.WriteState}");
				}
			}
		}

		/// <inheritdoc/>
		public void AdvanceWritePosition(int size)
		{
			if (size > 0)
			{
				ChunkStatePtr chunkStatePtr = _headerPtr.GetChunkStatePtr(_headerPtr.WriteChunkIdx);
				ChunkState chunkState = chunkStatePtr.Value;

				Debug.Assert(chunkState.WriteState == WriteState.Writing);
				chunkStatePtr.Append(size);

				SetAllReadEvents();
			}
		}

		/// <inheritdoc/>
		public Memory<byte> GetWriteBuffer()
		{
			ChunkState state = _headerPtr.GetChunkStatePtr(_headerPtr.WriteChunkIdx).Value;
			if (state.WriteState == WriteState.Writing)
			{
				return _chunks[_headerPtr.WriteChunkIdx].Slice(state.Length);
			}
			else
			{
				return Memory<byte>.Empty;
			}
		}

		/// <inheritdoc/>
		public bool MarkComplete()
		{
			ChunkStatePtr chunkStatePtr = _headerPtr.GetChunkStatePtr(_headerPtr.WriteChunkIdx);
			if (chunkStatePtr.WriteState != WriteState.Complete)
			{
				chunkStatePtr.MarkComplete();
				SetAllReadEvents();
				return true;
			}
			return false;
		}

		/// <inheritdoc/>
		public async ValueTask WaitToWriteAsync(int minSize, CancellationToken cancellationToken = default)
		{
			if (minSize > _headerPtr.ChunkLength)
			{
				throw new ArgumentException("Requested read size is larger than chunk size.", nameof(minSize));
			}

			for (; ; )
			{
				ChunkStatePtr chunkStatePtr = _headerPtr.GetChunkStatePtr(_headerPtr.WriteChunkIdx);

				ChunkState chunkState = chunkStatePtr.Value;
				if (chunkState.WriteState == WriteState.Writing)
				{
					int length = chunkState.Length;
					if (length + minSize <= _headerPtr.ChunkLength)
					{
						return;
					}
					chunkStatePtr.FinishWriting();
				}

				int nextChunkIdx = _headerPtr.WriteChunkIdx + 1;
				if (nextChunkIdx == _chunks.Length)
				{
					nextChunkIdx = 0;
				}

				ChunkStatePtr nextChunkStatePtr = _headerPtr.GetChunkStatePtr(nextChunkIdx);
				while (nextChunkStatePtr.ReaderFlags != 0)
				{
					await WaitForWriteEvent(cancellationToken);
					ResetWriteEvent();
				}

				_headerPtr.WriteChunkIdx = nextChunkIdx;
				nextChunkStatePtr.StartWriting(_headerPtr.NumReaders);

				SetAllReadEvents();
			}
		}
	}
}
