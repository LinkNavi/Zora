// src/commands/update.rs
use anyhow::{bail, Context, Result};
use colored::Colorize;
use std::path::Path;
use std::process::Command;

pub fn run(packages: Vec<String>) -> Result<()> {
    if !Path::new("project.toml").exists() {
        bail!("project.toml not found. Run 'zora init' first.");
    }

    let vcpkg_check = Command::new("vcpkg")
        .arg("version")
        .output();

    if vcpkg_check.is_err() {
        bail!("vcpkg not found. Please install vcpkg.");
    }

    println!("{}", "Updating packages...".bright_cyan());

    if packages.is_empty() {
        // Update all packages
        println!("  {} Updating all packages...", "→".bright_blue());
        let status = Command::new("vcpkg")
            .arg("upgrade")
            .arg("--no-dry-run")
            .status()?;

        if status.success() {
            println!("{} All packages updated", "✓".green().bold());
        }
    } else {
        // Update specific packages
        for package in &packages {
            println!("  {} Updating {}...", "→".bright_blue(), package);
            
            let status = Command::new("vcpkg")
                .args(&["upgrade", package, "--no-dry-run"])
                .status()
                .context(format!("failed to update package: {}", package))?;

            if status.success() {
                println!("  {} Updated {}", "✓".green(), package);
            }
        }
    }

    Ok(())
}
