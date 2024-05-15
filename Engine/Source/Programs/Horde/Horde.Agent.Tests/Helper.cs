// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Jobs;
using Google.Protobuf.WellKnownTypes;
using Grpc.Core;
using Grpc.Net.Client;
using Horde.Agent.Execution;
using Horde.Agent.Utility;
using Horde.Common.Rpc;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
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

	class RpcClientRefStub<TClient> : IRpcClientRef<TClient> where TClient : ClientBase<TClient>
	{
		public GrpcChannel Channel { get; }
		public TClient Client { get; }
		public Task DisposingTask { get; }

		public RpcClientRefStub(GrpcChannel channel, TClient client)
		{
			Channel = channel;
			Client = client;
			DisposingTask = new TaskCompletionSource<bool>().Task;
		}

		public void Dispose()
		{
		}
	}

	class RpcConnectionStub : IRpcConnection
	{
		private readonly GrpcChannel _grpcChannel;
		private readonly HordeRpc.HordeRpcClient _hordeRpcClient;
		private readonly JobRpc.JobRpcClient _jobRpcClient;

		public bool Healthy => true;
		public ILogger Logger => NullLogger.Instance;

		public RpcConnectionStub(GrpcChannel grpcChannel, HordeRpc.HordeRpcClient hordeRpcClient, JobRpc.JobRpcClient jobRpcClient)
		{
			_grpcChannel = grpcChannel;
			_hordeRpcClient = hordeRpcClient;
			_jobRpcClient = jobRpcClient;
		}

		public IRpcClientRef<TClient>? TryGetClientRef<TClient>() where TClient : ClientBase<TClient>
		{
			return (IRpcClientRef<TClient>)(object)new RpcClientRefStub<HordeRpc.HordeRpcClient>(_grpcChannel, _hordeRpcClient);
		}

		public Task<IRpcClientRef<TClient>> GetClientRefAsync<TClient>(CancellationToken cancellationToken) where TClient : ClientBase<TClient>
		{
			IRpcClientRef<TClient> rpcClientRefStub;
			if (typeof(TClient) == typeof(HordeRpc.HordeRpcClient))
			{
				rpcClientRefStub = (IRpcClientRef<TClient>)(object)new RpcClientRefStub<HordeRpc.HordeRpcClient>(_grpcChannel, _hordeRpcClient);
			}
			else if (typeof(TClient) == typeof(JobRpc.JobRpcClient))
			{
				rpcClientRefStub = (IRpcClientRef<TClient>)(object)new RpcClientRefStub<JobRpc.JobRpcClient>(_grpcChannel, _jobRpcClient);
			}
			else
			{
				throw new NotImplementedException();
			}
			return Task.FromResult(rpcClientRefStub);
		}

		public ValueTask DisposeAsync()
		{
			return new ValueTask();
		}
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

		public override AsyncUnaryCall<RpcGetStreamResponse> GetStreamAsync(RpcGetStreamRequest request, CallOptions options)
		{
			RpcGetStreamResponse res = new RpcGetStreamResponse();
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

		public override AsyncUnaryCall<Empty> CreateEventsAsync(RpcCreateEventsRequest request, CallOptions options)
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