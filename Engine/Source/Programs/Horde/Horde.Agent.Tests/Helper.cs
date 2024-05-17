// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Jobs;
using Google.Protobuf.WellKnownTypes;
using Grpc.Core;
using Horde.Agent.Execution;
using Horde.Common.Rpc;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Agent.Tests
{
	// Stub for fulfilling IOptionsMonitor interface during testing
	// Copied from HordeServerTests until a good way to share code between these is decided.
	public class TestOptionsMonitor<T> : IOptionsMonitor<T>
		where T : class, new()
	{
		public TestOptionsMonitor(T value)
		{
			CurrentValue = value;
		}

		public T CurrentValue { get; }

		public T Get(string? name)
			=> CurrentValue;

		public IDisposable? OnChange(Action<T, string?> listener)
			=> null;
	}

	class JobRpcClientStub : JobRpc.JobRpcClient
	{
		public readonly Queue<RpcBeginStepResponse> BeginStepResponses = new Queue<RpcBeginStepResponse>();
		public readonly List<RpcUpdateStepRequest> UpdateStepRequests = new List<RpcUpdateStepRequest>();
		public readonly Dictionary<RpcGetStepRequest, RpcGetStepResponse> GetStepResponses = new Dictionary<RpcGetStepRequest, RpcGetStepResponse>();
		public Func<RpcGetStepRequest, RpcGetStepResponse>? _getStepFunc = null;
		private readonly ILogger _logger;

		public JobRpcClientStub(ILogger logger)
		{
			_logger = logger;
		}

		public override AsyncUnaryCall<RpcBeginBatchResponse> BeginBatchAsync(RpcBeginBatchRequest request,
			CallOptions options)
		{
			_logger.LogDebug("HordeRpcClientStub.BeginBatchAsync()");
			RpcBeginBatchResponse res = new RpcBeginBatchResponse();

			res.AgentType = "agentType1";
			res.LogId = "logId1";
			res.Change = 1;

			return Wrap(res);
		}

		public override AsyncUnaryCall<Empty> FinishBatchAsync(RpcFinishBatchRequest request, CallOptions options)
		{
			Empty res = new Empty();
			return Wrap(res);
		}

		public override AsyncUnaryCall<RpcGetJobResponse> GetJobAsync(RpcGetJobRequest request, CallOptions options)
		{
			RpcGetJobResponse res = new RpcGetJobResponse();
			return Wrap(res);
		}

		public override AsyncUnaryCall<RpcBeginStepResponse> BeginStepAsync(RpcBeginStepRequest request, CallOptions options)
		{
			if (BeginStepResponses.Count == 0)
			{
				RpcBeginStepResponse completeRes = new RpcBeginStepResponse();
				completeRes.State = RpcBeginStepResponse.Types.Result.Complete;
				return Wrap(completeRes);
			}

			RpcBeginStepResponse res = BeginStepResponses.Dequeue();
			res.State = RpcBeginStepResponse.Types.Result.Ready;
			return Wrap(res);
		}

		public override AsyncUnaryCall<Empty> UpdateStepAsync(RpcUpdateStepRequest request, CallOptions options)
		{
			_logger.LogDebug("UpdateStepAsync(Request: {Request})", request);
			UpdateStepRequests.Add(request);
			Empty res = new Empty();
			return Wrap(res);
		}

		[Obsolete("Use LogRpc.CreateLogEvents instead")]
		public override AsyncUnaryCall<Empty> CreateEventsAsync(RpcCreateLogEventsRequest request, CallOptions options)
		{
			_logger.LogDebug("CreateEventsAsync: {Request}", request);
			Empty res = new Empty();
			return Wrap(res);
		}

		public override AsyncUnaryCall<RpcGetStepResponse> GetStepAsync(RpcGetStepRequest request, CallOptions options)
		{
			if (_getStepFunc != null)
			{
				return Wrap(_getStepFunc(request));
			}

			if (GetStepResponses.TryGetValue(request, out RpcGetStepResponse? res))
			{
				return Wrap(res);
			}

			return Wrap(new RpcGetStepResponse());
		}

		public static AsyncUnaryCall<T> Wrap<T>(T res)
		{
			return new AsyncUnaryCall<T>(Task.FromResult(res), Task.FromResult(Metadata.Empty),
				() => Status.DefaultSuccess, () => Metadata.Empty, null!);
		}
	}

	class SimpleTestExecutor : IJobExecutor
	{
		public const string Name = "Simple";

		private readonly Func<JobStepInfo, ILogger, CancellationToken, Task<JobStepOutcome>> _func;

		public SimpleTestExecutor(Func<JobStepInfo, ILogger, CancellationToken, Task<JobStepOutcome>> func)
		{
			_func = func;
		}

		public void Dispose()
		{
		}

		public Task InitializeAsync(ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogDebug("SimpleTestExecutor.InitializeAsync()");
			return Task.CompletedTask;
		}

		public Task<JobStepOutcome> RunAsync(JobStepInfo step, ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogDebug("SimpleTestExecutor.RunAsync(Step: {Step})", step);
			return _func(step, logger, cancellationToken);
		}

		public Task FinalizeAsync(ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogDebug("SimpleTestExecutor.FinalizeAsync()");
			return Task.CompletedTask;
		}
	}

	class SimpleTestExecutorFactory : IJobExecutorFactory
	{
		readonly IJobExecutor _executor;

		public string Name => SimpleTestExecutor.Name;

		public SimpleTestExecutorFactory(IJobExecutor executor)
		{
			_executor = executor;
		}

		public IJobExecutor CreateExecutor(RpcAgentWorkspace? workspaceInfo, RpcAgentWorkspace? autoSdkWorkspaceInfo, JobExecutorOptions options) => _executor;
	}
}