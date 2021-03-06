// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Web.Mvc;

namespace Tools.CrashReporter.CrashReportWebSite.Models
{
	/// <summary>
	/// A helper class to extract parameters passed from web pages
	/// </summary>
	public class FormHelper
	{
		/// <summary>The user search query.</summary>
		public string SearchQuery = "";
		/// <summary>The page to display from the list.</summary>
		public int Page = 1;
		/// <summary>The number of items to display per page.</summary>
		public int PageSize = 100;
		/// <summary>The term to sort by. e.g. TTPID.</summary>
		public string SortTerm = "";
		/// <summary>The types of crashes we wish to see.</summary>
		public string CrashType = "CrashesAsserts";
		/// <summary>Whether to sort ascending or descending.</summary>
		public string SortOrder = "Descending";
		/// <summary>The previous sort order.</summary>
		public string PreviousOrder = "";
		/// <summary>The user group to filter by.</summary>
		public string UserGroup = "General";
		/// <summary>The earliest date we are interested in.</summary>
		public DateTime DateFrom = DateTime.UtcNow;
		/// <summary>The most recent date we are interested in.</summary>
		public DateTime DateTo = DateTime.UtcNow;
		/// <summary>The branch name to filter by.</summary>
		public string BranchName = "";
		/// <summary>The game to filter by.</summary>
		public string GameName = "";

		private string PreviousTerm = "";

		/// <summary>
		/// Intelligently extract parameters from a web request that could be in a form or a query string (GET vs. POST).
		/// </summary>
		/// <param name="Request">The owning HttpRequest.</param>
		/// <param name="Form">The form passed to the controller.</param>
		/// <param name="Key">The parameter we wish to retrieve a value for.</param>
		/// <param name="DefaultValue">The default value to set if the parameter is null or empty.</param>
		/// <param name="Result">The value of the requested parameter.</param>
		/// <returns>true if the parameter was found to be not null or empty.</returns>
		private bool GetFormParameter( HttpRequestBase Request, FormCollection Form, string Key, string DefaultValue, out string Result )
		{
			Result = DefaultValue;

			string Value = "";
			if( Form.Count == 0 )
			{
				Value = Request.QueryString[Key];
			}
			else
			{
				Value = Form[Key];
			}

			if( !string.IsNullOrEmpty( Value ) )
			{
				Result = Value.Trim();
				return true;
			}

			return false;
		}

		/// <summary>
		/// Attempt to read a form item as a date in milliseconds since 1970
		/// </summary>
		/// <param name="Request">The request that contains parameters if the form does not.</param>
		/// <param name="Form">The form that contains parameters if the request does not.</param>
		/// <param name="Key">Name of item in form/request</param>
		/// <param name="Date">Result, only modified on succes</param>
		void TryParseDate(HttpRequestBase Request, FormCollection Form, string Key, ref DateTime Date)
		{
			string MillisecondsString;
			if (!GetFormParameter(Request, Form, Key, "", out MillisecondsString))
				return;

			try
			{
				Date = CrashesViewModel.Epoch.AddMilliseconds(long.Parse(MillisecondsString));
			}
			catch (FormatException)
			{
			}
		}

		/// <summary>
		/// Extract all the parameters from the client user input.
		/// </summary>
		/// <param name="Request">The request that contains parameters if the form does not.</param>
		/// <param name="Form">The form that contains parameters if the request does not.</param>
		/// <param name="DefaultSortTerm">The default sort term to use.</param>
		public FormHelper( HttpRequestBase Request, FormCollection Form, string DefaultSortTerm )
		{
			// Set up Default values if there is no QueryString and set values to the Query string if it is there.
			GetFormParameter( Request, Form, "SearchQuery", SearchQuery, out SearchQuery );

			GetFormParameter( Request, Form, "SortTerm", DefaultSortTerm, out SortTerm );

			GetFormParameter( Request, Form, "CrashType", CrashType, out CrashType );

			string PageString = Page.ToString();
			if( GetFormParameter( Request, Form, "Page", PageString, out PageString ) )
			{
				if( !int.TryParse( PageString, out Page ) || Page < 1 )
				{
					Page = 1;
				}
			}

			string PageSizeString = PageSize.ToString();
			if( GetFormParameter( Request, Form, "PageSize", PageSizeString, out PageSizeString ) )
			{
				if( !int.TryParse( PageSizeString, out PageSize ) || PageSize < 1 )
				{
					PageSize = 100;
				}
			}

			GetFormParameter( Request, Form, "SortOrder", SortOrder, out SortOrder );

			GetFormParameter( Request, Form, "PreviousOrder", PreviousOrder, out PreviousOrder );

			GetFormParameter( Request, Form, "PreviousTerm", PreviousTerm, out PreviousTerm );

			GetFormParameter( Request, Form, "UserGroup", UserGroup, out UserGroup );

			GetFormParameter( Request, Form, "BranchName", BranchName, out BranchName );

			GetFormParameter( Request, Form, "GameName", GameName, out GameName );

			DateFrom = DateTime.Today.AddDays(-7);
			TryParseDate(Request, Form, "DateFrom", ref DateFrom);
			DateFrom = DateFrom.ToUniversalTime();

			DateTo = DateTime.Today;
			TryParseDate(Request, Form, "DateTo", ref DateTo);
			DateTo = DateTo.ToUniversalTime();

			// Set the sort order 
			if( PreviousOrder == "Descending" && PreviousTerm == SortTerm )
			{
				SortOrder = "Ascending";
			}
			else if( PreviousOrder == "Ascending" && PreviousTerm == SortTerm )
			{
				SortOrder = "Descending";
			}
			else if( string.IsNullOrEmpty( PreviousOrder ) )
			{
				// keep SortOrder Where it's at.
			}
			else
			{
				SortOrder = "Descending";
			}
		}
	}
}