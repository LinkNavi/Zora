// src/commands/package.rs
use anyhow::{bail, Context, Result};
use colored::Colorize;
use std::fs;
use std::path::Path;
use std::process::Command;

use crate::config::ProjectConfig;

pub fn run(format: &str) -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found. Run 'zora init' first.");
    }

    let config = ProjectConfig::load()?;
    
    println!("{}", "Packaging project...".bright_cyan());

    // Ensure target/release exists
    let release_dir = "target/release";
    if !Path::new(release_dir).exists() {
        bail!("Release build not found. Run 'zora build --release' first.");
    }

    let package_name = format!("{}-{}", config.name, config.version);
    let package_dir = format!("target/package/{}", package_name);

    // Create package directory structure
    fs::create_dir_all(&package_dir)?;
    fs::create_dir_all(format!("{}/bin", package_dir))?;
    fs::create_dir_all(format!("{}/include", package_dir))?;
    fs::create_dir_all(format!("{}/lib", package_dir))?;

    // Copy executable or library
    if config.is_library() {
        // Copy library files
        for entry in fs::read_dir(release_dir)? {
            let entry = entry?;
            let path = entry.path();
            if let Some(ext) = path.extension() {
                if ext == "a" || ext == "so" || ext == "dll" || ext == "dylib" {
                    let dest = format!("{}/lib/{}", package_dir, path.file_name().unwrap().to_str().unwrap());
                    fs::copy(&path, dest)?;
                }
            }
        }
    } else {
        // Copy executable
        let exe_name = if cfg!(windows) {
            format!("{}.exe", config.name)
        } else {
            config.name.clone()
        };
        
        let src = format!("{}/{}", release_dir, exe_name);
        let dest = format!("{}/bin/{}", package_dir, exe_name);
        fs::copy(src, dest)?;
    }

    // Copy headers
    if Path::new("include").exists() {
        for entry in fs::read_dir("include")? {
            let entry = entry?;
            let dest = format!("{}/include/{}", package_dir, entry.file_name().to_str().unwrap());
            fs::copy(entry.path(), dest)?;
        }
    }

    // Copy README and LICENSE if they exist
    for file in &["README.md", "LICENSE", "LICENSE.txt"] {
        if Path::new(file).exists() {
            fs::copy(file, format!("{}/{}", package_dir, file))?;
        }
    }

    // Create archive
    let archive_name = match format {
        "tar" | "tar.gz" => {
            let archive = format!("target/{}.tar.gz", package_name);
            Command::new("tar")
                .args(&["-czf", &archive, "-C", "target/package", &package_name])
                .status()?;
            archive
        }
        "zip" => {
            let archive = format!("target/{}.zip", package_name);
            Command::new("zip")
                .args(&["-r", &archive, &package_name])
                .current_dir("target/package")
                .status()?;
            archive
        }
        _ => bail!("Unsupported format: {}. Use 'tar' or 'zip'", format),
    };

    println!("{} Package created: {}", "✓".green().bold(), archive_name);
    Ok(())
}
