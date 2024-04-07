// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics.CodeAnalysis;
using System.Security.Cryptography;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Streams;
using Google.Protobuf;
using Google.Protobuf.Collections;

// These partial classes/extensions operate on generated gRPC and Protobuf code.
// Warnings below are disabled to avoid documenting every class touched.
#pragma warning disable CS1591
#pragma warning disable CA1716
namespace HordeCommon.Rpc
{
	partial class RpcProperty
	{
		public RpcProperty(string name, string value)
		{
			Name = name;
			Value = value;
		}

		public RpcProperty(KeyValuePair<string, string> pair)
		{
			Name = pair.Key;
			Value = pair.Value;
		}
	}

	partial class RpcPropertyUpdate
	{
		public RpcPropertyUpdate(string name, string? value)
		{
			Name = name;
			Value = value;
		}
	}

	static class PropertyExtensions
	{
		public static string GetValue(this RepeatedField<RpcProperty> properties, string name)
		{
			return properties.First(x => x.Name == name).Value;
		}

		public static bool TryGetValue(this RepeatedField<RpcProperty> properties, string name, [MaybeNullWhen(false)] out string result)
		{
			RpcProperty? property = properties.FirstOrDefault(x => x.Name == name);
			if (property == null)
			{
				result = null!;
				return false;
			}
			else
			{
				result = property.Value;
				return true;
			}
		}
	}

	partial class RpcGetStreamRequest
	{
		public RpcGetStreamRequest(StreamId streamId)
		{
			StreamId = streamId.ToString();
		}
	}

	partial class RpcUpdateStreamRequest
	{
		public RpcUpdateStreamRequest(StreamId streamId, Dictionary<string, string?> properties)
		{
			StreamId = streamId.ToString();
			Properties.AddRange(properties.Select(x => new RpcPropertyUpdate(x.Key, x.Value)));
		}
	}

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
		public RpcCreateEventRequest(RpcEventSeverity severity, LogId logId, int lineIndex, int lineCount)
		{
			Severity = severity;
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

namespace HordeCommon.Rpc.Messages.Telemetry
{
	partial class RpcAgentMetadataEvent
	{
		/// <summary>
		/// Calculate an agent ID
		/// </summary>
		/// <returns>A unique hash for all fields</returns>
		public long CalculateAgentId()
		{
			using SHA256 sha256 = SHA256.Create();
			using MemoryStream ms = new(200);
			using BinaryWriter bw = new(ms);

			bw.Write(Ip ?? "<empty ip>");
			bw.Write(Hostname ?? "<empty hostname>");
			bw.Write(Region ?? "<empty region>");
			bw.Write(AvailabilityZone ?? "<empty az>");
			bw.Write(Environment ?? "<empty env>");
			bw.Write(AgentVersion ?? "<empty version>");
			bw.Write(Os ?? "<empty os>");
			bw.Write(OsVersion ?? "<empty os version>");
			bw.Write(Architecture ?? "<empty os architecture>");

			foreach (KeyValuePair<string, string> pair in Properties)
			{
				bw.Write(pair.Key ?? "<empty key>");
				bw.Write(pair.Value ?? "<empty value>");
			}

			foreach (string poolId in PoolIds)
			{
				bw.Write(poolId);
			}

			ms.Position = 0;
			byte[] hash = sha256.ComputeHash(ms);
			return BitConverter.ToInt64(hash, 0);
		}
	}
}

#pragma warning restore CA1716
#pragma warning restore CS1591