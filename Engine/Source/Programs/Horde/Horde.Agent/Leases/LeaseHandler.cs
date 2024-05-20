// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using EpicGames.Core;
using EpicGames.Horde.Agents.Leases;
using Google.Protobuf;
using Google.Protobuf.Reflection;
using Google.Protobuf.WellKnownTypes;
using Horde.Agent.Services;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Leases
{
	/// <summary>
	/// Handles execution of a specific lease type
	/// </summary>
	abstract class LeaseHandler
	{
		/// <summary>
		/// Returns protobuf type urls for the handled message types
		/// </summary>
		public abstract string LeaseType { get; }

		/// <summary>
		/// Executes a lease
		/// </summary>
		/// <returns>Result for the lease</returns>
		public abstract Task<LeaseResult> ExecuteAsync(ISession session, LeaseId leaseId, Any message, ILogger logger, CancellationToken cancellationToken);

		/// <summary>
		/// Runs a child process, piping the output to the given logger
		/// </summary>
		/// <param name="executable">Executable to launch</param>
		/// <param name="arguments">Command line arguments for the new process</param>
		/// <param name="environment">Environment for the new process</param>
		/// <param name="logger">Logger for output</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Exit code of the process</returns>
		protected static async Task<int> RunProcessAsync(string executable, IEnumerable<string> arguments, IReadOnlyDictionary<string, string>? environment, ILogger logger, CancellationToken cancellationToken)
		{
			string commandLine = CommandLineArguments.Join(arguments);
			logger.LogInformation("Running child process with arguments: {CommandLine}", commandLine);

			using (ManagedProcessGroup processGroup = new ManagedProcessGroup())
			using (ManagedProcess process = new ManagedProcess(processGroup, executable, commandLine, null, environment, ProcessPriorityClass.Normal))
			{
				for (; ; )
				{
					string? line = await process.ReadLineAsync(cancellationToken);
					if (line == null)
					{
						break;
					}

					JsonLogEvent jsonLogEvent;
					if (JsonLogEvent.TryParse(line, out jsonLogEvent))
					{
						logger.LogJsonLogEvent(jsonLogEvent);
					}
					else
					{
						logger.LogInformation("{Line}", line);
					}
				}

				await process.WaitForExitAsync(CancellationToken.None);
				return process.ExitCode;
			}
		}

		/// <summary>
		/// Runs a .NET assembly as a child process, piping the output to the given logger
		/// </summary>
		/// <param name="entryAssembly">Assembly to launch</param>
		/// <param name="arguments">Command line arguments for the new process</param>
		/// <param name="environment">Environment for the new process</param>
		/// <param name="useNativeHost">Whether to use a native host process</param>
		/// <param name="logger">Logger for output</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Exit code of the process</returns>
		protected static async Task<int> RunDotNetProcessAsync(FileReference entryAssembly, IEnumerable<string> arguments, IReadOnlyDictionary<string, string>? environment, bool useNativeHost, ILogger logger, CancellationToken cancellationToken)
		{
			if (useNativeHost)
			{
				FileReference nativeHost = entryAssembly.ChangeExtension(OperatingSystem.IsWindows() ? ".exe" : null);
				return await RunProcessAsync(nativeHost.FullName, arguments, environment, logger, cancellationToken);
			}
			else
			{
				IEnumerable<string> allArguments = arguments.Prepend(entryAssembly.FullName);
				return await RunProcessAsync("dotnet", allArguments, environment, logger, cancellationToken);
			}
		}
	}

	/// <summary>
	/// Implementation of <see cref="LeaseHandler"/> for a specific lease type
	/// </summary>
	/// <typeparam name="T">Type of the lease message</typeparam>
	abstract class LeaseHandler<T> : LeaseHandler where T : IMessage<T>, new()
	{
		/// <summary>
		/// Static for the message type descriptor
		/// </summary>
		public static MessageDescriptor Descriptor { get; } = new T().Descriptor;

		/// <inheritdoc/>
		public override string LeaseType { get; } = $"type.googleapis.com/{Descriptor.Name}";

		/// <inheritdoc/>
		public override Task<LeaseResult> ExecuteAsync(ISession session, LeaseId leaseId, Any message, ILogger logger, CancellationToken cancellationToken)
		{
			return ExecuteAsync(session, leaseId, message.Unpack<T>(), logger, cancellationToken);
		}

		/// <inheritdoc/>
		public abstract Task<LeaseResult> ExecuteAsync(ISession session, LeaseId leaseId, T message, ILogger logger, CancellationToken cancellationToken);
	}
}
