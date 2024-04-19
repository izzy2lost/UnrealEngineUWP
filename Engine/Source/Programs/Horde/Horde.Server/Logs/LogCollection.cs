// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Sessions;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Storage;
using Horde.Server.Server;
using Horde.Server.Storage;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Server.Logs
{
	/// <summary>
	/// Wrapper around the jobs collection in a mongo DB
	/// </summary>
	public class LogCollection : ILogCollection
	{
		class LogChunkDocument : ILogChunk
		{
			public long Offset { get; set; }
			public int Length { get; set; }
			public int LineIndex { get; set; }

			[BsonIgnoreIfNull]
			public string? Server { get; set; }

			[BsonConstructor]
			public LogChunkDocument()
			{
			}

			public LogChunkDocument(LogChunkDocument other)
			{
				Offset = other.Offset;
				Length = other.Length;
				LineIndex = other.LineIndex;
				Server = other.Server;
			}

			public LogChunkDocument Clone()
			{
				return (LogChunkDocument)MemberwiseClone();
			}
		}

		class LogDocument : ILog
		{
			[BsonRequired, BsonId]
			public LogId Id { get; set; }

			[BsonRequired]
			public JobId JobId { get; set; }

			[BsonIgnoreIfNull]
			public LeaseId? LeaseId { get; set; }

			[BsonIgnoreIfNull]
			public SessionId? SessionId { get; set; }

			public LogType Type { get; set; }
			public bool UseNewStorageBackend { get; set; }

			[BsonIgnoreIfNull]
			public int? MaxLineIndex { get; set; }

			[BsonIgnoreIfNull]
			public long? IndexLength { get; set; }

			public List<LogChunkDocument> Chunks { get; set; } = new List<LogChunkDocument>();

			public int LineCount { get; set; }

			public NamespaceId NamespaceId { get; set; } = Namespace.Logs;
			public RefName RefName { get; set; }

			[BsonIgnoreIfDefault]
			public bool Complete { get; set; }

			[BsonRequired]
			public int UpdateIndex { get; set; }

			IReadOnlyList<ILogChunk> ILog.Chunks => Chunks;

			[BsonConstructor]
			private LogDocument()
			{
			}

			public LogDocument(JobId jobId, LeaseId? leaseId, SessionId? sessionId, LogType type, LogId? logId, NamespaceId namespaceId)
			{
				Id = logId ?? LogIdUtils.GenerateNewId();
				JobId = jobId;
				LeaseId = leaseId;
				SessionId = sessionId;
				Type = type;
				UseNewStorageBackend = true;
				MaxLineIndex = 0;
				NamespaceId = namespaceId;
				RefName = new RefName(Id.ToString());
			}

			public LogDocument Clone()
			{
				LogDocument document = (LogDocument)MemberwiseClone();
				document.Chunks = document.Chunks.ConvertAll(x => x.Clone());
				return document;
			}
		}

		/// <summary>
		/// The jobs collection
		/// </summary>
		readonly IMongoCollection<LogDocument> _logCollection;

		/// <summary>
		/// Hostname for the current server
		/// </summary>
		readonly string _hostName;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service singleton</param>
		public LogCollection(MongoService mongoService)
		{
			_logCollection = mongoService.GetCollection<LogDocument>("LogFiles");
			_hostName = Dns.GetHostName();
		}

		/// <inheritdoc/>
		public async Task<ILog> CreateLogAsync(JobId jobId, LeaseId? leaseId, SessionId? sessionId, LogType type, LogId? logId, CancellationToken cancellationToken)
		{
			LogDocument newLog = new LogDocument(jobId, leaseId, sessionId, type, logId, Namespace.Logs);
			await _logCollection.InsertOneAsync(newLog, null, cancellationToken);
			return newLog;
		}

		/// <inheritdoc/>
		public async Task<ILog> UpdateLineCountAsync(ILog log, int lineCount, bool complete, CancellationToken cancellationToken)
		{
			FilterDefinition<LogDocument> filter = Builders<LogDocument>.Filter.Eq(x => x.Id, log.Id);
			UpdateDefinition<LogDocument> update = Builders<LogDocument>.Update.Set(x => x.LineCount, lineCount).Set(x => x.Complete, complete).Inc(x => x.UpdateIndex, 1);
			return await _logCollection.FindOneAndUpdateAsync(filter, update, new FindOneAndUpdateOptions<LogDocument, LogDocument> { ReturnDocument = ReturnDocument.After }, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<ILog?> TryAddChunkAsync(ILog logInterface, long offset, int lineIndex, CancellationToken cancellationToken)
		{
			LogDocument log = ((LogDocument)logInterface).Clone();

			int chunkIdx = log.Chunks.GetChunkForOffset(offset) + 1;

			LogChunkDocument chunk = new LogChunkDocument();
			chunk.Offset = offset;
			chunk.LineIndex = lineIndex;
			chunk.Server = _hostName;
			log.Chunks.Insert(chunkIdx, chunk);

			UpdateDefinition<LogDocument> update = Builders<LogDocument>.Update.Set(x => x.Chunks, log.Chunks);
			if (chunkIdx == log.Chunks.Count - 1)
			{
				log.MaxLineIndex = null;
				update = update.Unset(x => x.MaxLineIndex);
			}

			if (!await TryUpdateLogAsync(log, update, cancellationToken))
			{
				return null;
			}

			return log;
		}

		/// <inheritdoc/>
		public async Task<ILog?> TryCompleteChunksAsync(ILog logInterface, IEnumerable<CompleteLogChunkUpdate> chunkUpdates, CancellationToken cancellationToken)
		{
			LogDocument log = ((LogDocument)logInterface).Clone();

			// Update the length of any complete chunks
			UpdateDefinitionBuilder<LogDocument> updateBuilder = Builders<LogDocument>.Update;
			List<UpdateDefinition<LogDocument>> updates = new List<UpdateDefinition<LogDocument>>();
			foreach (CompleteLogChunkUpdate chunkUpdate in chunkUpdates)
			{
				LogChunkDocument chunk = log.Chunks[chunkUpdate.Index];
				chunk.Length = chunkUpdate.Length;
				updates.Add(updateBuilder.Set(x => x.Chunks[chunkUpdate.Index].Length, chunkUpdate.Length));

				if (chunkUpdate.Index == log.Chunks.Count - 1)
				{
					log.MaxLineIndex = chunk.LineIndex + chunkUpdate.LineCount;
					updates.Add(updateBuilder.Set(x => x.MaxLineIndex, log.MaxLineIndex));
				}
			}

			// Try to apply the updates
			if (updates.Count > 0 && !await TryUpdateLogAsync(log, updateBuilder.Combine(updates), cancellationToken))
			{
				return null;
			}

			return log;
		}

		/// <inheritdoc/>
		public async Task<ILog?> TryUpdateIndexAsync(ILog logInterface, long newIndexLength, CancellationToken cancellationToken)
		{
			LogDocument log = ((LogDocument)logInterface).Clone();

			UpdateDefinition<LogDocument> update = Builders<LogDocument>.Update.Set(x => x.IndexLength, newIndexLength);
			if (!await TryUpdateLogAsync(log, update, cancellationToken))
			{
				return null;
			}

			log.IndexLength = newIndexLength;
			return log;
		}

		/// <inheritdoc/>
		private async Task<bool> TryUpdateLogAsync(LogDocument current, UpdateDefinition<LogDocument> update, CancellationToken cancellationToken)
		{
			int prevUpdateIndex = current.UpdateIndex;
			current.UpdateIndex++;
			UpdateResult result = await _logCollection.UpdateOneAsync<LogDocument>(x => x.Id == current.Id && x.UpdateIndex == prevUpdateIndex, update.Set(x => x.UpdateIndex, current.UpdateIndex), cancellationToken: cancellationToken);
			return result.ModifiedCount == 1;
		}

		/// <inheritdoc/>
		public async Task<ILog?> GetLogAsync(LogId logId, CancellationToken cancellationToken)
		{
			LogDocument log = await _logCollection.Find<LogDocument>(x => x.Id == logId).FirstOrDefaultAsync(cancellationToken);
			return log;
		}

		/// <inheritdoc/>
		public async Task<List<ILog>> GetLogsAsync(int? index = null, int? count = null, CancellationToken cancellationToken = default)
		{
			IFindFluent<LogDocument, LogDocument> query = _logCollection.Find(FilterDefinition<LogDocument>.Empty);
			if (index != null)
			{
				query = query.Skip(index.Value);
			}
			if (count != null)
			{
				query = query.Limit(count.Value);
			}

			List<LogDocument> results = await query.ToListAsync(cancellationToken);
			return results.ConvertAll<ILog>(x => x);
		}
	}
}
