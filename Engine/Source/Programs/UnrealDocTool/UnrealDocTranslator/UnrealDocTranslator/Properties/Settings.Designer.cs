//------------------------------------------------------------------------------
// <auto-generated>
//     This code was generated by a tool.
//     Runtime Version:4.0.30319.18047
//
//     Changes to this file may cause incorrect behavior and will be lost if
//     the code is regenerated.
// </auto-generated>
//------------------------------------------------------------------------------

namespace UnrealDocTranslator.Properties {
    
    
    [global::System.Runtime.CompilerServices.CompilerGeneratedAttribute()]
    [global::System.CodeDom.Compiler.GeneratedCodeAttribute("Microsoft.VisualStudio.Editors.SettingsDesigner.SettingsSingleFileGenerator", "10.0.0.0")]
    internal sealed partial class Settings : global::System.Configuration.ApplicationSettingsBase {
        
        private static Settings defaultInstance = ((Settings)(global::System.Configuration.ApplicationSettingsBase.Synchronized(new Settings())));
        
        public static Settings Default {
            get {
                return defaultInstance;
            }
        }
        
        [global::System.Configuration.ApplicationScopedSettingAttribute()]
        [global::System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [global::System.Configuration.DefaultSettingValueAttribute("KOR,JPN,CHN")]
        public string TranslationLanguages {
            get {
                return ((string)(this["TranslationLanguages"]));
            }
        }
        
        [global::System.Configuration.ApplicationScopedSettingAttribute()]
        [global::System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [global::System.Configuration.DefaultSettingValueAttribute("Source")]
        public string SourceDirectoryName {
            get {
                return ((string)(this["SourceDirectoryName"]));
            }
        }
        
        [global::System.Configuration.ApplicationScopedSettingAttribute()]
        [global::System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [global::System.Configuration.DefaultSettingValueAttribute("HTML")]
        public string OutputDirectoryName {
            get {
                return ((string)(this["OutputDirectoryName"]));
            }
        }
        
        [global::System.Configuration.ApplicationScopedSettingAttribute()]
        [global::System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [global::System.Configuration.DefaultSettingValueAttribute("//depot/UE4/Engine/Documentation/Source...udn")]
        public string DefaultDepotPath {
            get {
                return ((string)(this["DefaultDepotPath"]));
            }
        }
        
        [global::System.Configuration.ApplicationScopedSettingAttribute()]
        [global::System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [global::System.Configuration.DefaultSettingValueAttribute("C:\\Program Files\\Araxis\\Araxis Merge")]
        public string DefaultAraxisPath {
            get {
                return ((string)(this["DefaultAraxisPath"]));
            }
        }
        
        [global::System.Configuration.UserScopedSettingAttribute()]
        [global::System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [global::System.Configuration.DefaultSettingValueAttribute("#loc UE4Doc{0} moved or deleted")]
        public string MoveDeleteChangelistDescription {
            get {
                return ((string)(this["MoveDeleteChangelistDescription"]));
            }
            set {
                this["MoveDeleteChangelistDescription"] = value;
            }
        }
        
        [global::System.Configuration.ApplicationScopedSettingAttribute()]
        [global::System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [global::System.Configuration.DefaultSettingValueAttribute("//Depot")]
        public string DepotPathMustStartWith {
            get {
                return ((string)(this["DepotPathMustStartWith"]));
            }
        }
        
        [global::System.Configuration.ApplicationScopedSettingAttribute()]
        [global::System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [global::System.Configuration.DefaultSettingValueAttribute("...udn")]
        public string DepotPathMustEndWith {
            get {
                return ((string)(this["DepotPathMustEndWith"]));
            }
        }
        
        [global::System.Configuration.UserScopedSettingAttribute()]
        [global::System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [global::System.Configuration.DefaultSettingValueAttribute("Search Files")]
        public string DefaultSearchBoxText {
            get {
                return ((string)(this["DefaultSearchBoxText"]));
            }
            set {
                this["DefaultSearchBoxText"] = value;
            }
        }
        
        [global::System.Configuration.UserScopedSettingAttribute()]
        [global::System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [global::System.Configuration.DefaultSettingValueAttribute("#loc UE4Doc{0}")]
        public string SubmitChangelistDescription {
            get {
                return ((string)(this["SubmitChangelistDescription"]));
            }
            set {
                this["SubmitChangelistDescription"] = value;
            }
        }
    }
}
