// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.RegularExpressions;
using DotLiquid;

namespace MarkdownSharp.Preprocessor
{
    public class MetadataManager
    {
        public PreprocessedData Data { get; private set; }

        public MetadataManager(PreprocessedData data)
        {
            Data = data;
            MetadataMap = new Dictionary<string, List<string>>();
        }

        private const string MetaRow = @"(?<MetadataKey>[a-zA-Z0-9][0-9a-zA-Z _-]*)[ ]*:[ ]*(?<MetadataValue>.*)";

        private const string MetaRows = MetaRow + @"\n?";

        public static readonly Regex MetadataPattern = new Regex(@"
                        \A                                  #Starts on first line
                        ^                                   #Start of line
                        (" + MetaRows + @")+                 # meta Data = $1
                        (?:\n+|\Z)
                        ", RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        public static readonly Regex MetadataRow = new Regex(@"
                        ^                                   #Start of line
                        " + MetaRow + @"                   # meta Data = $1
                        ", RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        /// <summary>
        /// @UE3 Parse each individual row of meta data
        /// </summary>
        /// <param name="match"></param>
        /// <returns></returns>
        private string ParseMetadataRow(Match match, TransformationData data)
        {
            var metaValue = match.Groups["MetadataValue"].Value;
            var metaDataCategory = match.Groups["MetadataKey"].Value;

            if (metaDataCategory.Contains(' '))
            {
                data.ErrorList.Add(
                    Markdown.GenerateError(
                        Language.Message("MetadataNamesMustNotContainSpaces", metaDataCategory),
                        MessageClass.Warning,
                        metaDataCategory,
                        data.ErrorList.Count,
                        data));
            }

            var metaDataCategoryLowerCase = metaDataCategory.ToLower();
            // If value is blank change to paragraph for table creation classification form
            if (!String.IsNullOrWhiteSpace(match.Groups["MetadataValue"].Value))
            {
                if (metaDataCategoryLowerCase == "title")
                {
                    DocumentTitle = metaValue;
                }

                if (metaDataCategoryLowerCase == "crumbs")
                {
                    CrumbsLinks.Add(metaValue);
                }

                if (metaDataCategoryLowerCase == "related")
                {
                    RelatedLinks.Add(data.Markdown.ProcessRelated(metaValue, data));
                }
            }

            // Add meta data to the list, we require some specific meta data keys to be unique others can be duplicates
            if (metaDataCategoryLowerCase.Equals("title") || metaDataCategoryLowerCase.Equals("description")
                || metaDataCategoryLowerCase.Equals("template") || metaDataCategoryLowerCase.Equals("forcepublishfiles"))
            {
                if (MetadataMap.ContainsKey(metaDataCategoryLowerCase))
                {
                    data.ErrorList.Add(
                        new ErrorDetail(
                            Language.Message("DuplicateMetadataDetected", metaDataCategory),
                            MessageClass.Info,
                            "",
                            "",
                            0,
                            0));
                }
                else
                {
                    var valueList = new List<string>();
                    valueList.Add(match.Groups["MetadataValue"].Value);
                    MetadataMap.Add(metaDataCategoryLowerCase, valueList);
                }
            }
            else
            {
                if (MetadataMap.ContainsKey(metaDataCategoryLowerCase))
                {
                    MetadataMap[metaDataCategoryLowerCase].Add(match.Groups["MetadataValue"].Value);
                }
                else
                {
                    var valueList = new List<string>();
                    valueList.Add(match.Groups["MetadataValue"].Value);
                    MetadataMap.Add(metaDataCategoryLowerCase, valueList);
                }
            }

            // Return empty string, we are removing the meta data from the document
            return "";
        }

        private string ParseMetadataMatches(Match match, TransformationData data)
        {
            var metaDataRows = Regex.Split(match.Groups[0].Value, "\n");

            foreach (var currentMetaDataRow in metaDataRows)
            {
                MetadataRow.Replace(currentMetaDataRow, m => ParseMetadataRow(m, data));
            }

            return "";
        }

        public string ParseMetadata(string text, TransformationData data, PreprocessedData preprocessedData)
        {
            var map = preprocessedData.TextMap;

            CrumbsLinks = new List<string>();
            RelatedLinks = new List<Hash>();

            var changes = new List<PreprocessingTextChange>();

            text = Preprocessor.Replace(MetadataPattern, text, match => ParseMetadataMatches(match, data), null, changes);

            Preprocessor.AddChangesToBounds(map, changes, preprocessedData, PreprocessedTextType.Metadata);

            if (map != null)
            {
                map.ApplyChanges(changes);
            }

            if (Data.Document.LocalPath.Equals(data.Document.LocalPath))
            {
                foreach (
                    var metadataType in
                        Markdown.MetadataErrorIfMissing.Where(
                            metadataType => !MetadataMap.ContainsKey(metadataType.ToLower())))
                {
                    data.ErrorList.Add(
                        new ErrorDetail(
                            Language.Message("MissingMetadata", metadataType), MessageClass.Error, "", "", 0, 0));
                }

                foreach (
                    var metadataType in
                        Markdown.MetadataInfoIfMissing.Where(
                            metadataType => !MetadataMap.ContainsKey(metadataType.ToLower())))
                {
                    data.ErrorList.Add(
                        new ErrorDetail(
                            Language.Message("MissingMetadata", metadataType), MessageClass.Info, "", "", 0, 0));
                }
            }

            foreach (var metadataEntry in MetadataMap)
            {
                Data.Variables.Add(
                    metadataEntry.Key.ToLower(), Templates.MetadataListInVariables.Render(Hash.FromAnonymousObject(
                        new
                        {
                            metadata = metadataEntry.Value
                        })).Trim(), data);
            }

            return text;
        }

        public bool Contains(string name)
        {
            return MetadataMap.ContainsKey(name);
        }

        public List<string> Get(string name)
        {
            return MetadataMap[name];
        }

        public List<string> CrumbsLinks { get; set; }
        public List<Hash> RelatedLinks { get; set; }
        public string DocumentTitle { get; set; }

        public Dictionary<string, List<string>> MetadataMap { get; private set; }
    }
}
