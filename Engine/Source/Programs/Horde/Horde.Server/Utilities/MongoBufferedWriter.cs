// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using MongoDB.Driver;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Utilities
{
	/// <summary>
	/// Utility class for buffering document  writes to a Mongo collection
	/// </summary>
	sealed class MongoBufferedWriter<TDocument> : IAsyncDisposable
		where TDocument : class
	{
		readonly IMongoCollection<TDocument> _collection;
		readonly int _flushCount;
		readonly TimeSpan _flushTime;
		readonly ConcurrentQueue<TDocument> _queue = new ConcurrentQueue<TDocument>();
		readonly BackgroundTask _backgroundTask;
		readonly AsyncEvent _newDataEvent = new AsyncEvent();
		readonly AsyncEvent _flushEvent = new AsyncEvent();
		readonly ILogger _logger;

		public MongoBufferedWriter(IMongoCollection<TDocument> collection, ILogger logger)
			: this(collection, 50, TimeSpan.FromSeconds(5.0), logger)
		{
		}

		public MongoBufferedWriter(IMongoCollection<TDocument> collection, int flushCount, TimeSpan flushTime, ILogger logger)
		{
			_collection = collection;
			_flushCount = flushCount;
			_flushTime = flushTime;
			_backgroundTask = new BackgroundTask(BackgroundFlushAsync);
			_logger = logger;
		}

		public async ValueTask DisposeAsync()
		{
			await _backgroundTask.DisposeAsync();
		}

		public ValueTask StartAsync()
		{
			_backgroundTask.Start();
			return new ValueTask();
		}

		public async ValueTask StopAsync(CancellationToken cancellationToken)
		{
			await _backgroundTask.StopAsync(cancellationToken);
		}

		// Flushes the sink in the background
		async Task BackgroundFlushAsync(CancellationToken cancellationToken)
		{
			Task newDataTask = _newDataEvent.Task;
			Task flushTask = _flushEvent.Task;

			while (!cancellationToken.IsCancellationRequested)
			{
				await newDataTask.WaitAsync(cancellationToken);
				await Task.WhenAny(flushTask, Task.Delay(_flushTime, cancellationToken));

				newDataTask = _newDataEvent.Task;
				flushTask = _flushEvent.Task;

				await FlushAsync(cancellationToken);
			}
		}

		/// <inheritdoc/>
		public async ValueTask FlushAsync(CancellationToken cancellationToken)
		{
			// Copy all the event documents from the queue
			List<TDocument> document = new List<TDocument>(_queue.Count);
			while (_queue.TryDequeue(out TDocument? eventDocument))
			{
				document.Add(eventDocument);
			}

			// Insert them into the database
			if (document.Count > 0)
			{
				_logger.LogInformation("Writing {NumEvents} new telemetry events to {CollectionName}.", document.Count, _collection.CollectionNamespace.CollectionName);
				await _collection.InsertManyAsync(document, cancellationToken: cancellationToken);
			}
		}

		/// <inheritdoc/>
		public void Write(TDocument document)
		{
			_queue.Enqueue(document);
			_newDataEvent.Set();

			if (_queue.Count > _flushCount)
			{
				_flushEvent.Set();
			}
		}
	}
}
