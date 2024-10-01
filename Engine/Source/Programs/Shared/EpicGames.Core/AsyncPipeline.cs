// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Runtime.ExceptionServices;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;

namespace EpicGames.Core
{
	/// <summary>
	/// Manages a set of async tasks forming a pipeline, which can be cancelled and awaited as a complete unit. The first exception thrown by an individual task is captured and reported back to the main thread.
	/// </summary>
	public sealed class AsyncPipeline : IAsyncDisposable
	{
		readonly List<Task> _tasks;
		readonly CancellationToken _cancellationToken;
		CancellationTokenSource _cancellationSource;
		ExceptionDispatchInfo? _exceptionDispatchInfo;

		/// <summary>
		/// Tests whether the pipeline has failed
		/// </summary>
		public bool IsFaulted
			=> _exceptionDispatchInfo != null;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the pipeline</param>
		public AsyncPipeline(CancellationToken cancellationToken)
		{
			_tasks = new List<Task>();
			_cancellationToken = cancellationToken;
			_cancellationSource = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			if (_cancellationSource != null)
			{
				await _cancellationSource.CancelAsync();
			}

			if (_tasks.Count > 0)
			{
				await Task.WhenAll(_tasks);
				_tasks.Clear();
			}

			if(_cancellationSource != null)
			{
				_cancellationSource.Dispose();
				_cancellationSource = null!;
			}
		}

		/// <summary>
		/// Adds a new task to the pipeline.
		/// </summary>
		/// <param name="taskFunc">Method to execute</param>
		public void AddTask(Func<CancellationToken, Task> taskFunc)
		{
			_tasks.Add(Task.Run(() => RunGuardedAsync(taskFunc), _cancellationSource.Token));
		}

		/// <summary>
		/// Adds several tasks to the pipeline.
		/// </summary>
		/// <param name="count">Number of tasks to run</param>
		/// <param name="taskFunc">Method to execute</param>
		public void AddTasks(int count, Func<CancellationToken, Task> taskFunc)
		{
			for (int idx = 0; idx < count; idx++)
			{
				AddTask(taskFunc);
			}
		}

		async Task RunGuardedAsync(Func<CancellationToken, Task> taskFunc)
		{
			try
			{
				await taskFunc(_cancellationSource.Token);
			}
			catch (OperationCanceledException)
			{
				// Ignore
			}
			catch (Exception ex)
			{
				if (_exceptionDispatchInfo == null)
				{
					ExceptionDispatchInfo dispatchInfo = ExceptionDispatchInfo.Capture(ex);
					Interlocked.CompareExchange(ref _exceptionDispatchInfo, dispatchInfo, null);
				}
				await _cancellationSource.CancelAsync();
			}
		}

		/// <summary>
		/// Waits for all tasks to complete, and throws any exceptions 
		/// </summary>
		public async Task WaitForCompletionAsync()
		{
			await Task.WhenAll(_tasks);

			_cancellationToken.ThrowIfCancellationRequested();

			if (_exceptionDispatchInfo != null)
			{
				_exceptionDispatchInfo.Throw();
			}
		}
	}

	/// <summary>
	/// Extension methods for async pipelines
	/// </summary>
	public static class AsyncPipelineExtensions
	{
		/// <summary>
		/// Adds a worker to process items from a channel
		/// </summary>
		/// <typeparam name="T">Item type</typeparam>
		/// <param name="pipeline">Pipeline to add the worker to</param>
		/// <param name="reader">Source for the items</param>
		/// <param name="taskFunc">Action to execute for each item</param>
		public static void AddTask<T>(this AsyncPipeline pipeline, ChannelReader<T> reader, Func<T, CancellationToken, Task> taskFunc)
		{
			pipeline.AddTask(ctx => ProcessItemsAsync(reader, taskFunc, ctx));
		}

		/// <summary>
		/// Adds a worker to process items from a channel
		/// </summary>
		/// <typeparam name="T">Item type</typeparam>
		/// <param name="pipeline">Pipeline to add the worker to</param>
		/// <param name="count">Number of workers to add</param>
		/// <param name="reader">Source for the items</param>
		/// <param name="taskFunc">Action to execute for each item</param>
		public static void AddTasks<T>(this AsyncPipeline pipeline, int count, ChannelReader<T> reader, Func<T, CancellationToken, Task> taskFunc)
		{
			for (int idx = 0; idx < count; idx++)
			{
				AddTask(pipeline, reader, taskFunc);
			}
		}

		static async Task ProcessItemsAsync<T>(ChannelReader<T> reader, Func<T, CancellationToken, Task> taskFunc, CancellationToken cancellationToken)
		{
			while (await reader.WaitToReadAsync(cancellationToken))
			{
				T? item;
				if (reader.TryRead(out item))
				{
					await taskFunc(item, cancellationToken);
				}
			}
		}
	}
}
