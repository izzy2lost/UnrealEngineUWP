// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO.Pipelines;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Transports;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Horde.Tests
{
	[TestClass]
	public class ComputeSocketTests
	{
		class TestComputeSocket : ComputeSocket, IDisposable
		{
			public Dictionary<int, ComputeBufferWriter> RecvBufferWriters { get; } = new Dictionary<int, ComputeBufferWriter>();
			public Dictionary<int, ComputeBufferReader> SendBufferReaders { get; } = new Dictionary<int, ComputeBufferReader>();

			public override ILogger Logger => NullLogger.Instance;

			public void Dispose()
			{
				foreach (ComputeBufferWriter writer in RecvBufferWriters.Values)
				{
					writer.Dispose();
				}
				foreach (ComputeBufferReader reader in SendBufferReaders.Values)
				{
					reader.Dispose();
				}
			}

			public override void AttachRecvBuffer(int channelId, ComputeBuffer recvBuffer)
			{
				RecvBufferWriters.Add(channelId, recvBuffer.CreateWriter());
			}

			public override void AttachSendBuffer(int channelId, ComputeBuffer sendBuffer)
			{
				SendBufferReaders.Add(channelId, sendBuffer.CreateReader());
			}
		}

		class TestLogger : ILogger
		{
			public IDisposable BeginScope<TState>(TState state) => null!;

			public bool IsEnabled(LogLevel logLevel) => true;

			public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter)
			{
				Console.WriteLine($"{logLevel}: {formatter(state, exception)}");
				Assert.IsFalse(logLevel == LogLevel.Error);
				Assert.IsFalse(logLevel == LogLevel.Warning);
			}
		}

		[TestMethod]
		public async Task TestAgentMessageLoopPipeAsync()
		{
			Pipe recvPipe = new Pipe();
			Pipe sendPipe = new Pipe();
			await using RemoteComputeSocket localSocket = new RemoteComputeSocket(new PipeTransport(sendPipe.Reader, recvPipe.Writer), new TestLogger());
			await using RemoteComputeSocket agentSocket = new RemoteComputeSocket(new PipeTransport(recvPipe.Reader, sendPipe.Writer), new TestLogger());

			await RunAgentTestsAsync(localSocket, agentSocket);
		}

		[TestMethod]
		public async Task TestAgentMessageLoopTcpAsync()
		{
			const int Port = 9990;
			TcpListener listener = new TcpListener(IPAddress.Loopback, Port);
			listener.Start();

			using Socket clientSocket = new Socket(SocketType.Stream, ProtocolType.Tcp);
			Task clientConnectTask = clientSocket.ConnectAsync(IPAddress.Loopback, Port, CancellationToken.None).AsTask();

			using Socket serverSocket = await listener.AcceptSocketAsync(CancellationToken.None);
			await clientConnectTask;

			await using RemoteComputeSocket localSocket = new RemoteComputeSocket(new TcpTransport(clientSocket), new TestLogger());
			await using RemoteComputeSocket agentSocket = new RemoteComputeSocket(new TcpTransport(serverSocket), new TestLogger());

			await RunAgentTestsAsync(localSocket, agentSocket);
		}

		static async Task RunAgentTestsAsync(RemoteComputeSocket localSocket, RemoteComputeSocket agentSocket)
		{
			DirectoryReference tempDir = new DirectoryReference("test-temp");
			await using (BackgroundTask agentTask = BackgroundTask.StartNew(ctx => RunAgentAsync(agentSocket, tempDir, ctx)))
			{
				const int PrimaryChannelId = 0;
				using (AgentMessageChannel channel = localSocket.CreateAgentMessageChannel(PrimaryChannelId, 4 * 1024 * 1024))
				{
					await channel.WaitForAttachAsync();

					await channel.PingAsync();
					using (AgentMessage message = await channel.ReceiveAsync(CancellationToken.None))
					{
						Assert.AreEqual(AgentMessageType.Ping, message.Type);
						Assert.IsTrue(message.Data.Span.SequenceEqual(ReadOnlySpan<byte>.Empty));
					}

					await channel.SendXorRequestAsync(new byte[] { 1, 2, 3 }, 44);
					using (AgentMessage message = await channel.ReceiveAsync(CancellationToken.None))
					{
						Assert.AreEqual(AgentMessageType.XorResponse, message.Type);
						Assert.IsTrue(message.Data.Span.SequenceEqual(new byte[] { 1 ^ 44, 2 ^ 44, 3 ^ 44 }));
					}

					const int SecondaryChannelId = 1;
					using (AgentMessageChannel channel2 = localSocket.CreateAgentMessageChannel(SecondaryChannelId, 4 * 1024 * 1024))
					{
						await channel.ForkAsync(SecondaryChannelId, 4 * 1024 * 1024);

						await channel2.WaitForAttachAsync();

						await channel2.SendXorRequestAsync(new byte[] { 1, 2, 3 }, 44);
						using (AgentMessage message = await channel2.ReceiveAsync(CancellationToken.None))
						{
							Assert.AreEqual(AgentMessageType.XorResponse, message.Type);
							Assert.IsTrue(message.Data.Span.SequenceEqual(new byte[] { 1 ^ 44, 2 ^ 44, 3 ^ 44 }));
						}

						await channel2.CloseAsync();
					}

					using MemoryStorageClient memoryStorage = new MemoryStorageClient();
					using BundleStorageClient storage = new BundleStorageClient(memoryStorage, BundleCache.None, NullLogger.Instance);
					await using (IStorageWriter treeWriter = storage.CreateWriter())
					{
						FileReference file = FileReference.Combine(tempDir, "subdir/hello.txt");
						if (FileReference.Exists(file))
						{
							FileReference.Delete(file);
						}
						Assert.IsFalse(FileReference.Exists(file));

						byte[] data = Encoding.UTF8.GetBytes("Hello world");

						using ChunkedDataWriter writer = new ChunkedDataWriter(treeWriter, new ChunkingOptions());
						ChunkedData chunkedData = await writer.CreateAsync(data, CancellationToken.None);

						DirectoryNode directory = new DirectoryNode();
						directory.AddFile("hello.txt", FileEntryFlags.None, data.Length, chunkedData);

						HashedNodeRef<DirectoryNode> directoryRef = await treeWriter.WriteHashedNodeAsync(directory);

						DirectoryNode root = new DirectoryNode();
						root.AddDirectory(new DirectoryEntry("subdir", directory.Length, directoryRef));

						IBlobHandle handle = await treeWriter.FlushAsync(root);
						await channel.UploadFilesAsync("", handle.GetLocator(), storage);

						Assert.IsTrue(FileReference.Exists(file));
						byte[] readData = await FileReference.ReadAllBytesAsync(file);
						Assert.IsTrue(readData.SequenceEqual(data));

						await channel.DeleteFilesAsync(new[] { "subdir/hello.txt" }, CancellationToken.None);

						await channel.PingAsync();
						using (AgentMessage message = await channel.ReceiveAsync(CancellationToken.None))
						{
							Assert.AreEqual(AgentMessageType.Ping, message.Type);
							Assert.IsTrue(message.Data.Span.SequenceEqual(ReadOnlySpan<byte>.Empty));
						}

						Assert.IsFalse(FileReference.Exists(file));
					}
				}
			}

			await localSocket.CloseAsync(CancellationToken.None);
			await agentSocket.CloseAsync(CancellationToken.None);
		}

		static async Task RunAgentAsync(ComputeSocket socket, DirectoryReference tempDir, CancellationToken cancellationToken)
		{
			AgentMessageHandler handler = new AgentMessageHandler(tempDir, null, true, null, NullLogger.Instance);
			await handler.RunAsync(socket, cancellationToken);
		}
	}
}
