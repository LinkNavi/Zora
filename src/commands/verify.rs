
use anyhow::{bail, Result};
use colored::Colorize;
use crate::config::ProjectConfig;
use std::path::Path;

pub fn run(locked: bool) -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found");
    }
    
    println!("{}", "Verifying project integrity...".bright_cyan());
    
    let config = ProjectConfig::load()?;
    
    // Check lock file
    if locked && !Path::new("project.lock").exists() {
        bail!("project.lock not found. Run without --locked or generate lock file");
    }
    
    // Check source files exist
    for dir in &config.sources.dirs {
        if !Path::new(dir).exists() {
            bail!("Source directory not found: {}", dir);
        }
    }
    
    // Check include files exist
    for dir in &config.includes.dirs {
        if !Path::new(dir).exists() {
            bail!("Include directory not found: {}", dir);
        }
    }
    
    println!("{} Project verified", "✓".green().bold());
    Ok(())
}

