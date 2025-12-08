// src/commands/install.rs
use anyhow::{bail, Context, Result};
use colored::Colorize;
use std::fs;
use std::path::{Path, PathBuf};

use crate::config::ProjectConfig;

pub fn run(prefix: Option<String>) -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found. Run 'zora init' first.");
    }

    let config = ProjectConfig::load()?;

    // Determine install prefix
    let install_prefix = prefix.unwrap_or_else(|| {
        if cfg!(windows) {
            "C:\\Program Files".to_string()
        } else {
            "/usr/local".to_string()
        }
    });

    let bin_dir = PathBuf::from(&install_prefix).join("bin");
    let lib_dir = PathBuf::from(&install_prefix).join("lib");
    let include_dir = PathBuf::from(&install_prefix).join("include");

    println!("{}", format!("Installing to {}...", install_prefix).bright_cyan());

    // Ensure target/release exists
    let release_dir = "target/release";
    if !Path::new(release_dir).exists() {
        bail!("Release build not found. Run 'zora build --release' first.");
    }

    // Install executable or library
    if config.is_library() {
        fs::create_dir_all(&lib_dir)?;
        
        for entry in fs::read_dir(release_dir)? {
            let entry = entry?;
            let path = entry.path();
            if let Some(ext) = path.extension() {
                if ext == "a" || ext == "so" || ext == "dll" || ext == "dylib" {
                    let dest = lib_dir.join(path.file_name().unwrap());
                    fs::copy(&path, &dest)?;
                    println!("  {} {}", "Installed".green(), dest.display());
                }
            }
        }

        // Install headers
        if Path::new("include").exists() {
            fs::create_dir_all(&include_dir)?;
            for entry in fs::read_dir("include")? {
                let entry = entry?;
                let dest = include_dir.join(entry.file_name());
                fs::copy(entry.path(), &dest)?;
                println!("  {} {}", "Installed".green(), dest.display());
            }
        }
    } else {
        fs::create_dir_all(&bin_dir)?;
        
        let exe_name = if cfg!(windows) {
            format!("{}.exe", config.name)
        } else {
            config.name.clone()
        };
        
        let src = PathBuf::from(release_dir).join(&exe_name);
        let dest = bin_dir.join(&exe_name);
        
        fs::copy(&src, &dest)?;
        
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            let mut perms = fs::metadata(&dest)?.permissions();
            perms.set_mode(0o755);
            fs::set_permissions(&dest, perms)?;
        }
        
        println!("  {} {}", "Installed".green(), dest.display());
    }

    println!("\n{} Installation complete", "✓".green().bold());
    Ok(())
}
