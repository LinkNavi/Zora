// src/commands/lint.rs
use anyhow::{bail, Context, Result};
use colored::Colorize;
use std::process::Command;
use walkdir::WalkDir;

use crate::config::ProjectConfig;

pub fn run(fix: bool) -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found. Run 'zora init' first.");
    }

    let clang_tidy_check = Command::new("clang-tidy")
        .arg("--version")
        .output();

    if clang_tidy_check.is_err() {
        bail!("clang-tidy not found. Please install clang-tidy.");
    }

    let config = ProjectConfig::load()?;
    
    println!("{}", if fix { "Fixing linting issues..." } else { "Linting code..." }.bright_cyan());

    let mut files = vec![];
    for source_dir in &config.sources.dirs {
        for entry in WalkDir::new(source_dir).into_iter().filter_map(|e| e.ok()) {
            let path = entry.path();
            if path.is_file() {
                if let Some(ext) = path.extension() {
                    if ext == "c" || ext == "cpp" {
                        files.push(path.to_path_buf());
                    }
                }
            }
        }
    }

    let mut issues = 0;
    for file in &files {
        let mut cmd = Command::new("clang-tidy");
        cmd.arg(file);
        
        if fix {
            cmd.arg("--fix");
        }

        cmd.arg("--");
        for include_dir in &config.includes.dirs {
            cmd.arg(format!("-I{}", include_dir));
        }

        let output = cmd.output()?;
        let stdout = String::from_utf8_lossy(&output.stdout);
        
        if stdout.contains("warning:") || stdout.contains("error:") {
            issues += 1;
            println!("  {} {}", "⚠".yellow(), file.display());
        }
    }

    if issues > 0 {
        println!("\n{} Found issues in {} file(s)", "⚠".yellow().bold(), issues);
        if !fix {
            println!("Run 'zora lint --fix' to automatically fix issues");
        }
    } else {
        println!("\n{} No linting issues found", "✓".green().bold());
    }

    Ok(())
}