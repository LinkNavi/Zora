use anyhow::{bail, Result};
use colored::Colorize;

pub fn run(_dry_run: bool, _registry: Option<String>) -> Result<()> {
    println!("{}", "Package publishing coming soon!".yellow());
    bail!("Publish command is not yet implemented");
}
