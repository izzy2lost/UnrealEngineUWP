// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Projects;
using EpicGames.Horde.Streams;
using HordeServer.Jobs;
using HordeServer.Jobs.Graphs;
using HordeServer.Jobs.Templates;
using HordeServer.Projects;
using HordeServer.Server;
using HordeServer.Streams;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace HordeServer.Tests.Jobs
{
	[TestClass]
	public class JobExpirationTests : TestSetup
	{
		async Task<IJob> CreateJobAsync()
		{
			ProjectId projectId = new ProjectId("ue5");
			StreamId streamId = new StreamId("ue5-main");
			TemplateId templateId = new TemplateId("template1");

			StreamConfig streamConfig = new StreamConfig { Id = streamId };
			streamConfig.JobOptions.ExpireAfterDays = 2;
			streamConfig.Templates.Add(new TemplateRefConfig { Id = templateId, Name = "Test Template" });

			ProjectConfig projectConfig = new ProjectConfig { Id = projectId };
			projectConfig.Streams.Add(streamConfig);

			GlobalConfig globalConfig = new GlobalConfig();
			globalConfig.Projects.Add(projectConfig);

			SetConfig(globalConfig);

			CreateJobOptions options = new CreateJobOptions();
			options.PreflightChange = 999;

			ITemplate template = await TemplateCollection.GetOrAddAsync(streamConfig.Templates[0]);

			IGraph graph = await GraphCollection.AddAsync(template, null);

			return await JobService.CreateJobAsync(null, streamConfig, templateId, template.Hash, graph, "Hello", 1234, 1233, options);
		}

		[TestMethod]
		public async Task TestJobExpiryAsync()
		{
			await ServiceProvider.GetRequiredService<JobExpirationService>().StartAsync(default);

			IJob job = await CreateJobAsync();

			IJob? newJob = await JobCollection.GetAsync(job.Id);
			Assert.IsNotNull(newJob);

			await Clock.AdvanceAsync(TimeSpan.FromDays(1.0));

			newJob = await JobCollection.GetAsync(job.Id);
			Assert.IsNotNull(newJob);

			await Clock.AdvanceAsync(TimeSpan.FromDays(2.0));

			newJob = await JobCollection.GetAsync(job.Id);
			Assert.IsNull(newJob);
		}
	}
}
