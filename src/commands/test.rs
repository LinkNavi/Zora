use anyhow::{bail, Context, Result};
use colored::Colorize;
use std::fs;
use std::path::Path;
use std::process::Command;
use walkdir::WalkDir;

use crate::config::ProjectConfig;

pub fn run(mode: &str, specific_test: Option<String>) -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found. Run 'zora init' first.");
    }

    let config = ProjectConfig::load()?;
    
    println!("{}", "Running tests...".bright_cyan());

    // Find test files
    let test_dirs = &config.tests.dirs;
    let mut test_files = vec![];

    for test_dir in test_dirs {
        if !Path::new(test_dir).exists() {
            continue;
        }

        for entry in WalkDir::new(test_dir)
            .into_iter()
            .filter_map(|e| e.ok())
        {
            let path = entry.path();
            if path.is_file() {
                if let Some(ext) = path.extension() {
                    if ext == "c" || ext == "cpp" {
                        if let Some(test_name) = &specific_test {
                            if path.file_stem()
                                .and_then(|s| s.to_str())
                                .map(|s| s.contains(test_name))
                                .unwrap_or(false)
                            {
                                test_files.push(path.to_path_buf());
                            }
                        } else {
                            test_files.push(path.to_path_buf());
                        }
                    }
                }
            }
        }
    }

    if test_files.is_empty() {
        println!("{}", "No test files found".yellow());
        return Ok(());
    }

    println!("Found {} test file(s)", test_files.len());

    let mut passed = 0;
    let mut failed = 0;

    for test_file in test_files {
        let test_name = test_file
            .file_stem()
            .and_then(|s| s.to_str())
            .unwrap_or("unknown");

        println!("\n{} {}...", "Testing".bright_blue(), test_name);

        // Compile test
        let output_dir = format!("target/{}/tests", mode);
        fs::create_dir_all(&output_dir)?;

        let output_file = format!("{}/{}", output_dir, test_name);
        let compiler = if config.is_cpp() { "g++" } else { "gcc" };

        let mut cmd = Command::new(compiler);
        cmd.arg(&test_file)
            .arg("-o")
            .arg(&output_file)
            .arg("-I")
            .arg("include");

        // Add optimization flags
        if mode == "release" {
            cmd.arg("-O2");
        }

        let compile_status = cmd.status()
            .context("failed to compile test")?;

        if !compile_status.success() {
            println!("  {} Compilation failed", "✗".red().bold());
            failed += 1;
            continue;
        }

        // Run test
        let test_status = Command::new(&output_file)
            .status()
            .context("failed to run test")?;

        if test_status.success() {
            println!("  {} {}", "✓".green().bold(), "PASSED".green());
            passed += 1;
        } else {
            println!("  {} {}", "✗".red().bold(), "FAILED".red());
            failed += 1;
        }
    }

    println!("\n{}", "─".repeat(40));
    println!("Test results: {} passed, {} failed", 
        passed.to_string().green(), 
        failed.to_string().red()
    );

    if failed > 0 {
        bail!("Some tests failed");
    }

    Ok(())
}
