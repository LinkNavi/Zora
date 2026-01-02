use anyhow::Result;
use colored::Colorize;
use std::process::Command;

pub fn run(query: String) -> Result<()> {
    println!("{} {}", "Searching vcpkg for".bright_cyan(), query.bright_yellow());
    
    Command::new("vcpkg")
        .args(&["search", &query])
        .status()?;
    
    Ok(())
}
