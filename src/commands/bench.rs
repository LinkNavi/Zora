use anyhow::{bail, Context, Result};
use colored::Colorize;
use std::fs;
use std::path::Path;
use std::process::Command;
use std::time::Instant;
use walkdir::WalkDir;

use crate::config::ProjectConfig;

pub fn run(specific_bench: Option<String>) -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found. Run 'zora init' first.");
    }

    let config = ProjectConfig::load()?;
    let bench_dir = "benches";

    if !Path::new(bench_dir).exists() {
        println!("{}", "No benchmarks directory found".yellow());
        println!("Create benchmarks in the 'benches/' directory");
        return Ok(());
    }

    println!("{}", "Running benchmarks...".bright_cyan());

    let mut bench_files = vec![];
    for entry in WalkDir::new(bench_dir).into_iter().filter_map(|e| e.ok()) {
        let path = entry.path();
        if path.is_file() {
            if let Some(ext) = path.extension() {
                if ext == "c" || ext == "cpp" {
                    if let Some(bench_name) = &specific_bench {
                        if path.file_stem()
                            .and_then(|s| s.to_str())
                            .map(|s| s.contains(bench_name))
                            .unwrap_or(false)
                        {
                            bench_files.push(path.to_path_buf());
                        }
                    } else {
                        bench_files.push(path.to_path_buf());
                    }
                }
            }
        }
    }

    if bench_files.is_empty() {
        println!("{}", "No benchmark files found".yellow());
        return Ok(());
    }

    for bench_file in bench_files {
        let bench_name = bench_file.file_stem().and_then(|s| s.to_str()).unwrap_or("unknown");
        
        // Compile benchmark
        let output_dir = "target/benches";
        fs::create_dir_all(output_dir)?;
        let output_file = format!("{}/{}", output_dir, bench_name);
        
        let compiler = if config.is_cpp() { "g++" } else { "gcc" };
        let status = Command::new(compiler)
            .arg(&bench_file)
            .arg("-o")
            .arg(&output_file)
            .arg("-O3")
            .arg("-I")
            .arg("include")
            .status()?;

        if !status.success() {
            println!("  {} Compilation failed for {}", "✗".red(), bench_name);
            continue;
        }

        // Run benchmark
        println!("\n{} {}...", "Benchmarking".bright_blue(), bench_name);
        let start = Instant::now();
        Command::new(&output_file).status()?;
        let duration = start.elapsed();
        
        println!("  Time: {:.2?}", duration);
    }

    Ok(())
}
