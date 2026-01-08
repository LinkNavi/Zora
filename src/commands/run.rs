use anyhow::{bail, Context, Result};
use colored::Colorize;
use std::process::Command;

pub fn run(
    name_opt: Option<String>, 
    mode: &str,
    verbose: bool,
    jobs: Option<usize>,
    args: Vec<String>
) -> Result<()> {
    // First, build the project
    println!("{}", "Building project...".bright_cyan());
    super::build::run(name_opt.clone(), mode, verbose, jobs, vec![], false, false, None)?;

    // Get the executable path
    let exe_path = super::build::get_executable_path(name_opt, mode)?;

    if !exe_path.exists() {
        bail!("Executable not found at: {}", exe_path.display());
    }

    println!("\n{} {}...\n", "Running".bright_blue(), exe_path.display());
    println!("{}", "─".repeat(50).dimmed());

    // Run the executable with any provided arguments
    let status = Command::new(&exe_path)
        .args(&args)
        .status()
        .context("failed to run executable")?;

    println!("{}", "─".repeat(50).dimmed());
    
    if !status.success() {
        let code = status.code().unwrap_or(-1);
        bail!("Program exited with error code: {}", code);
    }

    println!("\n{} Program completed successfully", "✓".green().bold());
    Ok(())
}
