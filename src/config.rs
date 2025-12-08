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
    pub sources: SourceConfig,
    #[serde(default)]
    pub includes: IncludeConfig,
    #[serde(default)]
    pub deps: HashMap<String, String>,
    #[serde(default)]
    pub build: BuildConfig,
    #[serde(default)]
    pub tests: TestConfig,
    #[serde(default)]
    pub scripts: HashMap<String, String>,
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
}

fn default_optimization() -> String {
    "2".to_string()
}

#[derive(Debug, Deserialize, Serialize, Default)]
pub struct TestConfig {
    #[serde(default = "default_test_dirs")]
    pub dirs: Vec<String>,
    #[serde(default)]
    pub framework: String,
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
}