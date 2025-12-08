use anyhow::{bail, Context, Result};
use colored::Colorize;
use std::process::Command;
use walkdir::WalkDir;

use crate::config::ProjectConfig;

pub fn run(verbose: bool) -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found. Run 'zora init' first.");
    }

    let config = ProjectConfig::load()?;
    
    println!("{}", "Checking project...".bright_cyan());

    let compiler = if config.is_cpp() { "g++" } else { "gcc" };

    // Check compiler is available
    let compiler_check = Command::new(compiler)
        .arg("--version")
        .output();

    match compiler_check {
        Ok(output) if output.status.success() => {
            if verbose {
                let version = String::from_utf8_lossy(&output.stdout);
                println!("  {} Compiler: {}", "✓".green(), 
                    version.lines().next().unwrap_or(compiler));
            }
        }
        _ => {
            bail!("Compiler '{}' not found", compiler);
        }
    }

    // Find all source files
    let mut source_files = vec![];
    for source_dir in &config.sources.dirs {
        for entry in WalkDir::new(source_dir)
            .into_iter()
            .filter_map(|e| e.ok())
        {
            let path = entry.path();
            if path.is_file() {
                if let Some(ext) = path.extension() {
                    if ext == "c" || ext == "cpp" {
                        source_files.push(path.to_path_buf());
                    }
                }
            }
        }
    }

    println!("  {} Found {} source file(s)", "✓".green(), source_files.len());

    // Syntax check each file
    let mut errors = 0;
    for source_file in &source_files {
        if verbose {
            println!("  Checking {}...", source_file.display());
        }

        let mut cmd = Command::new(compiler);
        cmd.arg("-fsyntax-only")
            .arg(source_file);

        // Add include directories
        for include_dir in &config.includes.dirs {
            cmd.arg("-I").arg(include_dir);
        }

        let output = cmd.output()
            .context("failed to run syntax check")?;

        if !output.status.success() {
            errors += 1;
            let stderr = String::from_utf8_lossy(&output.stderr);
            println!("  {} {}", "✗".red(), source_file.display());
            if verbose {
                println!("{}", stderr);
            }
        }
    }

    if errors > 0 {
        println!("\n{} Found {} error(s)", "✗".red().bold(), errors);
        bail!("Syntax check failed");
    }

    println!("\n{} All checks passed", "✓".green().bold());
    Ok(())
}