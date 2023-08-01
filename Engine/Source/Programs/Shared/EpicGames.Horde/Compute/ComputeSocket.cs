// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute.Buffers;
using JetBrains.Annotations;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Socket for sending and reciving data using a "push" model. The application can attach multiple writers to accept received data.
	/// </summary>
	public abstract class ComputeSocket
	{
		/// <summary>
		/// Attaches a buffer to receive data.
		/// </summary>
		/// <param name="channelId">Channel to receive data on</param>
		/// <param name="recvBuffer">Writer for the buffer to store received data</param>
		public abstract void AttachRecvBuffer(int channelId, ComputeBuffer recvBuffer);

		/// <summary>
		/// Attaches a buffer to send data.
		/// </summary>
		/// <param name="channelId">Channel to receive data on</param>
		/// <param name="sendBuffer">Reader for the buffer to send data from</param>
		public abstract void AttachSendBuffer(int channelId, ComputeBuffer sendBuffer);

		/// <summary>
		/// Creates a channel using a socket and receive buffer
		/// </summary>
		/// <param name="channelId">Channel id to send and receive data</param>
		public ComputeChannel CreateChannel(int channelId)
		{
			using SharedMemoryBuffer recvBuffer = SharedMemoryBuffer.CreateNew(null, 65536);
			using SharedMemoryBuffer sendBuffer = SharedMemoryBuffer.CreateNew(null, 65536);
			return CreateChannel(channelId, recvBuffer, sendBuffer);
		}

		/// <summary>
		/// Creates a channel using a socket and receive buffer
		/// </summary>
		/// <param name="channelId">Channel id to send and receive data</param>
		/// <param name="recvBuffer">Buffer for receiving data</param>
		/// <param name="sendBuffer">Buffer for sending data</param>
		public ComputeChannel CreateChannel(int channelId, ComputeBuffer recvBuffer, ComputeBuffer sendBuffer)
		{
			AttachRecvBuffer(channelId, recvBuffer);
			AttachSendBuffer(channelId, sendBuffer);

			using ComputeBufferReader recvBufferReader = recvBuffer.CreateReader();
			using ComputeBufferWriter sendBufferWriter = sendBuffer.CreateWriter();

			return new ComputeChannel(recvBufferReader, sendBufferWriter);
		}
	}

	internal enum IpcMessage
	{
		AttachRecvBuffer = 0,
		AttachSendBuffer = 1,
	}

	/// <summary>
	/// Provides functionality for attaching buffers for compute workers 
	/// </summary>
	public sealed class WorkerComputeSocket : ComputeSocket, IDisposable
	{
		/// <summary>
		/// Name of the environment variable for passing the name of the compute channel
		/// </summary>
		public const string IpcEnvVar = "UE_HORDE_COMPUTE_IPC";

		readonly ComputeBufferWriter _commandBufferWriter;
		readonly List<ComputeBuffer> _buffers = new List<ComputeBuffer>();

		/// <summary>
		/// Creates a socket for a worker
		/// </summary>
		private WorkerComputeSocket(ComputeBufferWriter commandBufferWriter)
		{
			_commandBufferWriter = commandBufferWriter;
		}

		/// <summary>
		/// Opens a socket which allows a worker to communicate with the Horde Agent
		/// </summary>
		public static WorkerComputeSocket Open()
		{
			string? baseName = Environment.GetEnvironmentVariable(IpcEnvVar);
			if (baseName == null)
			{
				throw new InvalidOperationException($"Environment variable {IpcEnvVar} is not defined; cannot connect as worker.");
			}

			return Open(baseName);
		}

		/// <summary>
		/// Opens a socket which allows a worker to communicate with the Horde Agent
		/// </summary>
		/// <param name="commandBufferName">Name of the command buffer</param>
		public static WorkerComputeSocket Open(string commandBufferName)
		{
			using SharedMemoryBuffer commandBuffer = SharedMemoryBuffer.OpenExisting(commandBufferName);
			return new WorkerComputeSocket(commandBuffer.CreateWriter());
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_commandBufferWriter.Dispose();
		}

		/// <inheritdoc/>
		public override void AttachRecvBuffer(int channelId, ComputeBuffer buffer)
		{
			_buffers.Add(buffer.AddRef());
			string bufferName = ((SharedMemoryBufferDetail)buffer._detail).Name;
			AttachBuffer(IpcMessage.AttachRecvBuffer, channelId, bufferName);
		}

		/// <inheritdoc/>
		public override void AttachSendBuffer(int channelId, ComputeBuffer buffer)
		{
			_buffers.Add(buffer.AddRef());
			string bufferName = ((SharedMemoryBufferDetail)buffer._detail).Name;
			AttachBuffer(IpcMessage.AttachSendBuffer, channelId, bufferName);
		}

		void AttachBuffer(IpcMessage message, int channelId, string bufferName)
		{
			MemoryWriter writer = new MemoryWriter(_commandBufferWriter.GetWriteBuffer());
			writer.WriteUnsignedVarInt((int)message);
			writer.WriteUnsignedVarInt(channelId);
			writer.WriteString(bufferName);

			_commandBufferWriter.AdvanceWritePosition(writer.Length);
		}
	}

	/// <summary>
	/// Enum identifying which end of the socket a particular machine is
	/// </summary>
	public enum ComputeSocketEndpoint
	{
		/// <summary>
		/// The initiating machine
		/// </summary>
		Local,

		/// <summary>
		/// The remote machine
		/// </summary>
		Remote,
	}

	/// <summary>
	/// Manages a set of readers and writers to buffers across a transport layer
	/// </summary>
	public class RemoteComputeSocket : ComputeSocket, IAsyncDisposable
	{
		enum ControlMessageType
		{
			Detach = -2,
		}

		class RecvBuffer : IDisposable
		{
			public ComputeBufferWriter? _writer;
			public readonly SemaphoreSlim _semaphore = new SemaphoreSlim(1);
			public int _refCount = 1;

			public RecvBuffer(ComputeBufferWriter writer) => _writer = writer;

			public void AddRef() => Interlocked.Increment(ref _refCount);

			public void Release()
			{
				if (Interlocked.Decrement(ref _refCount) == 0)
				{
					Dispose();
				}
			}

			public void Dispose()
			{
				_writer?.Dispose();
				_semaphore.Dispose();
			}
		}

		class SendBuffer : IDisposable
		{
			public ComputeBufferReader? _reader;
			public readonly SemaphoreSlim _semaphore = new SemaphoreSlim(1);
			[SuppressMessage("Usage", "CA2213:Disposable fields should be disposed")]
			public readonly BackgroundTask _task;
			int _refCount = 1;

			public SendBuffer(ComputeBufferReader reader, Func<SendBuffer, CancellationToken, Task> func)
			{
				_reader = reader;
				_task = BackgroundTask.StartNew(ctx => func(this, ctx));
			}

			public void AddRef() => Interlocked.Increment(ref _refCount);

			public void Release()
			{
				if (Interlocked.Decrement(ref _refCount) == 0)
				{
					Dispose();
				}
			}

			public void Dispose()
			{
				_reader?.Dispose();
				_semaphore.Dispose();
			}
		}

		readonly object _lockObject = new object();

		bool _complete;

		readonly ComputeTransport _transport;
		readonly ComputeSocketEndpoint _endpoint;
		readonly ILogger _logger;

		BackgroundTask? _recvTask;
		readonly Dictionary<int, RecvBuffer> _recvBuffers = new Dictionary<int, RecvBuffer>();

		readonly SemaphoreSlim _sendSemaphore = new SemaphoreSlim(1, 1);
		readonly Dictionary<int, SendBuffer> _sendBuffers = new Dictionary<int, SendBuffer>();

		string Tag => (_endpoint == ComputeSocketEndpoint.Local)? "LOCAL": "REMOTE";

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="transport">Transport to communicate with the remote</param>
		/// <param name="endpoint">Tag for log messages</param>
		/// <param name="logger">Logger for trace output</param>
		public RemoteComputeSocket(ComputeTransport transport, ComputeSocketEndpoint endpoint, ILogger logger)
		{
			_transport = transport;
			_endpoint = endpoint;
			_logger = logger;
		}

		/// <summary>
		/// Attempt to gracefully close the current connection and shutdown both ends of the transport
		/// </summary>
		public async ValueTask CloseAsync(CancellationToken cancellationToken)
		{
			// Close all the buffers
			await DetachAllBuffersAsync(true, true, cancellationToken);

			// Send a final message indicating that the lease is done. This will allow the senders on the remote end to terminate.
			await _transport.MarkCompleteAsync(cancellationToken);

			// Wait for the reader to stop
			if (_recvTask != null)
			{
				await _recvTask.StopAsync();
			}
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			await CloseAsync(CancellationToken.None);
			_sendSemaphore.Dispose();
			GC.SuppressFinalize(this);
		}

		async Task RunRecvTaskAsync(ComputeTransport transport, CancellationToken cancellationToken)
		{
			_logger.LogTrace("[{Tag}] Started socket reader", Tag);

			List<Task> detachTasks = new List<Task>();

			byte[] header = new byte[8];
			try
			{
				Memory<byte> last = Memory<byte>.Empty;

				// Process messages from the remote
				for (; ; )
				{
					detachTasks.RemoveCompleteTasks();

					// Read the next packet header
					if (!await transport.RecvOptionalAsync(header, cancellationToken))
					{
						_logger.LogTrace("[{Tag}] End of socket", Tag);
						break;
					}

					// Parse the target buffer and packet size
					int id = BinaryPrimitives.ReadInt32LittleEndian(header);
					int size = BinaryPrimitives.ReadInt32LittleEndian(header.AsSpan(4));

					// Dispatch it to the correct place
					if (size >= 0)
					{
						await ReadPacketAsync(transport, id, size, cancellationToken);
					}
					else if (size == (int)ControlMessageType.Detach)
					{
						detachTasks.Add(DetachRecvBufferAsync(id, cancellationToken));
					}
					else
					{
						_logger.LogWarning("[{Tag}] Unrecognized control message: {Message}", Tag, size);
					}
				}
			}
			catch (OperationCanceledException)
			{
			}

			// Mark all buffers as complete
			lock (_lockObject)
			{
				_complete = true;
				foreach (int channelIdx in _recvBuffers.Keys)
				{
					detachTasks.Add(DetachRecvBufferAsync(channelIdx, cancellationToken));
				}
			}

			// Wait for all the detach tasks to finish
			if (detachTasks.Count > 0)
			{
				_logger.LogTrace("[{Tag}] Waiting for detach tasks to complete...", Tag);
				await Task.WhenAll(detachTasks).WaitAsync(cancellationToken);
			}

			_logger.LogTrace("[{Tag}] Closing reader", Tag);
		}

		async Task ReadPacketAsync(ComputeTransport transport, int id, int size, CancellationToken cancellationToken)
		{
			if (!await TryReadPacketAsync(transport, id, size, cancellationToken))
			{
				_logger.LogWarning("Discarding {Size} bytes received on compute channel {Id}", size, id);

				int bufferSize = Math.Min(size, 65536);
				using (IMemoryOwner<byte> buffer = MemoryPool<byte>.Shared.Rent(bufferSize))
				{
					for (int remaining = size; remaining > 0;)
					{
						int chunkSize = Math.Min(bufferSize, remaining);
						await transport.RecvAsync(buffer.Memory.Slice(0, chunkSize), cancellationToken);
						remaining -= chunkSize;
					}
				}
			}
		}

		async ValueTask<bool> TryReadPacketAsync(ComputeTransport transport, int id, int size, CancellationToken cancellationToken)
		{
			// Try to get the receive buffer for this channel
			RecvBuffer? recvBuffer;
			lock (_lockObject)
			{
				if (_recvBuffers.TryGetValue(id, out recvBuffer))
				{
					recvBuffer.AddRef();
				}
				else
				{
					return false;
				}
			}

			// Lock it and read a packet
			bool result = false;
			try
			{
				await recvBuffer._semaphore.WaitAsync(cancellationToken);
				try
				{
					ComputeBufferWriter? writer = recvBuffer._writer;
					if (writer != null)
					{
						Memory<byte> memory = writer.GetWriteBuffer();
						while (memory.Length < size)
						{
							_logger.LogTrace("[{Tag}] No space in buffer {Id}, flushing", Tag, id);
							await writer.WaitToWriteAsync(size, cancellationToken);
							memory = writer.GetWriteBuffer();
						}

						for (int offset = 0; offset < size;)
						{
							int read = await transport.RecvAsync(memory.Slice(offset, size - offset), cancellationToken);
							offset += read;
						}

						writer.AdvanceWritePosition(size);
						result = true;
					}
				}
				finally
				{
					recvBuffer._semaphore.Release();
				}
			}
			finally
			{
				recvBuffer.Release();
			}
			return result;
		}

		class SendSegment : ReadOnlySequenceSegment<byte>
		{
			public void Set(ReadOnlyMemory<byte> memory, ReadOnlySequenceSegment<byte>? next, long runningIndex)
			{
				Memory = memory;
				Next = next;
				RunningIndex = runningIndex;
			}
		}

		readonly byte[] _header = new byte[8];
		readonly SendSegment _headerSegment = new SendSegment();
		readonly SendSegment _bodySegment = new SendSegment();

		/// <inheritdoc/>
		async ValueTask SendAsync(int id, ReadOnlyMemory<byte> memory, CancellationToken cancellationToken = default)
		{
			if (memory.Length > 0)
			{
				await SendInternalAsync(id, memory.Length, memory, cancellationToken);
			}
		}

		/// <inheritdoc/>
		public ValueTask MarkCompleteAsync(int id, CancellationToken cancellationToken = default) => SendInternalAsync(id, (int)ControlMessageType.Detach, ReadOnlyMemory<byte>.Empty, cancellationToken);

		async ValueTask SendInternalAsync(int id, int size, ReadOnlyMemory<byte> memory, CancellationToken cancellationToken)
		{
			await _sendSemaphore.WaitAsync(cancellationToken);
			try
			{
				BinaryPrimitives.WriteInt32LittleEndian(_header, id);
				BinaryPrimitives.WriteInt32LittleEndian(_header.AsSpan(4), size);
				_headerSegment.Set(_header, _bodySegment, 0);
				_bodySegment.Set(memory, null, _header.Length);

				ReadOnlySequence<byte> sequence = new ReadOnlySequence<byte>(_headerSegment, 0, _bodySegment, memory.Length);
				await _transport.SendAsync(sequence, cancellationToken);
			}
			finally
			{
				_sendSemaphore.Release();
			}
		}

		/// <inheritdoc/>
		public override void AttachRecvBuffer(int channelId, ComputeBuffer recvBuffer)
		{
			_logger.LogTrace("[{Tag}] Attaching recv buffer {Id}", Tag, channelId);
			lock (_lockObject)
			{
				if (_complete)
				{
					throw new InvalidOperationException($"Cannot attach new buffer to channel {channelId} after socket is closed");
				}
				if (_recvBuffers.ContainsKey(channelId))
				{
					throw new InvalidOperationException($"Buffer is already attached to channel {channelId}");
				}

				_recvBuffers.Add(channelId, new RecvBuffer(recvBuffer.CreateWriter()));
				_recvTask ??= BackgroundTask.StartNew(ctx => RunRecvTaskAsync(_transport, ctx));
			}
		}

		[SuppressMessage("Reliability", "CA2000:Dispose objects before losing scope")]
		async Task DetachRecvBufferAsync(int id, CancellationToken cancellationToken)
		{
			_logger.LogTrace("[{Tag}] Detaching recv buffer {Id}", Tag, id);

			// Get the current receive buffer
			RecvBuffer? recvBuffer;
			lock (_lockObject)
			{
				if (!_recvBuffers.TryGetValue(id, out recvBuffer))
				{
					_logger.LogTrace("[{Tag}] Buffer {Id} has already been detached", Tag, id);
					return;
				}
				recvBuffer.AddRef(); // Note: adding extra ref here
			}

			// Release the writer
			await recvBuffer._semaphore.WaitAsync(cancellationToken);
			try
			{
				recvBuffer._writer?.Dispose();
				recvBuffer._writer = null;
			}
			finally
			{
				recvBuffer._semaphore.Release();
				recvBuffer.Release(); // Ref added above
			}

			// Remove the buffer
			lock (_lockObject)
			{
				if (_recvBuffers.Remove(id, out recvBuffer))
				{
					recvBuffer.Release(); // Original ref from _recvBuffers
				}
			}
		}

		async Task DetachAllBuffersAsync(bool recvBuffers, bool sendBuffers, CancellationToken cancellationToken)
		{
			int[] recvChannelIds;
			int[] sendChannelIds;
			lock (_lockObject)
			{
				_complete = true;
				recvChannelIds = _recvBuffers.Keys.ToArray();
				sendChannelIds = _sendBuffers.Keys.ToArray();
			}

			List<Task> tasks = new List<Task>();
			if (recvBuffers)
			{
				foreach (int recvChannelId in recvChannelIds)
				{
					tasks.Add(DetachRecvBufferAsync(recvChannelId, cancellationToken));
				}
			}
			if (sendBuffers)
			{
				foreach (int sendChannelId in sendChannelIds)
				{
					tasks.Add(DetachSendBufferAsync(sendChannelId, cancellationToken));
				}
			}
			await Task.WhenAll(tasks).WaitAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public override void AttachSendBuffer(int channelId, ComputeBuffer sendBuffer)
		{
			_logger.LogTrace("[{Tag}] Attaching send buffer {Id}", Tag, channelId);
			lock (_lockObject)
			{
				if (_sendBuffers.ContainsKey(channelId))
				{
					throw new InvalidOperationException($"Buffer is already attached to channel {channelId}");
				}

				ComputeBufferReader sendBufferReader = sendBuffer.CreateReader();
				SendBuffer sendBufferInfo = new SendBuffer(sendBufferReader, (buffer, ctx) => SendFromBufferAsync(channelId, buffer, ctx));
				_sendBuffers.Add(channelId, sendBufferInfo);
			}
		}

		[SuppressMessage("Reliability", "CA2000:Dispose objects before losing scope")]
		async Task DetachSendBufferAsync(int channelId, CancellationToken cancellationToken)
		{
			// Get the current send buffer state
			SendBuffer? sendBuffer;
			lock (_lockObject)
			{
				if (!_sendBuffers.TryGetValue(channelId, out sendBuffer))
				{
					_logger.LogWarning("No buffer is attached to channel {ChannelId}", channelId);
					return;
				}
				sendBuffer.AddRef();
			}

			// Release the reader
			await sendBuffer._semaphore.WaitAsync(cancellationToken);
			try
			{
				sendBuffer._reader?.Dispose();
				sendBuffer._reader = null;
			}
			finally
			{
				sendBuffer._semaphore.Release();
				sendBuffer.Release(); // Added above
			}

			// Wait for the send task to complete
			await sendBuffer._task.DisposeAsync();

			// Remove the buffer from the dictionary
			lock (_lockObject)
			{
				if (_sendBuffers.Remove(channelId, out sendBuffer))
				{
					sendBuffer.Release(); // For _sendBuffers
				}
			}
		}

		async Task SendFromBufferAsync(int channelId, SendBuffer sendBuffer, CancellationToken cancellationToken)
		{
			ComputeBufferReader reader = sendBuffer._reader!;
			while (!cancellationToken.IsCancellationRequested)
			{
				ReadOnlyMemory<byte> memory = reader.GetReadBuffer();
				if (memory.Length > 0)
				{
					await SendAsync(channelId, memory, cancellationToken);
					reader.AdvanceReadPosition(memory.Length);
				}
				if (reader.IsComplete)
				{
					await MarkCompleteAsync(channelId, cancellationToken);
					break;
				}
				await reader.WaitToReadAsync(1, cancellationToken);
			}
		}
	}
}
