// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Net;
using System.Security.Claims;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Horde.Server;
using Horde.Server.Acls;
using Horde.Server.Authentication;
using Horde.Server.Users;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authentication;
using Microsoft.AspNetCore.Authentication.Cookies;
using Microsoft.AspNetCore.Authentication.OpenIdConnect;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

#pragma warning disable CA1054 // URI-like parameters should not be strings

namespace Horde.Server.Server
{
	/// <summary>
	/// Model for Horde account login view
	/// </summary>
	public class HordeAccountLoginViewModel
	{
		/// <summary>
		/// Where to post the form
		/// </summary>
		public string? FormPostUrl { get; set; }
		
		/// <summary>
		/// Optional error message to display
		/// </summary>
		public string? ErrorMessage { get; set; }
	}
	
	/// <summary>
	/// Controller managing account status
	/// </summary>
	[ApiController]
	[Route("[controller]")]
	public class AccountController : Controller
	{
		/// <summary>
		/// Style sheet for HTML responses
		/// </summary>
		const string StyleSheet =
			"body { font-family: 'Segoe UI', 'Roboto', arial, sans-serif; } " +
			"p { margin:20px; font-size:13px; } " +
			"h1 { margin:20px; font-size:32px; font-weight:200; } " +
			"table { margin:10px 20px; } " +
			"td { margin:5px; font-size:13px; }";

