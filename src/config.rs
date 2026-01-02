use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::Path;

#[derive(Debug, Deserialize, Serialize)]
pub struct ProjectConfig {
    pub name: String,
    pub version: String,
    #[serde(default = "default_project_type")]
    pub r#type: String,
    #[serde(default)]
    pub language: String,
    #[serde(default)]
    pub std: String,
    #[serde(default)]
    pub edition: String,
    #[serde(default)]
    pub authors: Vec<String>,
    #[serde(default)]
    pub sources: SourceConfig,
    #[serde(default)]
    pub includes: IncludeConfig,
    #[serde(default)]
    pub deps: HashMap<String, DependencySpec>,
    #[serde(default)]
    pub dev_deps: HashMap<String, DependencySpec>,
    #[serde(default)]
    pub build: BuildConfig,
    #[serde(default)]
    pub tests: TestConfig,
    #[serde(default)]
    pub scripts: HashMap<String, String>,
    #[serde(default)]
    pub profile: ProfilesConfig,
    #[serde(default)]
    pub features: HashMap<String, Vec<String>>,
    #[serde(default)]
    pub default_features: Vec<String>,
    #[serde(default)]
    pub workspace: Option<WorkspaceConfig>,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
#[serde(untagged)]
pub enum DependencySpec {
    Simple(String),
    Detailed {
        version: String,
        #[serde(default)]
        features: Vec<String>,
        #[serde(default)]
        optional: bool,
        #[serde(default)]
        git: Option<String>,
        #[serde(default)]
        branch: Option<String>,
        #[serde(default)]
        tag: Option<String>,
    },
}

impl DependencySpec {
    pub fn version(&self) -> &str {
        match self {
            DependencySpec::Simple(v) => v,
            DependencySpec::Detailed { version, .. } => version,
        }
    }
}

#[derive(Debug, Deserialize, Serialize, Default)]
pub struct WorkspaceConfig {
    pub members: Vec<String>,
    #[serde(default)]
    pub exclude: Vec<String>,
}

fn default_project_type() -> String {
    "exec".to_string()
}

#[derive(Debug, Deserialize, Serialize, Default)]
pub struct SourceConfig {
    #[serde(default = "default_source_dirs")]
    pub dirs: Vec<String>,
    #[serde(default)]
    pub exclude: Vec<String>,
}

fn default_source_dirs() -> Vec<String> {
    vec!["src".to_string()]
}

#[derive(Debug, Deserialize, Serialize, Default)]
pub struct IncludeConfig {
    #[serde(default = "default_include_dirs")]
    pub dirs: Vec<String>,
}

fn default_include_dirs() -> Vec<String> {
    vec!["include".to_string()]
}

#[derive(Debug, Deserialize, Serialize, Default)]
pub struct BuildConfig {
    #[serde(default)]
    pub flags: Vec<String>,
    #[serde(default)]
    pub defines: HashMap<String, String>,
    #[serde(default)]
    pub libs: Vec<String>,
    #[serde(default)]
    pub lib_dirs: Vec<String>,
    #[serde(default = "default_optimization")]
    pub optimization: String,
    #[serde(default)]
    pub warnings: Vec<String>,
    #[serde(default)]
    pub target: Option<String>,
}

fn default_optimization() -> String {
    "2".to_string()
}

#[derive(Debug, Deserialize, Serialize, Default)]
pub struct ProfilesConfig {
    #[serde(default = "default_dev_profile")]
    pub dev: ProfileConfig,
    #[serde(default = "default_release_profile")]
    pub release: ProfileConfig,
    #[serde(default)]
    pub custom: HashMap<String, ProfileConfig>,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct ProfileConfig {
    #[serde(default = "default_opt_level")]
    pub opt_level: String,
    #[serde(default = "default_debug")]
    pub debug: bool,
    #[serde(default)]
    pub lto: bool,
    #[serde(default)]
    pub strip: bool,
    #[serde(default)]
    pub flags: Vec<String>,
    #[serde(default)]
    pub defines: HashMap<String, String>,
}

fn default_dev_profile() -> ProfileConfig {
    ProfileConfig {
        opt_level: "0".to_string(),
        debug: true,
        lto: false,
        strip: false,
        flags: vec!["-Wall".to_string(), "-Wextra".to_string()],
        defines: HashMap::new(),
    }
}

fn default_release_profile() -> ProfileConfig {
    ProfileConfig {
        opt_level: "3".to_string(),
        debug: false,
        lto: true,
        strip: true,
        flags: vec!["-Wall".to_string(), "-Wextra".to_string(), "-DNDEBUG".to_string()],
        defines: HashMap::new(),
    }
}

fn default_opt_level() -> String {
    "2".to_string()
}

fn default_debug() -> bool {
    false
}

#[derive(Debug, Deserialize, Serialize, Default)]
pub struct TestConfig {
    #[serde(default = "default_test_dirs")]
    pub dirs: Vec<String>,
    #[serde(default)]
    pub framework: String,
    #[serde(default)]
    pub harness: bool,
}

fn default_test_dirs() -> Vec<String> {
    vec!["tests".to_string()]
}

impl ProjectConfig {
    pub fn load() -> Result<Self> {
        let content = fs::read_to_string("project.toml")
            .context("failed to read project.toml")?;
        
        toml::from_str(&content)
            .context("failed to parse project.toml")
    }

    pub fn save(&self) -> Result<()> {
        let content = toml::to_string_pretty(self)
            .context("failed to serialize project.toml")?;
        
        fs::write("project.toml", content)
            .context("failed to write project.toml")
    }

    pub fn exists() -> bool {
        Path::new("project.toml").exists()
    }

    pub fn is_library(&self) -> bool {
        self.r#type == "lib" || self.r#type == "library"
    }

    pub fn is_cpp(&self) -> bool {
        self.language == "cpp" || self.language == "c++"
    }

    pub fn get_profile(&self, mode: &str) -> ProfileConfig {
        match mode {
            "dev" | "debug" => self.profile.dev.clone(),
            "release" => self.profile.release.clone(),
            custom => self.profile.custom.get(custom).cloned()
                .unwrap_or_else(|| self.profile.dev.clone()),
        }
    }

    pub fn enabled_features(&self, features: &[String]) -> Vec<String> {
        let mut enabled = self.default_features.clone();
        enabled.extend_from_slice(features);
        enabled
    }
}
