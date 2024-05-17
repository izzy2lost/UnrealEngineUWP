// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Horde;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using Horde.Agent.Utility;
using Horde.Common.Rpc;
using HordeCommon.Rpc;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Driver.Utility
{
	/// <summary>
	/// Logger which updates the job step outcome whenever a warning or error is detected.
	/// </summary>
	class JobStepLogger : IServerLogger
	{
		/// <summary>
		/// The current outcome for this step. Updated to reflect any errors and warnings that occurred.
		/// </summary>
		public JobStepOutcome Outcome => _outcome;

		readonly IHordeClient _hordeClient;
		readonly JobId _jobId;
		readonly JobStepBatchId _jobBatchId;
		readonly JobStepId _jobStepId;

		readonly ServerLogger _inner;
		JobStepOutcome _outcome;
		Task _updateOutcomeTask;
		CancellationTokenSource _cancellationSource;
		ILogger _internalLogger;

		/// <summary>
		/// Constructor
		/// </summary>
		public JobStepLogger(IHordeClient hordeClient, LogId logId, ILogger localLogger, JobId jobId, JobStepBatchId jobBatchId, JobStepId jobStepId, bool? warnings, LogLevel outputLevel, ILogger internalLogger)
		{
			_hordeClient = hordeClient;
			_jobId = jobId;
			_jobBatchId = jobBatchId;
			_jobStepId = jobStepId;
			_inner = new ServerLogger(hordeClient, logId, warnings, outputLevel, localLogger, internalLogger);
			_outcome = JobStepOutcome.Success;
			_updateOutcomeTask = Task.CompletedTask;
			_cancellationSource = new CancellationTokenSource();
			_internalLogger = internalLogger;
		}

		/// <inheritdoc/>
		public IDisposable? BeginScope<TState>(TState state) where TState : notnull
			=> _inner.BeginScope(state);

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			if (!_updateOutcomeTask.IsCompleted)
			{
				_cancellationSource.Cancel();
				await _updateOutcomeTask;
				_cancellationSource.Dispose();
			}

			await _inner.DisposeAsync();
		}

		/// <inheritdoc/>
		public bool IsEnabled(LogLevel logLevel)
			=> _inner.IsEnabled(logLevel);

		/// <inheritdoc/>
		public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter)
		{
			// Forward the log data to the inner writer
			_inner.Log<TState>(logLevel, eventId, state, exception, formatter);

			// Update the state of this job if this is an error status
			JobStepOutcome newOutcome = _outcome;
			if (logLevel == LogLevel.Error || logLevel == LogLevel.Critical)
			{
				newOutcome = JobStepOutcome.Failure;
			}
			else if (logLevel == LogLevel.Warning && Outcome != JobStepOutcome.Failure)
			{
				newOutcome = JobStepOutcome.Warnings;
			}

			// If it changed, create a new task to update the job state on the server
			if (newOutcome != _outcome)
			{
				Task prevUpdateTask = _updateOutcomeTask;
				_updateOutcomeTask = Task.Run(() => UpdateOutcomeAsync(prevUpdateTask, newOutcome, _cancellationSource.Token), _cancellationSource.Token);
			}
		}

		/// <inheritdoc/>
		public async Task StopAsync()
		{
			await _inner.StopAsync();
			await _updateOutcomeTask;
		}

		async Task UpdateOutcomeAsync(Task prevTask, JobStepOutcome newOutcome, CancellationToken cancellationToken)
		{
			// Wait for the last task to complete
			await prevTask;

			// Update the outcome of this jobstep
			try
			{
				JobRpc.JobRpcClient jobRpc = await _hordeClient.CreateGrpcClientAsync<JobRpc.JobRpcClient>(cancellationToken);
				await jobRpc.UpdateStepAsync(new RpcUpdateStepRequest(_jobId, _jobBatchId, _jobStepId, JobStepState.Unspecified, newOutcome), cancellationToken: cancellationToken);
			}
			catch (Exception ex)
			{
				_internalLogger.LogWarning(ex, "Unable to update step outcome to {NewOutcome}", Outcome);
			}
		}
	}
}
