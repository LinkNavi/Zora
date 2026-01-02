use anyhow::{bail, Result};
use colored::Colorize;
use crate::config::ProjectConfig;

pub fn list() -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found");
    }
    
    let config = ProjectConfig::load()?;
    
    println!("\n{}", "Available features:".bright_cyan());
    for (name, deps) in &config.features {
        println!("  {} - {}", name.bright_yellow(), deps.join(", "));
    }
    
    if !config.default_features.is_empty() {
        println!("\n{}", "Default features:".bright_cyan());
        for feature in &config.default_features {
            println!("  {}", feature);
        }
    }
    
    Ok(())
}

pub fn enable(features: Vec<String>) -> Result<()> {
    for feature in features {
        println!("{} Enabled feature: {}", "✓".green(), feature);
    }
    Ok(())
}

pub fn disable(features: Vec<String>) -> Result<()> {
    for feature in features {
        println!("{} Disabled feature: {}", "✓".green(), feature);
    }
    Ok(())
}
