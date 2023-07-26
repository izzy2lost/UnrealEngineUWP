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
		private ComputeBufferWriter _writer;

		/// <summary>
		/// Writer for this buffer
		/// </summary>
		public ComputeBufferWriter Writer => _writer;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="detail">Resources shared between instances of the buffer</param>
		internal ComputeBuffer(ComputeBufferDetail detail)
		{
			_detail = detail;
			_writer = new ComputeBufferWriter(detail);
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
		protected virtual void Dispose(bool disposing)
		{
			if (_detail != null)
			{
				_detail.ReleaseWriter();
				_detail = null!;
			}
		}

		/// <summary>
		/// Creates a new reader for this buffer
		/// </summary>
		/// <returns></returns>
		public ComputeBufferReader CreateReader()
		{
			int readerIdx = _detail.AllocateReader();
			_detail.AddRef();
			return new ComputeBufferReader(_detail, new ComputeBufferDetail.ReaderState(readerIdx));
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
		ComputeBufferDetail _buffer;
		readonly ComputeBufferDetail.ReaderState _readerState;

		internal ComputeBufferDetail Detail => _buffer;

		internal ComputeBufferReader(ComputeBufferDetail buffer, ComputeBufferDetail.ReaderState readerState)
		{
			_buffer = buffer;
			_readerState = readerState;
		}

		/// <summary>
		/// Create a new reader instance using the same underlying buffer
		/// </summary>
		public ComputeBufferReader AddRef()
		{
			_buffer.AddRef();
			return new ComputeBufferReader(_buffer, _readerState);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			if (_buffer != null)
			{
				_buffer.Release();
				_buffer = null!;
			}
		}

		/// <summary>
		/// Detaches this reader from the underlying buffer
		/// </summary>
		public void Detach() => _buffer.DetachReader(_readerState);

		/// <summary>
		/// Whether this buffer is complete (no more data will be added)
		/// </summary>
		public bool IsComplete => _buffer.IsComplete(_readerState);

		/// <summary>
		/// Updates the read position
		/// </summary>
		/// <param name="length">Size of data that was read</param>
		public void AdvanceReadPosition(int length) => _readerState.Offset += length;

		/// <summary>
		/// Gets the next data to read
		/// </summary>
		/// <returns>Memory to read from</returns>
		public ReadOnlyMemory<byte> GetReadBuffer() => _buffer.GetReadBuffer(_readerState);

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
		public ValueTask<bool> WaitToReadAsync(int minLength, CancellationToken cancellationToken = default) => _buffer.WaitToReadAsync(_readerState, minLength, cancellationToken);
	}

	/// <summary>
	/// Buffer that can receive data from a remote machine.
	/// </summary>
	public sealed class ComputeBufferWriter : IDisposable
	{
		ComputeBufferDetail _detail;

		internal ComputeBufferDetail Detail => _detail;

		internal ComputeBufferWriter(ComputeBufferDetail detail) => _detail = detail;

		/// <summary>
		/// Create a new writer instance using the same underlying buffer
		/// </summary>
		public ComputeBufferWriter AddRef()
		{
			_detail.AddWriterRef();
			return new ComputeBufferWriter(_detail);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			if (_detail != null)
			{
				_detail.ReleaseWriter();
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
		internal const int HeaderSize = (3 + ComputeBuffer.MaxChunks) * sizeof(ulong);

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
			public ChunkState Value
			{
				get => new ChunkState(Interlocked.CompareExchange(ref *_data, 0, 0));
				set => Interlocked.Exchange(ref *_data, value.Value);
			}

			// Attempt to update the chunk state
			public bool TryUpdate(ChunkState prevState, ChunkState nextState) => Interlocked.CompareExchange(ref *_data, nextState.Value, prevState.Value) == prevState.Value;

			// Append data to the chunk
			public void Append(int length) => Interlocked.Add(ref *_data, (ulong)length);

			// Move to the next chunk
			public void MarkComplete() => Interlocked.Or(ref *_data, new ChunkState(WriteState.Complete, 0, 0).Value);

			// Start reading the chunk with the given reader
			public void StartReading(int readerIdx) => Interlocked.Or(ref *_data, new ChunkState(0, 1 << readerIdx, 0).Value);

			// Clear the reader flag
			public void FinishReading(int readerIdx) => Interlocked.And(ref *_data, ~new ChunkState(0, 1 << readerIdx, 0).Value);

			// Move to the next chunk
			public void FinishWriting() => Interlocked.And(ref *_data, ~new ChunkState(WriteState.Writing, 0, 0).Value);

			/// <inheritdoc/>
			public override string ToString() => Value.ToString();
		}

		/// <summary>
		/// State of a reader
		/// </summary>
		internal class ReaderState
		{
			public int ReaderIdx { get; }

			public int Offset { get; set; }
			public int ChunkIdx { get; set; }
			public bool Detached { get; set; }

			public ReaderState(int readerIdx) => ReaderIdx = readerIdx;
		}

		/// <summary>
		/// State of the writer
		/// </summary>
		public record struct WriterState(ulong Value)
		{
			public WriterState(int chunkIdx, int readerFlags, bool hasWrapped)
				: this((ulong)(uint)chunkIdx | ((ulong)(uint)readerFlags << 32) | (hasWrapped ? (1UL << 63) : 0))
			{ }

			public int ReaderFlags => (int)(Value >> 32) & 0xffff;
			public int ChunkIdx => (int)(Value & 0x7fffffff);
			public bool HasWrapped => (Value & (1UL << 63)) != 0;
		}

		/// <summary>
		/// Wraps a pointer to the state of a writer
		/// </summary>
		protected internal readonly unsafe struct WriterStatePtr
		{
			readonly ulong* _data;

			public WriterStatePtr(ulong* data) => _data = data;

			// Current value of the chunk state
			public WriterState Value
			{
				get => new WriterState(Interlocked.CompareExchange(ref *_data, 0, 0));
				set => Interlocked.Exchange(ref *_data, value.Value);
			}

			// Compare and swap
			public bool TryUpdate(WriterState prevState, WriterState nextState) => Interlocked.CompareExchange(ref *_data, nextState.Value, prevState.Value) == prevState.Value;
		}

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

				GetChunkStatePtr(0).Value = new ChunkState(WriteState.Writing, 0, 0);
			}

			public int NumReaders => (int)_data[0];
			public int NumChunks => (int)(_data[0] >> 32);
			public int ChunkLength => (int)_data[1];
			public uint AllocatedReaders => (uint)(_data[1] >> 32);

			public WriterStatePtr Writer => new WriterStatePtr(_data + 2);

			public ChunkStatePtr GetChunkStatePtr(int chunkIdx) => new ChunkStatePtr(_data + 3 + chunkIdx);

			public int AllocateReader()
			{
				for (int readerIdx = 0; readerIdx < NumReaders; readerIdx++)
				{
					ulong allocatedReaders = _data[1];
					ulong readerFlag = 1UL << (32 + readerIdx);

					if((allocatedReaders & readerFlag) == 0 && Interlocked.CompareExchange(ref _data[1], allocatedReaders | readerFlag, allocatedReaders) == allocatedReaders)
					{
						for (; ; )
						{
							WriterState state = Writer.Value;
							if (state.HasWrapped)
							{
								throw new InvalidOperationException("Cannot create a new reader after writer has wrapped back to the first chunk");
							}

							if (Writer.TryUpdate(state, new WriterState(state.ChunkIdx, state.ReaderFlags | (1 << readerIdx), state.HasWrapped)))
							{
								for (int WriteChunkIdx = 0; WriteChunkIdx <= state.ChunkIdx; WriteChunkIdx++)
								{
									GetChunkStatePtr(WriteChunkIdx).StartReading(readerIdx);
								}
								return readerIdx;
							}
						}
					}
				}
				throw new InvalidOperationException("Unable to allocate reader; all available readers are in use.");
			}

			void ReleaseReader(int readerIdx)
			{
				for (int Idx = 0; Idx < NumChunks; Idx++)
				{
					GetChunkStatePtr(Idx).FinishReading(readerIdx);
				}
				Interlocked.And(ref _data[1], ~(1UL << (readerIdx + 32)));
			}
		}

		readonly HeaderPtr _headerPtr;
		readonly Memory<byte>[] _chunks;
		int _refCount = 1;
		int _writerRefCount = 1;

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

		/// <summary>
		/// Increments the writer ref count
		/// </summary>
		internal void AddWriterRef()
		{
			AddRef();
			Interlocked.Increment(ref _writerRefCount);
		}

		/// <summary>
		/// Decrement the reference count on this object, and dispose of it once it reaches zero
		/// </summary>
		public void ReleaseWriter()
		{
			if (Interlocked.Decrement(ref _writerRefCount) == 0)
			{
				MarkComplete();
			}
			Release();
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
#pragma warning restore IDE0051 // Remove unused private members

		/// <summary>
		/// Allocate a new reader
		/// </summary>
		public int AllocateReader() => _headerPtr.AllocateReader();

		/// <inheritdoc/>
		public bool IsComplete(ReaderState readerState)
		{
			ChunkState chunkState = _headerPtr.GetChunkStatePtr(readerState.ChunkIdx).Value;
			return chunkState.WriteState == WriteState.Complete && readerState.Offset == chunkState.Length;
		}

		/// <inheritdoc/>
		public void DetachReader(ReaderState readerState)
		{
			readerState.Detached = true;
			SetReadEvent(readerState.ReaderIdx);
		}

		/// <inheritdoc/>
		public ReadOnlyMemory<byte> GetReadBuffer(ReaderState readerState)
		{
			ChunkStatePtr chunkStatePtr = _headerPtr.GetChunkStatePtr(readerState.ChunkIdx);
			ChunkState chunkState = chunkStatePtr.Value;

			if (chunkState.HasReaderFlag(readerState.ReaderIdx))
			{
				return _chunks[readerState.ChunkIdx].Slice(readerState.Offset, chunkState.Length - readerState.Offset);
			}
			else
			{
				return ReadOnlyMemory<byte>.Empty;
			}
		}

		/// <inheritdoc/>
		public async ValueTask<bool> WaitToReadAsync(ReaderState readerState, int minLength, CancellationToken cancellationToken = default)
		{
			int readerIdx = readerState.ReaderIdx;
			for (; ; )
			{
				if (readerState.Detached)
				{
					return false;
				}

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

					if (++readerState.ChunkIdx == _chunks.Length)
					{
						readerState.ChunkIdx = 0;
					}

					readerState.Offset = 0;
				}
				else
				{
					throw new NotImplementedException($"Invalid write state for buffer: {chunkState.WriteState}");
				}
			}
		}

		/// <inheritdoc/>
		public bool MarkComplete()
		{
			WriterState writerState = _headerPtr.Writer.Value;

			ChunkStatePtr chunkStatePtr = _headerPtr.GetChunkStatePtr(writerState.ChunkIdx);
			if (chunkStatePtr.Value.WriteState != WriteState.Complete)
			{
				chunkStatePtr.MarkComplete();
				SetAllReadEvents();
				return true;
			}

			return false;
		}

		/// <inheritdoc/>
		public void AdvanceWritePosition(int size)
		{
			if (size > 0)
			{
				WriterState writerState = _headerPtr.Writer.Value;

				ChunkStatePtr chunkStatePtr = _headerPtr.GetChunkStatePtr(writerState.ChunkIdx);
				ChunkState chunkState = chunkStatePtr.Value;

				Debug.Assert(chunkState.WriteState == WriteState.Writing);
				chunkStatePtr.Append(size);

				SetAllReadEvents();
			}
		}

		/// <inheritdoc/>
		public Memory<byte> GetWriteBuffer()
		{
			WriterState writerState = _headerPtr.Writer.Value;

			ChunkState chunkState = _headerPtr.GetChunkStatePtr(writerState.ChunkIdx).Value;
			if (chunkState.WriteState == WriteState.Writing)
			{
				return _chunks[writerState.ChunkIdx].Slice(chunkState.Length);
			}
			else
			{
				return Memory<byte>.Empty;
			}
		}

		/// <inheritdoc/>
		public async ValueTask WaitToWriteAsync(int minSize, CancellationToken cancellationToken = default)
		{
			if (minSize > _headerPtr.ChunkLength)
			{
				throw new ArgumentException("Requested read size is larger than chunk size.", nameof(minSize));
			}

			// Get the current chunk we're writing to
			WriterState writerState = _headerPtr.Writer.Value;
			int writeChunkIdx = writerState.ChunkIdx;

			ChunkStatePtr writeChunkStatePtr = _headerPtr.GetChunkStatePtr(writeChunkIdx);

			// Check if we can append to this chunk
			ChunkState chunkState = writeChunkStatePtr.Value;
			if (chunkState.WriteState == WriteState.Writing)
			{
				int length = chunkState.Length;
				if (length + minSize <= _headerPtr.ChunkLength)
				{
					return;
				}

				writeChunkStatePtr.FinishWriting();
				SetAllReadEvents();
			}

			if (chunkState.WriteState == WriteState.Complete)
			{
				return;
			}

			// Otherwise get the next chunk to write to
			int nextWriteChunkIdx = writeChunkIdx + 1;
			if (nextWriteChunkIdx == _chunks.Length)
			{
				nextWriteChunkIdx = 0;
			}

			// Wait until all readers have finished with the chunk, and we can update the writer to match
			ChunkStatePtr nextWriteChunkStatePtr = _headerPtr.GetChunkStatePtr(nextWriteChunkIdx);
			for (; ; )
			{
				ChunkState nextWriteChunkState = nextWriteChunkStatePtr.Value;
				if (nextWriteChunkState.ReaderFlags != 0)
				{
					await WaitForWriteEvent(cancellationToken);
					ResetWriteEvent();
				}
				else if (nextWriteChunkStatePtr.TryUpdate(nextWriteChunkState, new ChunkState(WriteState.Writing, writerState.ReaderFlags, 0)))
				{
					if (_headerPtr.Writer.TryUpdate(writerState, new WriterState(nextWriteChunkIdx, writerState.ReaderFlags, nextWriteChunkIdx == 0)))
					{
						break;
					}
					else
					{
						writerState = _headerPtr.Writer.Value;
					}
				}
			}
		}
	}
}