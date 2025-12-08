use anyhow::{Context, Result};
use colored::Colorize;
use std::fs;
use std::path::Path;

pub fn run(all: bool) -> Result<()> {
    println!("{}", "Cleaning build artifacts...".bright_cyan());

    let mut cleaned = vec![];

    // Clean standard build directories
    let dirs_to_clean = vec![
        "target",
        ".build",
        "build",
    ];

    for dir in dirs_to_clean {
        if Path::new(dir).exists() {
            fs::remove_dir_all(dir)
                .with_context(|| format!("failed to remove {}", dir))?;
            cleaned.push(dir);
            println!("  {} {}/", "Removed".red(), dir);
        }
    }

    // Remove compile_commands.json
    if Path::new("compile_commands.json").exists() {
        fs::remove_file("compile_commands.json")
            .context("failed to remove compile_commands.json")?;
        cleaned.push("compile_commands.json");
        println!("  {} compile_commands.json", "Removed".red());
    }

    // Clean vcpkg if --all flag is set
    if all {
        if Path::new("vcpkg_installed").exists() {
            fs::remove_dir_all("vcpkg_installed")
                .context("failed to remove vcpkg_installed")?;
            cleaned.push("vcpkg_installed");
            println!("  {} vcpkg_installed/", "Removed".red());
        }
    }

    if cleaned.is_empty() {
        println!("{}", "Nothing to clean".yellow());
    } else {
        println!("\n{} Cleaned {} item(s)", "✓".green().bold(), cleaned.len());
    }

    Ok(())
}