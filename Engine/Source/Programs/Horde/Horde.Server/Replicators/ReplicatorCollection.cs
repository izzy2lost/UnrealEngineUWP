// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using MongoDB.Bson;
using MongoDB.Driver;
using Horde.Server.Server;
using EpicGames.Horde.Replicators;
using System.Threading;
using MongoDB.Bson.Serialization.Attributes;
using HordeCommon;
using MongoDB.Bson.Serialization;
using Horde.Server.Utilities;

namespace Horde.Server.Replicators
{
	/// <summary>
	/// Default implementation of <see cref="IReplicatorCollection"/>.
	/// </summary>
	class ReplicatorCollection : IReplicatorCollection
	{
		class ReplicatorDoc
		{
			public ReplicatorId Id { get; set; }

			[BsonElement("lc")]
			public int? LastChange { get; set; }

			[BsonElement("lct")]
			public DateTime? LastChangeFinishTime { get; set; }

			[BsonElement("cc")]
			public int? CurrentChange { get; set; }

			[BsonElement("cct")]
			public DateTime? CurrentChangeStartTime { get; set; }

			[BsonElement("err")]
			public string? Error { get; set; }

			[BsonElement("idx")]
			public int UpdateIndex { get; set; }
		}

		class Replicator : IReplicator
		{
			readonly ReplicatorCollection _collection;
			readonly ReplicatorDoc _document;

			public Replicator(ReplicatorCollection collection, ReplicatorDoc document)
			{
				_collection = collection;
				_document = document;
			}

			public ReplicatorId Id => _document.Id;
			public int? LastChange => _document.LastChange;
			public DateTime? LastChangeFinishTime => _document.LastChangeFinishTime;
			public int? CurrentChange => _document.CurrentChange;
			public DateTime? CurrentChangeStartTime => _document.CurrentChangeStartTime;
			public string? Error => _document.Error;

			public Task<IReplicator?> RefreshAsync(CancellationToken cancellationToken = default)
				=> _collection.GetAsync(Id, cancellationToken);

			public Task<bool> TryDeleteAsync(CancellationToken cancellationToken = default)
				=> _collection.TryDeleteAsync(_document, cancellationToken);

			public async Task<IReplicator?> TryUpdateAsync(UpdateReplicatorOptions options, CancellationToken cancellationToken = default)
			{
				ReplicatorDoc? newDoc = await _collection.TryUpdateAsync(_document, options, cancellationToken);
				return (newDoc == null) ? null : new Replicator(_collection, newDoc);
			}
		}

		readonly IMongoCollection<ReplicatorDoc> _documents;
		readonly IClock _clock;

		static ReplicatorCollection()
		{
			BsonClassMap.RegisterClassMap<ReplicatorId>(cm =>
			{
				cm.MapProperty(x => x.StreamId).SetElementName("sid");
				cm.MapProperty(x => x.StreamReplicatorId).SetElementName("rid");
			});
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public ReplicatorCollection(MongoService mongoService, IClock clock)
		{
			_documents = mongoService.GetCollection<ReplicatorDoc>("Replicators");
			_clock = clock;
		}

		/// <inheritdoc/>
		public async Task<List<IReplicator>> FindAsync(CancellationToken cancellationToken = default)
		{
			List<ReplicatorDoc> documents = await _documents.Find(FilterDefinition<ReplicatorDoc>.Empty, null).ToListAsync(cancellationToken);
			return documents.ConvertAll<IReplicator>(x => new Replicator(this, x));
		}

		/// <inheritdoc/>
		public async Task<IReplicator?> GetAsync(ReplicatorId id, CancellationToken cancellationToken = default)
		{
			ReplicatorDoc? document = await _documents.Find(x => x.Id == id).FirstOrDefaultAsync(cancellationToken);
			if (document == null)
			{
				return null;
			}
			return new Replicator(this, document);
		}

		/// <inheritdoc/>
		public async Task<IReplicator> GetOrAddAsync(ReplicatorId id, CreateReplicatorOptions? replicator, CancellationToken cancellationToken = default)
		{
			DateTime utcNow = _clock.UtcNow;

			ReplicatorDoc? document;
			for (; ; )
			{
				document = await _documents.Find(x => x.Id == id).FirstOrDefaultAsync(cancellationToken);
				if (document != null)
				{
					break;
				}

				document = new ReplicatorDoc();
				document.Id = id;
				document.LastChange = replicator?.LastChange;
				document.LastChangeFinishTime = (document.LastChange == null) ? null : utcNow;
				document.CurrentChange = replicator?.CurrentChange;
				document.CurrentChangeStartTime = (document.CurrentChange == null) ? null : utcNow;
				document.Error = replicator?.Error;
				document.UpdateIndex = 1;

				if (await _documents.InsertOneIgnoreDuplicatesAsync(document, cancellationToken))
				{
					break;
				}
			}

			return new Replicator(this, document);
		}

		/// <inheritdoc/>
		async Task<bool> TryDeleteAsync(ReplicatorDoc replicator, CancellationToken cancellationToken)
		{
			DeleteResult result = await _documents.DeleteOneAsync(x => x.Id == replicator.Id && x.UpdateIndex == replicator.UpdateIndex, cancellationToken);
			return result.DeletedCount > 0;
		}

		/// <inheritdoc/>
		async Task<ReplicatorDoc?> TryUpdateAsync(ReplicatorDoc current, UpdateReplicatorOptions options, CancellationToken cancellationToken = default)
		{
			List<UpdateDefinition<ReplicatorDoc>> updates = new List<UpdateDefinition<ReplicatorDoc>>();

			if (options.NewLastChange != null)
			{
				if (options.NewLastChange.Value == 0)
				{
					updates.Add(Builders<ReplicatorDoc>.Update.Unset(x => x.LastChange).Unset(x => x.LastChangeFinishTime));
				}
				else
				{
					updates.Add(Builders<ReplicatorDoc>.Update.Set(x => x.LastChange, options.NewLastChange).Set(x => x.LastChangeFinishTime, _clock.UtcNow));
				}
			}

			if (options.NewCurrentChange != null)
			{
				if (options.NewCurrentChange.Value == 0)
				{
					updates.Add(Builders<ReplicatorDoc>.Update.Unset(x => x.CurrentChange).Unset(x => x.CurrentChangeStartTime));
				}
				else
				{
					updates.Add(Builders<ReplicatorDoc>.Update.Set(x => x.CurrentChange, options.NewCurrentChange.Value).Set(x => x.CurrentChangeStartTime, _clock.UtcNow));
				}
			}

			if (options.NewError != null)
			{
				if (options.NewError.Length == 0)
				{
					updates.Add(Builders<ReplicatorDoc>.Update.Unset(x => x.Error));
				}
				else
				{
					updates.Add(Builders<ReplicatorDoc>.Update.Set(x => x.Error, options.NewError));
				}
			}

			updates.Add(Builders<ReplicatorDoc>.Update.Inc(x => x.UpdateIndex, 1));

			UpdateDefinition<ReplicatorDoc> update = Builders<ReplicatorDoc>.Update.Combine(updates);
			return await _documents.FindOneAndUpdateAsync<ReplicatorDoc>(x => x.Id == current.Id && x.UpdateIndex == current.UpdateIndex, update, new FindOneAndUpdateOptions<ReplicatorDoc, ReplicatorDoc> { ReturnDocument = ReturnDocument.After }, cancellationToken);
		}
	}
}
