[Horde](../Home.md) > [Configuration](../Config.md) > Continuous Integration

# Continuous Integration

**Continous Integration** (CI) was the first use case targeted for Horde at Epic, and the most mature. 

It is designed to support [**BuildGraph**](https://docs.unrealengine.com/5.0/en-US/buildgraph-for-unreal-engine/) as a first class citizen, allowing efficient distributed and parallelized build pipelines to be scripted, with automatic tracking and transfer of intermediate build artifacts between agents.

It also supports the following features:

* Build Health
* Perforce metadata caching, workspace managemnt
* Support for autoscaling based on job queues
* Support for structured logging, with many common UE-types automatically annotated with additional metadata.
* Profiling and telemetry functionality

CI works hand-in-hand with Horde's remote execution capabilities to distribute compute-heavy workloads between multiple agents, and the storage functionality to archive and retrieve cached, intermediate and final artifacts.

## Introduction

Horde uses BuildGraph to script build pipelines, using Perforce as a VCS. 

BuildGraph describes the build pipeline as a parameterized graph, with each node in the graph performing some set of sequential operation on a set of inputs (in the form of files produced by another node), and producing a set of outputs (in the form of a set of files). One or more nodes execute sequentially on an agent with a synced Peforce workspace. 

When running a job on Horde, you specify a BuildGraph script, any command line arguments to pass to it, and the name of one or more nodes to evaluate. Horde takes care of provisioning machines, syncing from Perforce, and transferring inputs and outputs to temporary storage.

## Enabling CI functionality

In order to enable CI functionality in Horde, you will need to perform the following steps:

* Write a BuildGraph script and submit it to source control.
* Define a project in the [`Globals.json`](Config-Global.md) file which includes a [`*.project.json`](Config-Project.md) configuration file.
* Define a stream in the [`.project.json`](Config-Project.md) file which includes a [`*.stream.json`](Config-Stream.md) configuration file.
* Declare an [`agent type`](#agent-types) which defines a machine that can execute steps in your BuildGraph script.
* Declare a [`job template`](#job-templates) which defines parameters for your BuildGraph script, and references it in source control.

## Agent Types

Agent type specify the actual machines desired to execute a particular type of work, given a name specified in a BuildGraph script. This layer of indirection can be useful when running the same BuildGraph script in multiple streams, or parameterizing the same BuildGraph script to run for different projects.

For ease of management, agents are typically grouped into static or dynamic [pools](#pools) for including in an agent type definition. Agent types can also reference a [workspace](#workspaces) that specifies which files need to be synced on the machine in order to execute a job. Agents configured for use in a particular stream will keep a 'hot' sync of the data from that stream, allowing them to start servicing a job more quickly when required, so filtering the files to sync can reduce disk space and allow higher packing of workspaces per machine.

For more information on configuring agent types, see the [.stream.json reference](Config-Stream.md#agenttypeconfig).

## Job Templates

A job template describes the parameters and default arguments required to run a job, as well as the widgets the Horde dashboard can use to present these parameters to the user. A typical template is as follows:

		{
			"id": "editor-only",
			"name": "Editor Only",
			"arguments": [
				"-Target=Editor Only",
				"-Script=Engine/Restricted/NotForLicensees/Build/DevStreams.xml"
			],
			"parameters": [
				{
					"type": "List",
					"style": "List",
					"label": "Toolchain Versions",
					"items": [
						{
							"text": "Latest",
							"argumentIfEnabled": "-set:WithLatest=true",
							"argumentIfDisabled": "-set:WithLatest=false",
							"default": true
						},
						{
							"text": "Preview",
							"argumentIfEnabled": "-set:WithPreview=true",
							"argumentIfDisabled": "-set:WithPreview=false",
							"default": true
						},
						{
							"text": "Clang",
							"argumentIfEnabled": "-set:WithClang=true",
							"argumentIfDisabled": "-set:WithClang=false",
							"default": true
						}
					]
				}
			]
		}

### Parameters

Various widget types can be specified in the parameters block:

#### Text

Allows entering arbitrary text for an argument, with an optional regex for validation. 

TODO: Screenshot
TODO: Example Script

See [TextParameterData](../Schema/Streams.md#textparameterdata) for valid properties.

#### List

Allows the user to select one or more options from a predefined list. 

TODO: Screenshot
TODO: Example Script

See [ListParameterData](../Schema/Streams.md#listparameterdata) for valid properties. 

#### Bool

Allows toggling whether to enable an option or not.

TODO: Screenshot
TODO: Example Script

See [BoolParameterData](../Schema/Streams.md#boolparameterdata) for valid properties. 

### Schedules

Templates may also specify a schedule and policy on which to trigger automatically; running for every submitted change, for the last `n` changes, whenever certain files are modified, and so on. For more information, see the [ScheduleConfig](../Schema/Streams.md#scheduleconfig) section in the config reference.

## Workspaces

Workspace definitions tell an agent what and how to sync data from source control to execute a job.

The `view` property allows refinining the stream definition with an additional set of filters to apply to the stream view, with wildcards specified in standard Perforce syntax.

The `incremental` property allows definining incremental workspaces. By default, Horde will return a workspace to a pristine state prior to running a job. If this flag is set, any *tracked files* will be synced back to a pristine state (ie. any modified files, as determined by timestamp, will be reset to their original version), but other files in the workspace will be left alone. This allows keeping intermediate state, such as compile artifacts, when tools are robust enough in their dependency tracking to determine whether files need to be rebuilt.

## AutoSDK

* TODO

## Links

* [Build Health](CI/BuildHealth.md)
