// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute.Buffers;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Logging;
using EpicGames.Horde.Storage;
using System.IO;
using System.Linq;
using EpicGames.Horde.Storage.Clients;
using System.Runtime.ExceptionServices;
using System.Text;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Implements the remote end of a compute worker. 
	/// </summary>
	public class AgentMessageHandler
	{
		readonly DirectoryReference _sandboxDir;
		readonly Dictionary<string, string?> _envVars;
		readonly bool _executeInProcess;
		readonly string? _wineExecutablePath;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="sandboxDir">Directory to use for reading/writing files</param>
		/// <param name="envVars">Environment variables to set for any child processes</param>
		/// <param name="executeInProcess">Whether to execute any external assemblies in the current process</param>
		/// <param name="wineExecutablePath">Path to Wine executable. If null, execution under Wine is disabled</param>
		/// <param name="logger">Logger for diagnostics</param>
		public AgentMessageHandler(DirectoryReference sandboxDir, Dictionary<string, string?>? envVars, bool executeInProcess, string? wineExecutablePath, ILogger logger)
		{
			_sandboxDir = sandboxDir;
			_envVars = envVars ?? new Dictionary<string, string?>();
			_executeInProcess = executeInProcess;
			_wineExecutablePath = wineExecutablePath;
			_logger = logger;
		}

		/// <summary>
		/// Runs the worker using commands sent along the given socket
		/// </summary>
		/// <param name="socket">Socket to read from</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task RunAsync(ComputeSocket socket, CancellationToken cancellationToken)
		{
			// Since we allow forking message channels, we want to ensure that errors on one channel are propagated back here, and terminate the whole connection. 
			// To do that, we take first exception thrown and rethrow it with the original callstack here, while also forcing all other tasks to terminate via a 
			// shared cancellation token.
			using CancellationTokenSource cancellationSource = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
			ExceptionDispatchInfo? exceptionInfo = null;

			void PostException(Exception ex)
			{
				// Capture stack from call site
				Interlocked.CompareExchange(ref exceptionInfo, ExceptionDispatchInfo.Capture(ex), null);
				cancellationSource.Cancel();
			}

			await RunAsync(socket, 0, 4 * 1024 * 1024, PostException, cancellationSource.Token);

			// Throw the regular cancellation exception if requested
			cancellationToken.ThrowIfCancellationRequested();

			// Otherwise throw any exception posted by a child task
#pragma warning disable CA1508 // Static analyzer doesn't understand how this can be non-null
			exceptionInfo?.Throw();
#pragma warning restore CA1508
		}

		async Task RunAsync(ComputeSocket socket, int channelId, int bufferSize, Action<Exception> postException, CancellationToken cancellationToken)
		{
			List<Task> childTasks = new List<Task>();
			try
			{
				using AgentMessageChannel channel = socket.CreateAgentMessageChannel(channelId, bufferSize);
				await channel.AttachAsync(cancellationToken);

				for (; ; )
				{
					using AgentMessage message = await channel.ReceiveAsync(cancellationToken);
					_logger.LogDebug("Compute Channel {ChannelId}: {MessageType}", channelId, message.Type);

					switch (message.Type)
					{
						case AgentMessageType.None:
							return;
						case AgentMessageType.Ping:
							await channel.PingAsync(cancellationToken);
							break;
						case AgentMessageType.Fork:
							{
								ForkMessage fork = message.ParseForkMessage();
								childTasks.Add(Task.Run(() => RunAsync(socket, fork.ChannelId, fork.BufferSize, postException, cancellationToken), cancellationToken));
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
						case AgentMessageType.ExecuteV1:
							{
								ExecuteProcessMessage executeProcess = message.ParseExecuteProcessV1Message();
								await ExecuteProcessAsync(socket, channel, executeProcess.Executable, executeProcess.Arguments, executeProcess.WorkingDir, executeProcess.EnvVars, executeProcess.Flags, cancellationToken);
							}
							break;
						case AgentMessageType.ExecuteV2:
							{
								ExecuteProcessMessage executeProcess = message.ParseExecuteProcessV2Message();
								await ExecuteProcessAsync(socket, channel, executeProcess.Executable, executeProcess.Arguments, executeProcess.WorkingDir, executeProcess.EnvVars, executeProcess.Flags, cancellationToken);
							}
							break;
						case AgentMessageType.XorRequest:
							{
								XorRequestMessage xorRequest = message.AsXorRequest();
								await RunXorAsync(channel, xorRequest.Data, xorRequest.Value, cancellationToken);
							}
							break;
						default:
							throw new InvalidAgentMessageException(message);
					}
				}
			}
			catch (OperationCanceledException ex)
			{
				// Ignore cancellations; we will re-throw from the root RunAsync() method.
				_logger.LogDebug(ex, "Compute Channel {ChannelId}: Cancelled.", channelId);
			}
			catch (Exception ex)
			{
				_logger.LogInformation(ex, "Compute Channel {ChannelId}: Exception: {Message}", channelId, ex.Message);
				postException(ex);
			}
			finally
			{
				await Task.WhenAll(childTasks);
			}
		}

		static async ValueTask RunXorAsync(AgentMessageChannel channel, ReadOnlyMemory<byte> source, byte value, CancellationToken cancellationToken)
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

		async Task WriteFilesAsync(AgentMessageChannel channel, string path, BlobLocator locator, CancellationToken cancellationToken)
		{
			using AgentStorageClient innerStore = new AgentStorageClient(channel);
			await using BundleCache cache = new BundleCache(new BundleCacheOptions { HeaderCacheSize = 10 * 1024 * 1024, PacketCacheSize = 128 * 1024 * 1024 });
			using BundleStorageClient store = new BundleStorageClient(innerStore, cache, _logger);

			IBlobHandle handle = store.CreateBlobHandle(locator);
			DirectoryNode directoryNode = await handle.ReadNodeAsync<DirectoryNode>(cancellationToken);

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

		async Task ExecuteProcessAsync(ComputeSocket socket, AgentMessageChannel channel, string executable, IReadOnlyList<string> arguments, string? workingDir, IReadOnlyDictionary<string, string?>? envVars, ExecuteProcessFlags flags, CancellationToken cancellationToken)
		{
			try
			{
				if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
				{
					await ExecuteProcessWindowsAsync(socket, channel, executable, arguments, workingDir, envVars, flags, cancellationToken);
				}
				else
				{
					await ExecuteProcessInternalAsync(channel, executable, arguments, workingDir, envVars, flags, cancellationToken);
				}
			}
			catch (Exception ex)
			{
				await channel.SendExceptionAsync(ex, cancellationToken);
			}
		}

		async Task ExecuteProcessWindowsAsync(ComputeSocket socket, AgentMessageChannel channel, string executable, IReadOnlyList<string> arguments, string? workingDir, IReadOnlyDictionary<string, string?>? envVars, ExecuteProcessFlags flags, CancellationToken cancellationToken)
		{
			Dictionary<string, string?> newEnvVars = new Dictionary<string, string?>(_envVars);
			if (envVars != null)
			{
				foreach ((string name, string? value) in envVars)
				{
					newEnvVars.Add(name, value);
				}
			}

			await using (WorkerComputeSocketBridge server = await WorkerComputeSocketBridge.CreateAsync(socket, _logger))
			{
				newEnvVars[WorkerComputeSocket.IpcEnvVar] = server.BufferName;

				_logger.LogInformation("Launching {Executable} {Arguments}", CommandLineArguments.Quote(executable), CommandLineArguments.Join(arguments));

				await ExecuteProcessInternalAsync(channel, executable, arguments, workingDir, newEnvVars, flags, cancellationToken);
				_logger.LogInformation("Finished executing process");
			}

			_logger.LogInformation("Child process has shut down");
		}

		internal static async Task ProcessIpcMessagesAsync(ComputeSocket socket, ComputeBufferReader ipcReader, CancellationToken[] cancellationTokens, ILogger logger)
		{
			using CancellationTokenSource cancellationTokenSource = CancellationTokenSource.CreateLinkedTokenSource(cancellationTokens);
			CancellationToken cancellationToken = cancellationTokenSource.Token;

			List <SharedMemoryBuffer> buffers = new();
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
									logger.LogDebug("Attaching send buffer for channel {ChannelId} to {Name}", channelId, name);

									SharedMemoryBuffer buffer = SharedMemoryBuffer.OpenExisting(name);
									buffers.Add(buffer);

									socket.AttachSendBuffer(channelId, buffer);
								}
								break;
							case IpcMessage.AttachRecvBuffer:
								{
									int channelId = (int)reader.ReadUnsignedVarInt();
									string name = reader.ReadString();
									logger.LogDebug("Attaching recv buffer for channel {ChannelId} to {Name}", channelId, name);

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
						logger.LogError(ex, "Exception while processing messages from child process: {Message}", ex.Message);
					}

					ipcReader.AdvanceReadPosition(memory.Length - reader.RemainingMemory.Length);
				}
			}
			catch (OperationCanceledException)
			{
				logger.LogDebug("Ipc message loop cancelled");
			}
			finally
			{
				foreach (SharedMemoryBuffer buffer in buffers)
				{
					buffer.Dispose();
				}
			}
		}

		// Helper class to take raw UTF8 output and merge it into log lines
		class ProcessOutputWriter
		{
			readonly string _prefix;
			readonly ByteArrayBuilder _lineBuffer = new ByteArrayBuilder();
			readonly ILogger _logger;

			public ProcessOutputWriter(string prefix, ILogger logger)
			{
				_prefix = prefix;
				_logger = logger;
			}

			public void WriteBytes(ReadOnlySpan<byte> span)
			{
				for (; ; )
				{
					int newlineIdx = span.IndexOf((byte)'\n');
					if (newlineIdx == -1)
					{
						_lineBuffer.WriteFixedLengthBytes(span);
						break;
					}

					ReadOnlySpan<byte> line = span.Slice(0, newlineIdx);
					if (line.Length > 0 && line[line.Length - 1] == (byte)'\r')
					{
						line = line.Slice(0, line.Length - 1);
					}

					if (_lineBuffer.Length > 0)
					{
						_lineBuffer.WriteFixedLengthBytes(line);
						_logger.LogInformation("{Prefix}: {Line}", _prefix, Encoding.UTF8.GetString(_lineBuffer.AsMemory().Span));
						_lineBuffer.Clear();
					}
					else
					{
						_logger.LogInformation("{Prefix}: {Line}", _prefix, Encoding.UTF8.GetString(line));
					}

					span = span.Slice(newlineIdx + 1);
				}
			}
		}

		async Task ExecuteProcessInternalAsync(AgentMessageChannel channel, string executable, IReadOnlyList<string> arguments, string? workingDir, IReadOnlyDictionary<string, string?>? envVars, ExecuteProcessFlags flags, CancellationToken cancellationToken)
		{
			string resolvedExecutable = FileReference.Combine(_sandboxDir, executable).FullName;
			string resolvedWorkingDir = DirectoryReference.Combine(_sandboxDir, workingDir ?? String.Empty).FullName;
			if (_executeInProcess && Path.GetFileNameWithoutExtension(resolvedExecutable).Equals("dotnet", StringComparison.OrdinalIgnoreCase))
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
					string assemblyPath = FileReference.Combine(_sandboxDir, arguments[0]).FullName;
					string[] mainArgs = arguments.Skip(1).ToArray();

					_logger.LogWarning("Note: Loading and running {Assembly} in process", assemblyPath);

					TaskCompletionSource<int> resultTcs = new TaskCompletionSource<int>();

					Thread thread = new Thread(() => resultTcs.SetResult(AppDomain.CurrentDomain.ExecuteAssembly(assemblyPath, mainArgs)));
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
				string resolvedCommandLine = CommandLineArguments.Join(arguments);

				if (flags.HasFlag(ExecuteProcessFlags.UseWine) && _wineExecutablePath != null)
				{
					// Path to the original Windows executable is prepended to the argument list so Wine can run it
					resolvedCommandLine = CommandLineArguments.Join(new[] { resolvedExecutable }.Concat(arguments).ToList());
					resolvedExecutable = _wineExecutablePath;
				}

				Dictionary<string, string> resolvedEnvVars = ManagedProcess.GetCurrentEnvVars();

				foreach ((string key, string? value) in _envVars)
				{
					if (value != null)
					{
						resolvedEnvVars[key] = value;
					}
				}
				
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

				if (!File.Exists(resolvedExecutable))
				{
					_logger.LogWarning("Executable {Path} does not exist", resolvedExecutable);	
				}
				
				if (!Directory.Exists(resolvedWorkingDir))
				{
					_logger.LogWarning("Working dir {Path} does not exist", resolvedWorkingDir);	
				}

				using ManagedProcessGroup group = new ManagedProcessGroup();
				using ManagedProcess process = new ManagedProcess(group, resolvedExecutable, resolvedCommandLine, resolvedWorkingDir, resolvedEnvVars, null, ProcessPriorityClass.Normal);
				byte[] buffer = new byte[1024];

				ProcessOutputWriter outputWriter = new ProcessOutputWriter($"{Path.GetFileNameWithoutExtension(resolvedExecutable)}> ", _logger);
				for (; ; )
				{
					int length = await process.ReadAsync(buffer, 0, buffer.Length, cancellationToken);
					if (length == 0)
					{
						await process.WaitForExitAsync(cancellationToken);
						await channel.SendExecuteResultAsync(process.ExitCode, cancellationToken);
						return;
					}

					ReadOnlyMemory<byte> output = buffer.AsMemory(0, length);
					await channel.SendExecuteOutputAsync(output, cancellationToken);

					outputWriter.WriteBytes(output.Span);
				}
			}
		}
	}
}
