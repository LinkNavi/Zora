use anyhow::{bail, Result};
use colored::Colorize;
use std::path::Path;
use std::process::Command;
use std::thread;
use std::time::{Duration, SystemTime};

use crate::config::ProjectConfig;

pub fn run(command: &str) -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found. Run 'zora init' first.");
    }

    println!("{}", "Watching for changes...".bright_cyan());
    println!("Press Ctrl+C to stop\n");

    let config = ProjectConfig::load()?;
    let mut last_modified = SystemTime::now();

    loop {
        let mut should_rebuild = false;

        // Check source files
        for source_dir in &config.sources.dirs {
            if let Ok(entries) = std::fs::read_dir(source_dir) {
                for entry in entries.filter_map(|e| e.ok()) {
                    if let Ok(metadata) = entry.metadata() {
                        if let Ok(modified) = metadata.modified() {
                            if modified > last_modified {
                                should_rebuild = true;
                                last_modified = SystemTime::now();
                                break;
                            }
                        }
                    }
                }
            }
        }

        if should_rebuild {
            println!("\n{} Change detected, rebuilding...", "→".bright_blue());
            
            let result = match command {
                "build" => Command::new("zora").arg("build").status(),
                "test" => Command::new("zora").arg("test").status(),
                "run" => Command::new("zora").arg("run").status(),
                _ => {
                    println!("Unknown command: {}", command);
                    continue;
                }
            };

            match result {
                Ok(status) if status.success() => {
                    println!("{} Build succeeded", "✓".green().bold());
                }
                _ => {
                    println!("{} Build failed", "✗".red().bold());
                }
            }
            
            println!("\n{}", "Watching for changes...".bright_cyan());
        }

        thread::sleep(Duration::from_secs(1));
    }
}