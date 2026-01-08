use anyhow::{bail, Result};
use colored::Colorize;

pub fn run(_shell: String) -> Result<()> {
    println!("{}", "Shell completions coming soon!".yellow());
    bail!("Completions command is not yet implemented");
}
