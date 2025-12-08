// src/commands/info.rs
use anyhow::{bail, Result};
use colored::Colorize;

use crate::config::ProjectConfig;

pub fn run() -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found. Run 'zora init' first.");
    }

    let config = ProjectConfig::load()?;

    println!("\n{}", "Project Information".bright_cyan().bold());
    println!("{}", "─".repeat(40));
    
    println!("{}: {}", "Name".bright_yellow(), config.name);
    println!("{}: {}", "Version".bright_yellow(), config.version);
    println!("{}: {}", "Type".bright_yellow(), config.r#type);
    println!("{}: {}", "Language".bright_yellow(), 
        if config.language.is_empty() { "C" } else { &config.language });

    if !config.deps.is_empty() {
        println!("\n{}", "Dependencies".bright_cyan());
        for (name, version) in &config.deps {
            println!("  • {} = {}", name, version);
        }
    }

    if !config.sources.dirs.is_empty() {
        println!("\n{}", "Source Directories".bright_cyan());
        for dir in &config.sources.dirs {
            println!("  • {}", dir);
        }
    }

    if !config.includes.dirs.is_empty() {
        println!("\n{}", "Include Directories".bright_cyan());
        for dir in &config.includes.dirs {
            println!("  • {}", dir);
        }
    }

    if !config.build.flags.is_empty() {
        println!("\n{}", "Build Flags".bright_cyan());
        for flag in &config.build.flags {
            println!("  • {}", flag);
        }
    }

    println!();
    Ok(())
}