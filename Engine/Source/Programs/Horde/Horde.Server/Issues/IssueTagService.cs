// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Streams;
using Horde.Server.Commits;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Bson.Serialization.Options;

namespace Horde.Server.Issues
{
	/// <summary>
	/// Polls revision control for fix changelist numbers in commits using the syntax '#horde 1234'
	/// </summary>
	public sealed class IssueTagService : IHostedService, IAsyncDisposable
	{
		[SingletonDocument("issue-tags")]
		class State : SingletonBase
		{
			[BsonElement("streams"), BsonDictionaryOptions(DictionaryRepresentation.ArrayOfDocuments)]
			public Dictionary<StreamId, int> Streams { get; set; } = new Dictionary<StreamId, int>();
		}

		readonly ISingletonDocument<State> _state;
		readonly ICommitService _commitService;
		readonly IIssueCollection _issueCollection;
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly ITicker _ticker;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public IssueTagService(MongoService mongoService, ICommitService commitService, IIssueCollection issueCollection, IClock clock, IOptionsMonitor<GlobalConfig> globalConfig, ILogger<IssueTagService> logger)
		{
			_state = new SingletonDocument<State>(mongoService);
			_commitService = commitService;
			_issueCollection = issueCollection;
			_globalConfig = globalConfig;
			_ticker = clock.AddSharedTicker<IssueTagService>(TimeSpan.FromSeconds(30.0), TickAsync, logger);
			_logger = logger;
		}

		/// <inheritdoc/>
		public ValueTask DisposeAsync() => _ticker.DisposeAsync();

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken) => _ticker.StartAsync();

		/// <inheritdoc/>
		public Task StopAsync(CancellationToken cancellationToken) => _ticker.StopAsync();

		async ValueTask TickAsync(CancellationToken cancellationToken)
		{
			using CancellationTokenSource cancellationSource = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
			using IDisposable? listener = _globalConfig.OnChange((_, _) => cancellationSource.Cancel());

			State initialState = await _state.GetAsync(cancellationSource.Token);

			GlobalConfig globalConfig = _globalConfig.CurrentValue;
			if (globalConfig.Streams.Count > 0)
			{
				List<Task> tasks = new List<Task>();
				try
				{
					foreach (StreamConfig streamConfig in globalConfig.Streams)
					{
						tasks.Add(Task.Run(() => TickStreamGuardedAsync(streamConfig, initialState, cancellationSource.Token), cancellationSource.Token));
					}
					await Task.WhenAny(tasks);
				}
				finally
				{
					try
					{
						await cancellationSource.CancelAsync();
						await Task.WhenAll(tasks);
					}
					catch
					{
					}
				}
			}
		}

		async Task TickStreamGuardedAsync(StreamConfig streamConfig, State initialState, CancellationToken cancellationToken)
		{
			try
			{
				await TickStreamAsync(streamConfig, initialState, cancellationToken);
			}
			catch (OperationCanceledException)
			{
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Exception while scanning for #horde tags: {Message}", ex.Message);
			}
		}

		async ValueTask TickStreamAsync(StreamConfig streamConfig, State initialState, CancellationToken cancellationToken)
		{
			ICommitCollection commits = _commitService.GetCollection(streamConfig);

			int minChange;
			if (!initialState.Streams.TryGetValue(streamConfig.Id, out minChange))
			{
				minChange = await commits.GetLatestNumberAsync(cancellationToken);
			}

			await foreach (ICommit commit in commits.SubscribeAsync(minChange + 1, null, cancellationToken))
			{
				_logger.LogDebug("Checking commit {Change} in {StreamId}", commit.Number, streamConfig.Id);
				foreach (int issueId in ParseTags(_globalConfig.CurrentValue.IssueFixedTag, commit.Description))
				{
					for (; ; )
					{
						IIssue? issue = await _issueCollection.GetIssueAsync(issueId, cancellationToken);
						if (issue == null)
						{
							_logger.LogInformation("Commit {Change} by {Author} in {StreamId} has invalid issue id {IssueId}", commit.Number, commit.AuthorId, streamConfig.Id, issueId);
							break;
						}

						issue = await _issueCollection.TryUpdateIssueAsync(issue, commit.AuthorId, newFixChange: commit.Number, newResolvedById: commit.AuthorId, cancellationToken: cancellationToken);
						if (issue != null)
						{
							_logger.LogInformation("Commit {Change} by {Author} in {StreamId} fixes issue id {IssueId}", commit.Number, commit.AuthorId, streamConfig.Id, issueId);
							break;
						}
					}
				}
				await _state.UpdateAsync(x => x.Streams[streamConfig.Id] = commit.Number, cancellationToken);
			}
		}

		internal static IEnumerable<int> ParseTags(string issueFixedTag, string description)
		{
			if (!Regex.IsMatch(description, @"^\s*#ROBOMERGE-SOURCE", RegexOptions.Multiline))
			{
				foreach (Match match in Regex.Matches(description, $"^\\s*{issueFixedTag}\\s+(.*)$", RegexOptions.Multiline))
				{
					string[] issues = match.Groups[1].Value.Split(new[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries);
					foreach (string issue in issues)
					{
						if (Int32.TryParse(issue, System.Globalization.NumberStyles.None, null, out int issueId))
						{
							yield return issueId;
						}
					}
				}
			}
		}
	}
}

