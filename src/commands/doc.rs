// src/commands/doc.rs
use anyhow::{bail, Context, Result};
use colored::Colorize;
use std::process::Command;

use crate::config::ProjectConfig;

pub fn run(open: bool) -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found. Run 'zora init' first.");
    }

    // Check for Doxygen
    let doxygen_check = Command::new("doxygen")
        .arg("--version")
        .output();

    if doxygen_check.is_err() {
        bail!("doxygen not found. Please install Doxygen for documentation generation.");
    }

    println!("{}", "Generating documentation...".bright_cyan());

    // Generate default Doxyfile if it doesn't exist
    if !std::path::Path::new("Doxyfile").exists() {
        let status = Command::new("doxygen")
            .arg("-g")
            .status()?;
        
        if status.success() {
            println!("  {} Generated Doxyfile", "✓".green());
        }
    }

    // Run Doxygen
    let status = Command::new("doxygen")
        .status()
        .context("failed to run doxygen")?;

    if !status.success() {
        bail!("Documentation generation failed");
    }

    println!("{} Documentation generated in docs/", "✓".green().bold());

    if open {
        #[cfg(target_os = "macos")]
        Command::new("open").arg("docs/html/index.html").spawn()?;
        
        #[cfg(target_os = "linux")]
        Command::new("xdg-open").arg("docs/html/index.html").spawn()?;
        
        #[cfg(target_os = "windows")]
        Command::new("cmd").args(&["/C", "start", "docs\\html\\index.html"]).spawn()?;
    }

    Ok(())
}