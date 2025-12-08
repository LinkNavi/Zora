use anyhow::{bail, Context, Result};
use colored::Colorize;
use std::fs;
use std::path::Path;
use std::process::Command;

pub fn run(packages: Vec<String>) -> Result<()> {
    if packages.is_empty() {
        bail!("No packages specified. Usage: zora remove <package1> <package2> ...");
    }

    if !Path::new("project.toml").exists() {
        bail!("project.toml not found. Run 'zora init' first.");
    }

    println!("{}", "Removing packages...".bright_cyan());

    for package in &packages {
        println!("  {} Removing {}...", "→".bright_blue(), package);
        
        let status = Command::new("vcpkg")
            .args(&["remove", package])
            .status()
            .context(format!("failed to remove package: {}", package))?;

        if status.success() {
            println!("  {} Removed {}", "✓".green(), package);
        }
    }

    // Update project.toml
    let project_toml = fs::read_to_string("project.toml")?;
    let updated_toml = remove_dependencies_from_toml(&project_toml, &packages)?;
    fs::write("project.toml", updated_toml)?;

    println!("\n{} Removed {} package(s)", "✓".green().bold(), packages.len());
    Ok(())
}

fn remove_dependencies_from_toml(toml_content: &str, packages: &[String]) -> Result<String> {
    let lines: Vec<String> = toml_content
        .lines()
        .filter(|line| {
            let trimmed = line.trim();
            !packages.iter().any(|pkg| trimmed.starts_with(pkg))
        })
        .map(|s| s.to_string())
        .collect();
    
    Ok(lines.join("\n") + "\n")
}
