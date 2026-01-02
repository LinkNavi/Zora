use anyhow::{bail, Result};
use colored::Colorize;
use crate::config::ProjectConfig;

pub fn run(depth: Option<usize>) -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found");
    }
    
    let config = ProjectConfig::load()?;
    let max_depth = depth.unwrap_or(usize::MAX);
    
    println!("{} v{}", config.name.bright_yellow(), config.version);
    
    fn print_deps(deps: &std::collections::HashMap<String, crate::config::DependencySpec>, 
                  prefix: &str, depth: usize, max_depth: usize) {
        if depth >= max_depth {
            return;
        }
        
        let count = deps.len();
        for (i, (name, spec)) in deps.iter().enumerate() {
            let is_last = i == count - 1;
            let connector = if is_last { "└──" } else { "├──" };
            let version = spec.version();
            
            println!("{}{} {} v{}", prefix, connector, name, version);
        }
    }
    
    print_deps(&config.deps, "", 0, max_depth);
    
    if !config.dev_deps.is_empty() {
        println!("\n{}", "[dev-dependencies]".bright_cyan());
        print_deps(&config.dev_deps, "", 0, max_depth);
    }
    
    Ok(())
}
