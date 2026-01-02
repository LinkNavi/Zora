use anyhow::{Context, Result};
use colored::Colorize;
use std::fs;
use std::path::Path;

pub fn run(path: String, cpp: bool, lib: bool, name: Option<String>) -> Result<()> {
    let project_path = Path::new(&path);
    
    if project_path.exists() {
        anyhow::bail!("Directory already exists: {}", path);
    }
    
    fs::create_dir_all(project_path)
        .context("Failed to create project directory")?;
    
    std::env::set_current_dir(project_path)?;
    
    println!("{} Creating new project at {}", "→".bright_blue(), path);
    
    crate::commands::init::run(name, cpp, lib)?;
    
    Ok(())
}
