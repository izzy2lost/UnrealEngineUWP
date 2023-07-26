// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute.Buffers;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;
using EpicGames.Horde.Storage.Bundles;
using System.Text;
using System.IO;
using System.Reflection;
using System.Linq;
using System.Buffers.Binary;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Implements the remote end of a compute worker. 
	/// </summary>
	public class AgentMessageHandler
	{
		readonly DirectoryReference _sandboxDir;
		readonly IMemoryCache _memoryCache;
		readonly ILogger _logger;

		readonly bool _executeLocally = false;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="sandboxDir">Directory to use for reading/writing files</param>
		/// <param name="memoryCache">Cache for nodes read from storage</param>
		/// <param name="logger">Logger for diagnostics</param>
		public AgentMessageHandler(DirectoryReference sandboxDir, IMemoryCache memoryCache, ILogger logger)
		{
			_sandboxDir = sandboxDir;
			_memoryCache = memoryCache;
			_logger = logger;
		}

		/// <summary>
		/// Runs the worker using commands sent along the given socket
		/// </summary>
		/// <param name="socket">Socket to read from</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task RunAsync(ComputeSocket socket, CancellationToken cancellationToken)
		{
			await RunAsync(socket, 0, 4 * 1024 * 1024, cancellationToken);
		}

		async Task RunAsync(ComputeSocket socket, int channelId, int bufferSize, CancellationToken cancellationToken)
		{
			using (AgentMessageChannel channel = socket.CreateAgentMessageChannel(channelId, bufferSize, _logger))
			{
				await channel.AttachAsync(cancellationToken);

				List<Task> childTasks = new List<Task>();
				for (; ; )
				{
					using AgentMessage message = await channel.ReceiveAsync(cancellationToken);
					_logger.LogTrace("Compute Channel {ChannelId}: {MessageType}", channelId, message.Type);

					switch (message.Type)
					{
						case AgentMessageType.None:
							await Task.WhenAll(childTasks);
							return;
						case AgentMessageType.Ping:
							await channel.PingAsync(cancellationToken);
							break;
						case AgentMessageType.Fork:
							{
								ForkMessage fork = message.ParseForkMessage();
								childTasks.Add(Task.Run(() => RunAsync(socket, fork.channelId, fork.bufferSize, cancellationToken), cancellationToken));
							}
							break;
						case AgentMessageType.WriteFiles:
							{
								UploadFilesMessage writeFiles = message.ParseUploadFilesMessage();
								await WriteFilesAsync(channel, writeFiles.Name, writeFiles.Locator, cancellationToken);
							}
							break;
						case AgentMessageType.DeleteFiles:
							{
								DeleteFilesMessage deleteFiles = message.ParseDeleteFilesMessage();
								DeleteFiles(deleteFiles.Filter);
							}
							break;
						case AgentMessageType.Execute:
							{
								ExecuteProcessMessage executeProcess = message.ParseExecuteProcessMessage();
								await ExecuteProcessAsync(socket, channel, executeProcess.Executable, executeProcess.Arguments, executeProcess.WorkingDir, executeProcess.EnvVars, cancellationToken);
							}
							break;
						case AgentMessageType.XorRequest:
							{
								XorRequestMessage xorRequest = message.AsXorRequest();
								await RunXor(channel, xorRequest.Data, xorRequest.Value, cancellationToken);
							}
							break;
						default:
							throw new InvalidAgentMessageException(message);
					}
				}
			}
		}

		static async ValueTask RunXor(AgentMessageChannel channel, ReadOnlyMemory<byte> source, byte value, CancellationToken cancellationToken)
		{
			using IAgentMessageBuilder response = await channel.CreateMessageAsync(AgentMessageType.XorResponse, source.Length, cancellationToken);
			XorData(source.Span, response.GetSpanAndAdvance(source.Length), value);
			response.Send();
		}

		static void XorData(ReadOnlySpan<byte> source, Span<byte> target, byte value)
		{
			for (int idx = 0; idx < source.Length; idx++)
			{
				target[idx] = (byte)(source[idx] ^ value);
			}
		}

		async Task WriteFilesAsync(AgentMessageChannel channel, string path, BundleNodeLocator locator, CancellationToken cancellationToken)
		{
			using AgentStorageClient store = new AgentStorageClient(channel);
			BundleReader reader = new BundleReader(store, _memoryCache, _logger);

			DirectoryNode directoryNode = await reader.ReadNodeAsync<DirectoryNode>(locator, cancellationToken);

			DirectoryReference outputDir = DirectoryReference.Combine(_sandboxDir, path);
			if (!outputDir.IsUnderDirectory(_sandboxDir))
			{
				throw new InvalidOperationException("Cannot write files outside sandbox");
			}

			await directoryNode.CopyToDirectoryAsync(outputDir.ToDirectoryInfo(), _logger, cancellationToken);

			using (IAgentMessageBuilder message = await channel.CreateMessageAsync(AgentMessageType.WriteFilesResponse, cancellationToken))
			{
				message.Send();
			}
		}

		void DeleteFiles(IReadOnlyList<string> deleteFiles)
		{
			FileFilter filter = new FileFilter(deleteFiles);

			List<FileReference> files = filter.ApplyToDirectory(_sandboxDir, false);
			foreach (FileReference file in files)
			{
				FileUtils.ForceDeleteFile(file);
			}
		}

		async Task ExecuteProcessAsync(ComputeSocket socket, AgentMessageChannel channel, string executable, IReadOnlyList<string> arguments, string? workingDir, IReadOnlyDictionary<string, string?>? envVars, CancellationToken cancellationToken)
		{
			try
			{
				if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
				{
					await ExecuteProcessWindowsAsync(socket, channel, executable, arguments, workingDir, envVars, cancellationToken);
				}
				else
				{
					await ExecuteProcessInternalAsync(channel, executable, arguments, workingDir, envVars, cancellationToken);
				}
			}
			catch (Exception ex)
			{
				await channel.SendExceptionAsync(ex, cancellationToken);
			}
		}

		async Task ExecuteProcessWindowsAsync(ComputeSocket socket, AgentMessageChannel channel, string executable, IReadOnlyList<string> arguments, string? workingDir, IReadOnlyDictionary<string, string?>? envVars, CancellationToken cancellationToken)
		{
			Dictionary<string, string?> newEnvVars = new Dictionary<string, string?>();
			if (envVars != null)
			{
				foreach ((string name, string? value) in envVars)
				{
					newEnvVars.Add(name, value);
				}
			}

			using (SharedMemoryBuffer ipcBuffer = SharedMemoryBuffer.CreateNew(null, 1, 64 * 1024))
			{
				newEnvVars[WorkerComputeSocket.IpcEnvVar] = ipcBuffer.Name;

				using ComputeBufferReader ipcBufferReader = ipcBuffer.CreateReader();
				using BackgroundTask backgroundTask = BackgroundTask.StartNew(ctx => ProcessIpcMessagesAsync(socket, ipcBufferReader, cancellationToken));

				_logger.LogInformation("Launching {Executable} {Arguments}", CommandLineArguments.Quote(executable), CommandLineArguments.Join(arguments));
				await ExecuteProcessInternalAsync(channel, executable, arguments, workingDir, newEnvVars, cancellationToken);
				_logger.LogInformation("Finished executing process");
				ipcBuffer.Writer.MarkComplete();
			}

			_logger.LogInformation("Child process has shut down");
		}

		async Task ProcessIpcMessagesAsync(ComputeSocket socket, ComputeBufferReader ipcReader, CancellationToken cancellationToken)
		{
			List<SharedMemoryBuffer> buffers = new();
			try
			{
				List<(int, ComputeBufferWriter)> writers = new List<(int, ComputeBufferWriter)>();
				while (await ipcReader.WaitToReadAsync(1, cancellationToken))
				{
					ReadOnlyMemory<byte> memory = ipcReader.GetReadBuffer();
					MemoryReader reader = new MemoryReader(memory);

					IpcMessage message = (IpcMessage)reader.ReadUnsignedVarInt();
					try
					{
						switch (message)
						{
							case IpcMessage.AttachSendBuffer:
								{
									int channelId = (int)reader.ReadUnsignedVarInt();
									string name = reader.ReadString();
									_logger.LogDebug("Attaching send buffer for channel {ChannelId} to {Name}", channelId, name);

									SharedMemoryBuffer buffer = SharedMemoryBuffer.OpenExisting(name);
									buffers.Add(buffer);

									socket.AttachSendBuffer(channelId, buffer);
								}
								break;
							case IpcMessage.AttachRecvBuffer:
								{
									int channelId = (int)reader.ReadUnsignedVarInt();
									string name = reader.ReadString();
									_logger.LogDebug("Attaching recv buffer for channel {ChannelId} to {Name}", channelId, name);

									SharedMemoryBuffer buffer = SharedMemoryBuffer.OpenExisting(name);
									buffers.Add(buffer);

									socket.AttachRecvBuffer(channelId, buffer);
								}
								break;
							default:
								throw new InvalidOperationException($"Invalid IPC message: {message}");
						}
					}
					catch (Exception ex)
					{
						_logger.LogError(ex, "Exception while processing messages from child process: {Message}", ex.Message);
					}

					ipcReader.AdvanceReadPosition(memory.Length - reader.RemainingMemory.Length);
				}
			}
			finally
			{
				foreach (SharedMemoryBuffer buffer in buffers)
				{
					buffer.Dispose();
				}
			}
		}

		async Task ExecuteProcessInternalAsync(AgentMessageChannel channel, string executable, IReadOnlyList<string> arguments, string? workingDir, IReadOnlyDictionary<string, string?>? envVars, CancellationToken cancellationToken)
		{
			string resolvedWorkingDir = DirectoryReference.Combine(_sandboxDir, workingDir ?? String.Empty).FullName;
			if (_executeLocally)
			{
				List<(string, string?)> prevEnvVars = new List<(string, string?)>();
				if (envVars != null)
				{
					foreach ((string key, string? value) in envVars)
					{
						prevEnvVars.Add((key, Environment.GetEnvironmentVariable(key)));
						Environment.SetEnvironmentVariable(key, value);
					}
				}

				string prevWorkingDir = Directory.GetCurrentDirectory();
				Directory.SetCurrentDirectory(resolvedWorkingDir);

				try
				{
					string resolvedExecutable = FileReference.Combine(_sandboxDir, arguments[0]).FullName;
					string[] mainArgs = arguments.Skip(1).ToArray();

					TaskCompletionSource<int> resultTcs = new TaskCompletionSource<int>();

					Thread thread = new Thread(() => resultTcs.SetResult(AppDomain.CurrentDomain.ExecuteAssembly(resolvedExecutable, mainArgs)));
					thread.Start();

					int result = await resultTcs.Task;
					await channel.SendExecuteResultAsync(result, cancellationToken);
				}
				finally
				{
					Directory.SetCurrentDirectory(prevWorkingDir);
					foreach((string key, string? value) in prevEnvVars)
					{
						Environment.SetEnvironmentVariable(key, value);
					}
				}
			}
			else
			{
				string resolvedExecutable = FileReference.Combine(_sandboxDir, executable).FullName;
				string resolvedCommandLine = CommandLineArguments.Join(arguments);

				Dictionary<string, string> resolvedEnvVars = ManagedProcess.GetCurrentEnvVars();
				if (envVars != null)
				{
					foreach ((string key, string? value) in envVars)
					{
						if (value == null)
						{
							resolvedEnvVars.Remove(key);
						}
						else
						{
							resolvedEnvVars[key] = value;
						}
					}
				}

				using (ManagedProcessGroup group = new ManagedProcessGroup())
				{
					using (ManagedProcess process = new ManagedProcess(group, resolvedExecutable, resolvedCommandLine, resolvedWorkingDir, resolvedEnvVars, null, ProcessPriorityClass.Normal))
					{
						byte[] buffer = new byte[1024];
						for (; ; )
						{
							int length = await process.ReadAsync(buffer, 0, buffer.Length, cancellationToken);
							if (length == 0)
							{
								await channel.SendExecuteResultAsync(process.ExitCode, cancellationToken);
								return;
							}
							await channel.SendExecuteOutputAsync(buffer.AsMemory(0, length), cancellationToken);
						}
					}
				}
			}
		}
	}
}