		readonly IUserCollection _users;
		readonly IServiceAccountCollection _serviceAccounts;
		readonly string _authenticationScheme;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public AccountController(IUserCollection users, IServiceAccountCollection serviceAccounts, IOptionsMonitor<ServerSettings> serverSettings, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_users = users;
			_serviceAccounts = serviceAccounts;
			_authenticationScheme = GetAuthScheme(serverSettings.CurrentValue.AuthMethod);
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Get auth scheme name for a given auth method
		/// </summary>
		/// <param name="method">Authentication method</param>
		/// <returns>Name of authentication scheme</returns>
		public static string GetAuthScheme(AuthMethod method)
		{
			return method switch
			{
				AuthMethod.Anonymous => AnonymousAuthenticationHandler.AuthenticationScheme,
				AuthMethod.Okta => OktaDefaults.AuthenticationScheme,
				AuthMethod.OpenIdConnect => OpenIdConnectDefaults.AuthenticationScheme,
				AuthMethod.Horde => CookieAuthenticationDefaults.AuthenticationScheme,
				_ => throw new ArgumentOutOfRangeException(nameof(method), method, null)
			};
		}

		/// <summary>
		/// Gets the current login status
		/// </summary>
		/// <returns>The current login state</returns>
		[HttpGet]
		[Route("/account")]
		public ActionResult State()
		{
			StringBuilder content = new StringBuilder();
			content.Append($"<html><style>{StyleSheet}</style><h1>Horde Server</h1>");
			if (User.Identity?.IsAuthenticated ?? false)
			{
				content.Append(CultureInfo.InvariantCulture, $"<p>User <b>{User.Identity?.Name}</b> is logged in. <a href=\"/account/logout\">Log out</a></p>");
				if (_globalConfig.Value.Authorize(AdminAclAction.AdminWrite, User))
				{
					content.Append("<p>");
					content.Append("<a href=\"/api/v1/admin/token\">Get bearer token</a><br/>");
					content.Append("<a href=\"/api/v1/admin/registrationtoken\">Get agent registration token</a><br/>");
					content.Append("<a href=\"/api/v1/admin/softwaretoken\">Get agent software upload token</a><br/>");
					content.Append("<a href=\"/api/v1/admin/softwaredownloadtoken\">Get agent software download token</a><br/>");
					content.Append("<a href=\"/api/v1/admin/configtoken\">Get configuration token</a><br/>");
					content.Append("<a href=\"/api/v1/admin/chainedjobtoken\">Get chained job token</a><br/>");
					content.Append("</p>");
				}
				content.Append(CultureInfo.InvariantCulture, $"<p>Claims for {User.Identity?.Name}:");
				content.Append("<table>");
				foreach (System.Security.Claims.Claim claim in User.Claims)
				{
					content.Append(CultureInfo.InvariantCulture, $"<tr><td>{claim.Type}</td><td>{claim.Value}</td></tr>");
				}
				content.Append("</table>");
				content.Append("</p>");

				content.Append(CultureInfo.InvariantCulture, $"<p>Built from Perforce</p>");
			}
			else
			{
				content.Append("<p><a href=\"/account/login\"><b>Login with OAuth2</b></a></p>");
			}
			content.Append("</html>");
			return new ContentResult { ContentType = "text/html", StatusCode = (int)HttpStatusCode.OK, Content = content.ToString() };
		}

		
		/// <summary>
		/// Show login form for username/password login
		/// </summary>
		/// <returns>HTML for a login form</returns>
		[HttpGet]
		[Route("/account/login/horde")]
		public IActionResult UserPassLoginForm(string? returnUrl = null)
		{
			if (User.Identity is { IsAuthenticated: true })
			{
				// Redirect if already logged in
				return Redirect(returnUrl ?? "/");
			}

			return View("~/Server/HordeAccountLogin.cshtml", new HordeAccountLoginViewModel
			{
				FormPostUrl = Url.Action("UserPassLogin", "Account", returnUrl != null ? new { returnUrl } : null)
			});
		}
		
		/// <summary>
		/// Perform a login with username/password credentials
		/// </summary>
		/// <returns>An HTTP redirect if successful</returns>
		[HttpPost]
		[Route("/account/login/horde")]
		public async Task<IActionResult> UserPassLoginAsync(string? returnUrl = null)
		{
			const string ErrorMsg = "Invalid username or password";
			string? username = Request.Form["username"];
			string? password = Request.Form["password"];

			if (String.IsNullOrEmpty(username) || String.IsNullOrEmpty(password))
			{
				return LoginFormError(ErrorMsg, returnUrl);
			}

			IServiceAccount? account = await _serviceAccounts.GetByLogin(username);
			if (account == null)
			{
				return LoginFormError(ErrorMsg, returnUrl);
			}

			byte[] correctHash = PasswordHasher.HashFromString(account.PasswordHash);
			byte[] salt = PasswordHasher.SaltFromString(account.PasswordSalt);
			if (!PasswordHasher.ValidatePassword(password, salt, correctHash))
			{
				return LoginFormError(ErrorMsg, returnUrl);
			}
			
			if (String.IsNullOrEmpty(account.Email))
			{
				return LoginFormError("E-mail not set for user", returnUrl);
			}

			IUser user = await _users.FindOrAddUserByLoginAsync(account.Login, account.Name, account.Email);
			List<Claim> claims = new()
			{
				new Claim(HordeClaimTypes.Version, HordeClaimTypes.CurrentVersion),
				new Claim(ClaimTypes.Name, account.Name),
				new Claim(ClaimTypes.Email, account.Email),
				new Claim(HordeClaimTypes.User, account.Login),
				new Claim(HordeClaimTypes.UserId, user.Id.ToString()),
			};
			foreach (IUserClaim claim in account.GetClaims())
			{
				claims.Add(new Claim(claim.Type, claim.Value));
			}

			ClaimsIdentity claimsIdentity = new (claims, CookieAuthenticationDefaults.AuthenticationScheme);
			AuthenticationProperties authProperties = new ()
			{
				IsPersistent = true,
				ExpiresUtc = DateTimeOffset.UtcNow.AddDays(7)
			};

			await HttpContext.SignInAsync(
				CookieAuthenticationDefaults.AuthenticationScheme,
				new ClaimsPrincipal(claimsIdentity),
				authProperties);

			return Redirect(returnUrl ?? "/");
		}

		private ViewResult LoginFormError(string message, string? returnUrl = null, HttpStatusCode statusCode = HttpStatusCode.BadRequest)
		{
			Response.StatusCode = (int)statusCode;
			return View("~/Server/HordeAccountLogin.cshtml", new HordeAccountLoginViewModel
			{
				FormPostUrl = Url.Action("UserPassLogin", "Account", returnUrl != null ? new { returnUrl } : null),
				ErrorMessage = message
			});
		}

		/// <summary>
		/// Login to the server
		/// </summary>
		/// <returns>Http result</returns>
		[HttpGet]
		[Route("/account/login")]
		public IActionResult Login()
		{
			return new ChallengeResult(_authenticationScheme, new AuthenticationProperties { RedirectUri = "/account" });
		}

		/// <summary>
		/// Logout of the current account
		/// </summary>
		/// <returns>Http result</returns>
		[HttpGet]
		[Route("/account/logout")]
		public async Task<IActionResult> LogoutAsync()
		{
			await HttpContext.SignOutAsync(CookieAuthenticationDefaults.AuthenticationScheme);
			try
			{
				await HttpContext.SignOutAsync(_authenticationScheme);
			}
			catch
			{
			}

			string content = $"<html><style>{StyleSheet}</style><body onload=\"setTimeout(function(){{ window.location = '/account'; }}, 2000)\"><p>User has been logged out. Returning to login page.</p></body></html>";
			return new ContentResult { ContentType = "text/html", StatusCode = (int)HttpStatusCode.OK, Content = content };
		}
	}
}
