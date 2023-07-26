// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO.Pipelines;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Buffers;
using EpicGames.Horde.Compute.Transports;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Horde.Tests
{
	[TestClass]
	public class BufferTests
	{
		const int ChannelId = 0;

		[TestMethod]
		public async Task TestSimpleBuffer()
		{
			using PooledBuffer buffer = new PooledBuffer(2, 1024);
			buffer.Writer.AdvanceWritePosition(10);

			using ComputeBufferReader bufferReader = buffer.CreateReader();
			await bufferReader.WaitToReadAsync(9);
			bufferReader.AdvanceReadPosition(9);
			await bufferReader.WaitToReadAsync(1);
		}

		[TestMethod]
		public void TestOverflow()
		{
			using PooledBuffer buffer = new PooledBuffer(2, 20);
			using ComputeBufferReader bufferReader = buffer.CreateReader();

			// Fill up the first chunk
			Assert.AreEqual(20, buffer.Writer.GetWriteBuffer().Length);
			buffer.Writer.AdvanceWritePosition(10);
			Assert.AreEqual(10, buffer.Writer.GetWriteBuffer().Length);
			buffer.Writer.AdvanceWritePosition(10);
			Assert.AreEqual(0, buffer.Writer.GetWriteBuffer().Length);

			Task waitToWriteTask = buffer.Writer.WaitToWriteAsync(1).AsTask();
			Assert.IsTrue(waitToWriteTask.IsCompleted);

			// Fill up the second chunk
			Assert.AreEqual(20, buffer.Writer.GetWriteBuffer().Length);
			buffer.Writer.AdvanceWritePosition(10);
			Assert.AreEqual(10, buffer.Writer.GetWriteBuffer().Length);
			buffer.Writer.AdvanceWritePosition(10);
			Assert.AreEqual(0, buffer.Writer.GetWriteBuffer().Length);

			waitToWriteTask = buffer.Writer.WaitToWriteAsync(1).AsTask();
			Assert.IsFalse(waitToWriteTask.IsCompleted);

			// Wait for data to be read
			Assert.AreEqual(20, bufferReader.GetReadBuffer().Length);
			bufferReader.AdvanceReadPosition(10);
			Assert.IsFalse(waitToWriteTask.IsCompleted);

			Assert.AreEqual(10, bufferReader.GetReadBuffer().Length);
			bufferReader.AdvanceReadPosition(10);
			Assert.AreEqual(0, bufferReader.GetReadBuffer().Length);

			Task waitToReadTask = bufferReader.WaitToReadAsync(1).AsTask();
			Assert.IsTrue(waitToReadTask.IsCompleted);

			Assert.AreEqual(20, bufferReader.GetReadBuffer().Length);
			Assert.IsTrue(waitToWriteTask.IsCompleted);

			// Make sure both reader and writer have something to work with
			Assert.AreEqual(20, bufferReader.GetReadBuffer().Length);
			Assert.AreEqual(20, buffer.Writer.GetWriteBuffer().Length);
		}

		[TestMethod]
		public async Task TestPooledBuffer()
		{
			await TestProducerConsumerAsync(length => new PooledBuffer(length), CancellationToken.None);
		}

		[TestMethod]
		public async Task TestSharedMemoryBuffer()
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				await TestProducerConsumerAsync(length => SharedMemoryBuffer.CreateNew(null, length), CancellationToken.None);
			}
		}

		static async Task TestProducerConsumerAsync(Func<int, ComputeBuffer> createBuffer, CancellationToken cancellationToken)
		{
			const int Length = 8000;

			Pipe sourceToTargetPipe = new Pipe();
			Pipe targetToSourcePipe = new Pipe();

			await using RemoteComputeSocket producerSocket = new RemoteComputeSocket(new PipeTransport(targetToSourcePipe.Reader, sourceToTargetPipe.Writer), ComputeSocketEndpoint.Local, NullLogger.Instance);
			await using RemoteComputeSocket consumerSocket = new RemoteComputeSocket(new PipeTransport(sourceToTargetPipe.Reader, targetToSourcePipe.Writer), ComputeSocketEndpoint.Remote, NullLogger.Instance);

			using ComputeBuffer consumerBuffer = createBuffer(Length);
			consumerSocket.AttachRecvBuffer(ChannelId, consumerBuffer);

			byte[] input = RandomNumberGenerator.GetBytes(Length);
			Task producerTask = RunProducerAsync(producerSocket, input);

			using ComputeBufferReader consumerBufferReader = consumerBuffer.CreateReader();

			byte[] output = new byte[Length];
			await RunConsumerAsync(consumerBufferReader, output);

			await producerTask;
			Assert.IsTrue(input.SequenceEqual(output));
		}

		static async Task RunProducerAsync(RemoteComputeSocket socket, ReadOnlyMemory<byte> input)
		{
			int offset = 0;
			while (offset < input.Length)
			{
				int length = Math.Min(input.Length - offset, 100);
				await socket.SendAsync(ChannelId, input.Slice(offset, length));
				await Task.Delay(10);
				offset += length;
			}
			await socket.MarkCompleteAsync(ChannelId);
		}

		static async Task RunConsumerAsync(ComputeBufferReader reader, Memory<byte> output)
		{
			int offset = 0;
			while (!reader.IsComplete)
			{
				ReadOnlyMemory<byte> memory = reader.GetReadBuffer();
				if (memory.Length == 0)
				{
					await reader.WaitToReadAsync(1, CancellationToken.None);
					continue;
				}

				int length = Math.Min(memory.Length, 7);
				memory.Slice(0, length).CopyTo(output.Slice(offset));
				reader.AdvanceReadPosition(length);
				offset += length;
			}
		}

		[TestMethod]
		public async Task TestSendBufferComplete()
		{
			Pipe recvPipe = new Pipe();
			Pipe sendPipe = new Pipe();
			await using RemoteComputeSocket localSocket = new RemoteComputeSocket(new PipeTransport(sendPipe.Reader, recvPipe.Writer), ComputeSocketEndpoint.Local, NullLogger.Instance);
			await using RemoteComputeSocket remoteSocket = new RemoteComputeSocket(new PipeTransport(recvPipe.Reader, sendPipe.Writer), ComputeSocketEndpoint.Remote, NullLogger.Instance);

			using (PooledBuffer remoteBuffer = new PooledBuffer(1024))
			{
				remoteSocket.AttachRecvBuffer(1, remoteBuffer);

				using ComputeBufferReader reader = remoteBuffer.CreateReader();

				// Disposing of the buffer should mark the channel as complete
				using (PooledBuffer localBuffer = new PooledBuffer(1024))
				{
					localSocket.AttachSendBuffer(1, localBuffer);
					await localBuffer.Writer.WriteAsync(new byte[] { 1, 2, 3 });
				}

				Assert.IsTrue(await reader.WaitToReadAsync(3));
				Assert.IsTrue(reader.GetReadBuffer().Slice(0, 3).Span.SequenceEqual(new byte[] { 1, 2, 3 }));
				reader.AdvanceReadPosition(3);

				Assert.IsFalse(await reader.WaitToReadAsync(1));
				Assert.IsTrue(reader.IsComplete);
			}
		}
	}
}
