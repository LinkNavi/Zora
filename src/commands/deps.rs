use anyhow::{bail, Result};
use colored::Colorize;

use crate::config::ProjectConfig;

pub fn run(tree: bool) -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found. Run 'zora init' first.");
    }

    let config = ProjectConfig::load()?;

    println!("\n{}", "Dependencies".bright_cyan().bold());
    println!("{}", "─".repeat(40));

    if config.deps.is_empty() {
        println!("{}", "No dependencies".yellow());
    } else {
        for (name, version) in &config.deps {
            if tree {
                println!("├── {} {}", name.bright_yellow(), version);
            } else {
                println!("{} = {}", name, version);
            }
        }
    }

    println!();
    Ok(())
}