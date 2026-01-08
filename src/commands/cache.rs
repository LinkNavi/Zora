// src/commands/cache.rs
use anyhow::Result;
use colored::Colorize;
use std::fs;
use std::path::Path;

pub fn stats() -> Result<()> {
    println!("\n{}", "Build Cache Statistics".bright_cyan().bold());
    println!("{}", "─".repeat(40));

    let cache_dirs = vec![
        ("Build artifacts", ".build"),
        ("Target directory", "target"),
        ("vcpkg cache", "vcpkg_installed"),
    ];

    let mut total_size = 0u64;

    for (name, dir) in cache_dirs {
        if Path::new(dir).exists() {
            let size = dir_size(dir)?;
            total_size += size;
            println!("{}: {}", name, format_size(size));
        } else {
            println!("{}: {}", name, "not found".dimmed());
        }
    }

    println!("{}", "─".repeat(40));
    println!("{}: {}", "Total".bright_yellow(), format_size(total_size));
    println!();

    Ok(())
}

pub fn clear() -> Result<()> {
    println!("{}", "Clearing build cache...".bright_cyan());

    let dirs_to_clear = vec![".build", "target"];
    let mut cleared = 0;

    for dir in dirs_to_clear {
        if Path::new(dir).exists() {
            fs::remove_dir_all(dir)?;
            cleared += 1;
            println!("  {} {}/", "Cleared".red(), dir);
        }
    }

    if cleared > 0 {
        println!("\n{} Cache cleared", "✓".green().bold());
    } else {
        println!("{}", "Nothing to clear".yellow());
    }

    Ok(())
}

pub fn prune() -> Result<()> {
    println!("{}", "Pruning old build artifacts...".bright_cyan());

    let mut pruned = 0;

    // Remove old build directories (except current)
    if Path::new(".build").exists() {
        for entry in fs::read_dir(".build")? {
            let entry = entry?;
            let path = entry.path();
            if path.is_dir() {
                let dir_name = path.file_name().and_then(|n| n.to_str()).unwrap_or("");
                // Keep dev and release, remove others
                if dir_name != "dev" && dir_name != "release" {
                    fs::remove_dir_all(&path)?;
                    pruned += 1;
                    println!("  {} {}", "Pruned".yellow(), path.display());
                }
            }
        }
    }

    if pruned > 0 {
        println!("\n{} Pruned {} old artifact(s)", "✓".green().bold(), pruned);
    } else {
        println!("{}", "Nothing to prune".yellow());
    }

    Ok(())
}

fn dir_size(path: impl AsRef<Path>) -> Result<u64> {
    let mut size = 0;
    if path.as_ref().is_dir() {
        for entry in fs::read_dir(path)? {
            let entry = entry?;
            let metadata = entry.metadata()?;
            if metadata.is_file() {
                size += metadata.len();
            } else if metadata.is_dir() {
                size += dir_size(entry.path())?;
            }
        }
    }
    Ok(size)
}

fn format_size(size: u64) -> String {
    const KB: u64 = 1024;
    const MB: u64 = KB * 1024;
    const GB: u64 = MB * 1024;

    if size >= GB {
        format!("{:.2} GB", size as f64 / GB as f64)
    } else if size >= MB {
        format!("{:.2} MB", size as f64 / MB as f64)
    } else if size >= KB {
        format!("{:.2} KB", size as f64 / KB as f64)
    } else {
        format!("{} bytes", size)
    }
}
