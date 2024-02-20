[Horde](../Home.md) > [Configuration](../Config.md) > Permissions

# Permissions

## Authentication

Horde offers three ways to do authentication and authorization of users:

* Anonymous
* OpenID Connect
* Built-in user accounts

The mode is configured via the `AuthMethod` setting

### Anonymous
Horde ships with authorization disabled by default for demonstration purposes and to get started.
For any production deployment, it's required to configure proper authentication by any of the two methods below.

### OpenID Connect
Horde can use an external _OpenID Connect (OIDC)_ provider for auth,
see the [server deployment](../Deployment/Server.md) documentation for information about configuring an OIDC provider.
OIDC is recommended for studios where a central authentication provider is already in use, such as Google Workspaces, Okta, or Azure AD/Entra ID.

After an OIDC provider is configured, a user's claims may be viewed by navigating to the
`http://{{ server_url }}/account` page in a browser.

### Built-in User Accounts
If you are a smaller studio or don't see the need for using OpenID Connect method, Horde's built-in user accounts is an option.
These accounts are managed by Horde itself and stored in the local database.
With the server in anonymous mode, you can set up user accounts via the web UI (Server dropdown in top right).
Configure these with at least one administrator user and set `AuthMethod` to `Horde`. 

## Access Control Lists

Access to entities in Horde is controlled by **access control lists (ACLs)**. Each item in the list grants the ability
to perform certain actions to any users with specific OIDC claims. Each claim is a key/value pair that is returned by
the OIDC provider or synthesized by Horde at login.
See [ACL Actions](../Config/Schema/AclActions.md) page for a complete list of actions available.

Many objects that users can query or manipulate have an attached ACL which exists within a hierarchy of other
ACL-controlled objects. A _stream_ is part of a _project_, for example, and users can be granted entitlements to view
that specific Perforce stream (via the ACL on that stream's configuration), for all streams within the project (via the ACL on
the project's configuration), or for all streams on the server (via the ACL on the global configuration).

### Administrators

Admin users are permitted to perform any operations regardless of any configured ACLs. Users are granted admin status
if they contain a particular claim configured in the server's [Server.json](../Deployment/ServerSettings.md) file
via the `AdminClaimType` and `AdminClaimValue` properties.

### Synthesized Claims

Horde adds several claims to the configured claims returned through the OIDC provider:

| Name | Description |
| ---- | ----------- |
| `http://epicgames.com/ue/horde/user` | Real name of the user. This is extracted from claims returned by the OIDC provider according to the `OidcClaimNameMapping` [server setting](../Deployment/Server.md). |
| `http://epicgames.com/ue/horde/user-id-v3` | Identifier for the user. This is a 24-character unique id assigned by Horde. |
| `http://epicgames.com/ue/horde/agent` | Identififes a particular agent (with the value being the agent id). |
| `http://epicgames.com/ue/horde/perforce-user` | Gives the Perforce username corresponding to the user | 

### Example

The following config fragment declares an ACL which:

* Grants the `ViewJob` and `CreateJob` entitlement to a user by the name of `Tim Sweeney`. 
* Grants the `ViewJob` entitlement to any users with the role claim of `app-horde-users`.

---

    "acl":
    {
        "entries": [
            {
                "claim": {
                    "type": "http://epicgames.com/ue/horde/user",
                    "value": "Tim Sweeney"
                },
                "actions": [
                    "ViewJob",
                    "CreateJob"
                ]
            },
            {
                "claim": {
                    "type": "http://schemas.microsoft.com/ws/2008/06/identity/claims/role",
                    "value": "app-horde-viewers"
                },
                "actions": [
                    "ViewJob"
                ]
            }
        ],
        "inherit": true
    }

---
