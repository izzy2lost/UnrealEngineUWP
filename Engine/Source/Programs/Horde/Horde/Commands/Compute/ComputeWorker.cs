// Copyright Epic Games, Inc. All Rights Reserved.

using System.Net;
using System.Net.Sockets;
using EpicGames.Core;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Transports;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;

namespace Horde.Commands.Compute
{
	/// <summary>
	/// Helper command for hosting a local compute worker in a separate process
	/// </summary>
	[Command("compute", "worker", "Runs the agent as a local compute host, accepting incoming connections on the loopback adapter with a given port")]
	class ComputeWorkerCommand : Command
	{
		readonly IMemoryCache _memoryCache;

		[CommandLine("-Port=")]
		int Port { get; set; } = 2000;

		public ComputeWorkerCommand(IMemoryCache memoryCache)
		{
			_memoryCache = memoryCache;
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			logger.LogInformation("** WORKER **");

			using Socket tcpSocket = new Socket(SocketType.Stream, ProtocolType.IP);
			await tcpSocket.ConnectAsync(IPAddress.Loopback, Port);

			await using (RemoteComputeSocket socket = new RemoteComputeSocket(new TcpTransport(tcpSocket), ComputeSocketEndpoint.Remote, logger))
			{
				logger.LogInformation("Running worker...");
				await RunWorkerAsync(socket, _memoryCache, logger, CancellationToken.None);
				logger.LogInformation("Worker complete");
				await socket.CloseAsync(CancellationToken.None);
			}

			logger.LogInformation("Stopping");
			return 0;
		}

		public static async Task RunWorkerAsync(ComputeSocket socket, IMemoryCache memoryCache, ILogger logger, CancellationToken cancellationToken)
		{
			DirectoryReference sandboxDir = DirectoryReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.LocalApplicationData)!, "Horde", "Sandbox");

			AgentMessageHandler worker = new AgentMessageHandler(sandboxDir, memoryCache, null, false, null, logger);
			await worker.RunAsync(socket, cancellationToken);
		}
	}
}
