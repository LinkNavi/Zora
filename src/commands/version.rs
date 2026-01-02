
use anyhow::Result;
use colored::Colorize;
use crate::config::ProjectConfig;

pub fn run(verbose: bool) -> Result<()> {
    let version = env!("CARGO_PKG_VERSION");
    
    if verbose {
        println!("{} {}", "zora".bright_cyan().bold(), version);
        println!("{}: {}", "commit-hash".dimmed(), "unknown");
        
        if ProjectConfig::exists() {
            let config = ProjectConfig::load()?;
            println!("\n{}", "Project:".bright_cyan());
            println!("  {}: {}", "name", config.name);
            println!("  {}: {}", "version", config.version);
            println!("  {}: {}", "type", config.r#type);
        }
    } else {
        println!("zora {}", version);
    }
    
    Ok(())
}

