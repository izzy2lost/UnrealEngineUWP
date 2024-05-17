// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using Google.Protobuf;

// These partial classes/extensions operate on generated gRPC and Protobuf code.
// Warnings below are disabled to avoid documenting every class touched.
#pragma warning disable CS1591
#pragma warning disable CA1716
namespace HordeCommon.Rpc
{
	partial class RpcGetJobRequest
	{
		public RpcGetJobRequest(JobId jobId)
		{
			JobId = jobId.ToString();
		}
	}

	partial class RpcBeginBatchRequest
	{
		public RpcBeginBatchRequest(JobId jobId, JobStepBatchId batchId, LeaseId leaseId)
		{
			JobId = jobId.ToString();
			BatchId = batchId.ToString();
			LeaseId = leaseId.ToString();
		}
	}

	partial class RpcFinishBatchRequest
	{
		public RpcFinishBatchRequest(JobId jobId, JobStepBatchId batchId, LeaseId leaseId)
		{
			JobId = jobId.ToString();
			BatchId = batchId.ToString();
			LeaseId = leaseId.ToString();
		}
	}

	partial class RpcBeginStepRequest
	{
		public RpcBeginStepRequest(JobId jobId, JobStepBatchId batchId, LeaseId leaseId)
		{
			JobId = jobId.ToString();
			BatchId = batchId.ToString();
			LeaseId = leaseId.ToString();
		}
	}

	partial class RpcUpdateStepRequest
	{
		public RpcUpdateStepRequest(JobId jobId, JobStepBatchId batchId, JobStepId stepId, JobStepState state, JobStepOutcome outcome)
		{
			JobId = jobId.ToString();
			BatchId = batchId.ToString();
			StepId = stepId.ToString();
			State = (int)state;
			Outcome = (int)outcome;
		}
	}

	partial class RpcGetStepRequest
	{
		public RpcGetStepRequest(JobId jobId, JobStepBatchId batchId, JobStepId stepId)
		{
			JobId = jobId.ToString();
			BatchId = batchId.ToString();
			StepId = stepId.ToString();
		}
	}

	partial class RpcGetStepResponse
	{
		public RpcGetStepResponse(JobStepOutcome outcome, JobStepState state, bool abortRequested)
		{
			Outcome = (int)outcome;
			State = (int)state;
			AbortRequested = abortRequested;
		}
	}

	partial class RpcCreateEventRequest
	{
		public RpcCreateEventRequest(LogEventSeverity severity, LogId logId, int lineIndex, int lineCount)
		{
			Severity = (int)severity;
			LogId = logId.ToString();
			LineIndex = lineIndex;
			LineCount = lineCount;
		}
	}

	partial class RpcCreateEventsRequest
	{
		public RpcCreateEventsRequest(IEnumerable<RpcCreateEventRequest> events)
		{
			Events.AddRange(events);
		}
	}

	partial class RpcWriteOutputRequest
	{
		public RpcWriteOutputRequest(LogId logId, long offset, int lineIndex, ByteString data, bool flush)
		{
			LogId = logId.ToString();
			Offset = offset;
			LineIndex = lineIndex;
			Data = data;
			Flush = flush;
		}
	}

	partial class RpcDownloadSoftwareRequest
	{
		public RpcDownloadSoftwareRequest(string version)
		{
			Version = version;
		}
	}
}

#pragma warning restore CA1716
#pragma warning restore CS1591