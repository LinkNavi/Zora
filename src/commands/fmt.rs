use anyhow::{bail, Context, Result};
use colored::Colorize;
use std::process::Command;
use walkdir::WalkDir;

use crate::config::ProjectConfig;

pub fn run(check: bool) -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found. Run 'zora init' first.");
    }

    // Check if clang-format is installed
    let clang_format_check = Command::new("clang-format")
        .arg("--version")
        .output();

    if clang_format_check.is_err() {
        bail!("clang-format not found. Please install clang-format.");
    }

    let config = ProjectConfig::load()?;
    
    if check {
        println!("{}", "Checking code formatting...".bright_cyan());
    } else {
        println!("{}", "Formatting code...".bright_cyan());
    }

    // Find all source and header files
    let mut files = vec![];
    
    for source_dir in &config.sources.dirs {
        for entry in WalkDir::new(source_dir)
            .into_iter()
            .filter_map(|e| e.ok())
        {
            let path = entry.path();
            if path.is_file() {
                if let Some(ext) = path.extension() {
                    let ext_str = ext.to_str().unwrap_or("");
                    if ["c", "cpp", "h", "hpp", "cc", "cxx"].contains(&ext_str) {
                        files.push(path.to_path_buf());
                    }
                }
            }
        }
    }

    for include_dir in &config.includes.dirs {
        for entry in WalkDir::new(include_dir)
            .into_iter()
            .filter_map(|e| e.ok())
        {
            let path = entry.path();
            if path.is_file() {
                if let Some(ext) = path.extension() {
                    let ext_str = ext.to_str().unwrap_or("");
                    if ["h", "hpp"].contains(&ext_str) {
                        files.push(path.to_path_buf());
                    }
                }
            }
        }
    }

    if files.is_empty() {
        println!("{}", "No files to format".yellow());
        return Ok(());
    }

    let mut formatted = 0;
    let mut needs_formatting = 0;

    for file in &files {
        let mut cmd = Command::new("clang-format");
        
        if check {
            cmd.arg("--dry-run")
                .arg("-Werror");
        } else {
            cmd.arg("-i");
        }
        
        cmd.arg(file);

        let output = cmd.output()
            .context("failed to run clang-format")?;

        if check {
            if !output.status.success() {
                needs_formatting += 1;
                println!("  {} {}", "✗".red(), file.display());
            }
        } else {
            if output.status.success() {
                formatted += 1;
                println!("  {} {}", "✓".green(), file.display());
            }
        }
    }

    if check {
        if needs_formatting > 0 {
            println!("\n{} {} file(s) need formatting", 
                "✗".red().bold(), 
                needs_formatting
            );
            println!("Run 'zora fmt' to format them");
            bail!("Formatting check failed");
        } else {
            println!("\n{} All files are properly formatted", "✓".green().bold());
        }
    } else {
        println!("\n{} Formatted {} file(s)", "✓".green().bold(), formatted);
    }

    Ok(())
}