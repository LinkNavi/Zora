
use anyhow::{bail, Result};
use colored::Colorize;
use std::process::Command;
use crate::config::ProjectConfig;

pub fn run(name: String) -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found");
    }
    
    let config = ProjectConfig::load()?;
    
    if let Some(script) = config.scripts.get(&name) {
        println!("{} Running script: {}", "→".bright_blue(), name);
        
        let status = if cfg!(windows) {
            Command::new("cmd").args(&["/C", script]).status()?
        } else {
            Command::new("sh").args(&["-c", script]).status()?
        };
        
        if !status.success() {
            bail!("Script failed");
        }
        
        println!("{} Script completed", "✓".green().bold());
    } else {
        bail!("Script '{}' not found in project.toml", name);
    }
    
    Ok(())
}

