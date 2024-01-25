[Horde](../Home.md) > Getting Started: Installing the Horde Server

# Getting Started: Installing the Horde Server

## Introduction

This guide describes a simple local Horde server installation on Windows.

Horde can also be installed via Docker on Linux, both as a single instance and horizontally-scaled service via a
container orchestration system such as Kubernetes. For more detailed discussion of these advanced deployment scenarios,
see [Horde > Deployment](../Deployment.md).

## Prerequisites

* Windows machine with admin access (required for service installation)

## Steps

1. Download MSI
2. Run the installer. This will create and start a Horde Server service running under a local user account, which
  will spawn a bundled MongoDB and Redis instance at startup. The installer will prompt for a port to expose the server
  on; you may need to allow this port in your firewall to allow other machines to connect to it.
3. Open a web browser and navigate to `http://localhost:5000` (or whichever port you entered during installation).
