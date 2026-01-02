use anyhow::{bail, Result};
use colored::Colorize;
use std::fs;
use std::path::PathBuf;
use crate::config::ProjectConfig;

pub fn run(prefix: Option<String>) -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found");
    }
    
    let config = ProjectConfig::load()?;
    let install_prefix = prefix.unwrap_or_else(|| {
        if cfg!(windows) {
            "C:\\Program Files".to_string()
        } else {
            "/usr/local".to_string()
        }
    });
    
    let bin_dir = PathBuf::from(&install_prefix).join("bin");
    let exe_name = if cfg!(windows) {
        format!("{}.exe", config.name)
    } else {
        config.name.clone()
    };
    
    let exe_path = bin_dir.join(&exe_name);
    
    if exe_path.exists() {
        fs::remove_file(&exe_path)?;
        println!("{} Uninstalled {}", "✓".green().bold(), exe_path.display());
    } else {
        println!("{}", "Not installed".yellow());
    }
    
    Ok(())
}
