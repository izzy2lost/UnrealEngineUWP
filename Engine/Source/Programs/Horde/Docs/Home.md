# Horde

## Introduction

**Horde** is a set of services built for teams working with Unreal Engine, designed around golden-path workflows that have proved valuable for teams at Epic.

## Features

The Horde platform has two key pillars; **compute** (ie. managing farms of machines, scheduling and assigning out work) and **storage** (ie. providing a repository for storing bulk data).

On this foundation, Horde provides the following services:

* **Continuous Integration (CI)**: A build automation system designed for teams working with large Perforce repositories.
* **Remote Execution**: Functionality to offload compute work to other machines, including C++ compilation and content manipulation.
* **Derived Data Cache (DDC)**: Cache for derived-data, such as textures and meshes produced during cooking.
* **UGS Metadata Service**: Various features for teams using UnrealGameSync, including build status reporting, comment aggregation, and crowdsourced voting functionality.
* **Device Manager**: A system for allocating and managing a farm of development kits and mobile devices.
* **Automation Hub**: A frontend for querying automation results across streams and projects, integrated with AutomationTool and Gauntlet.
* **Artifact Service**: A storage backend for tools and final build artifacts.
* **Studio Telemetry**: Receives telemetry from the Unreal Editor, and shows charts for key workflow metrics.
 
Horde is provided with full source code to all Unreal Engine licensees, and is meant for licensees to host and configure themselves. We provide pre-built Docker images for deployment on Linux, and an MSI installer for Windows.

Read more about our [goals and philosophy](Goals.md), or check out the [FAQ](Faq.md).

## Status

Horde is under heavy development, and large parts of it are still in flux. While we use aspects of it (particularly the CI system) heavily at Epic, we don't consider it polished enough to fully advocate and support licensees using it, though teams are welcome to try using it and provide feedback.

## Reference

Horde documentation is divided into sections focusing of aspects of Horde of interest to different groups.

* [**Deploying Horde**](Deployment.md)
   * Information on the architecture and components making up Horde, and best practices for deploying them.
   * **Target audience:** IT, sysadmins, coders intending to modify Horde.
* [**Configuring and Operating Horde**](Config.md)
   * Describes how to set up and administer Horde.
   * **Target audience:** Build/dev ops teams, admins.

## Further Reading

* [Glossary](Glossary.md)