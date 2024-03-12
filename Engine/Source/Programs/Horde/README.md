![Horde](Docs/Images/Logo.png#gh-light-mode-only)![Horde](Docs/Images/Logo-DarkMode.png#gh-dark-mode-only)

**Horde** is a set of services that support workflows that Epic uses to develop Fortnite,
Unreal Engine, and other titles.

It is provided with full source code to Unreal Engine licensees, and is meant for licensees to host and configure
themselves. We provide pre-built Docker images for deployment on Linux, and an MSI installer for Windows.

Horde provides the following functionality, each of which may be enabled or disabled independently:

* **[Remote Execution](Docs/Tutorials/RemoteCompilation.md)**: Functionality to offload compute work to other machines,
  including C++ compilation with **Unreal Build Accelerator**.
* **[Build Automation (CI/CD)](Docs/Tutorials/BuildAutomation.md)**: A build automation system designed for teams working
  with large Perforce repositories.
* **[Studio Analytics](Docs/Config/Analytics.md)**: Receives telemetry from the Unreal Editor, and shows charts for
  key workflow metrics.
* **[UnrealGameSync Metadata Server](Docs/Config/UgsMetadataServer.md)**: Various features for teams using
  UnrealGameSync, including build status reporting, comment aggregation, and crowdsourced build health functionality.
* **[Device Manager](Docs/Config/Devices.md)**: A system for allocating and managing a farm of development kits and
  mobile devices.
* **[Automation Hub](Docs/Config/AutomationHub.md)**: A frontend for querying automation results across streams and
  projects, integrated with AutomationTool and Gauntlet.

Read more about our [goals and philosophy](Docs/Goals.md), or check out the [FAQ](Docs/Faq.md).

## Status

Horde is under heavy development, and large parts of it are still in flux. While we use aspects of it (particularly
the CI system) heavily at Epic, we consider it experimental for Unreal Engine licensees and offer limited support
for it.

See also: [Feature Status Page](Docs/Features.md)

## Getting Started

* **[Installing Horde](Docs/Tutorials/InstallHorde.md)**
* **[Set up remote C++ compilation](Docs/Tutorials/RemoteCompilation.md)**
* **[Set up build automation](Docs/Tutorials/BuildAutomation.md)**

## Reference

Horde's reference documentation is divided into sections by target audience:

* [**Deploying Horde**](Docs/Deployment.md)
  * Information on the architecture and components making up Horde, and best practices for deploying them.<br>
  **Audience:** IT, sysadmins, coders intending to modify Horde.
* [**Configuring and Operating Horde**](Docs/Config.md)
  * Describes how to set up and administer Horde.<br>
  **Audience:** Build/dev ops teams, admins.
* [**Horde Internals**](Docs/Internals.md)
  * Describes how to build and modify Horde, and its architecture.<br>
  **Audience:** Developers wishing to extend Horde.

## Further Reading

* [Glossary](Docs/Glossary.md)
