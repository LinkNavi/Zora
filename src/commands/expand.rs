use anyhow::{bail, Result};
use colored::Colorize;
use std::process::Command;

pub fn run(file: Option<String>) -> Result<()> {
    if let Some(f) = file {
        println!("{} Expanding macros in: {}", "→".bright_blue(), f);
        
        Command::new("gcc")
            .args(&["-E", &f])
            .status()?;
    } else {
        bail!("No file specified");
    }
    
    Ok(())
}
