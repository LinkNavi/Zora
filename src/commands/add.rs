use anyhow::{bail, Context, Result};
use std::fs;
use std::path::Path;
use std::process::Command;

pub fn run(packages: Vec<String>) -> Result<()> {
    if packages.is_empty() {
        bail!("No packages specified. Usage: zora add <package1> <package2> ...");
    }

    // Check if vcpkg is installed
    let vcpkg_check = Command::new("vcpkg")
        .arg("version")
        .output();

    if vcpkg_check.is_err() {
        bail!("vcpkg not found. Please install vcpkg and ensure it's in your PATH.\nSee: https://vcpkg.io/en/getting-started.html");
    }

    // Check if project.toml exists
    if !Path::new("project.toml").exists() {
        bail!("project.toml not found. Run 'zora init' first.");
    }

    // Read the current project.toml
    let project_toml = fs::read_to_string("project.toml")
        .context("failed to read project.toml")?;

    // Install each package with vcpkg
    for package in &packages {
        println!("📦 Installing {} via vcpkg...", package);
        
        let status = Command::new("vcpkg")
            .args(&["install", package])
            .status()
            .context(format!("failed to install package: {}", package))?;

        if !status.success() {
            bail!("Failed to install package: {}", package);
        }

        println!("✔ Installed {}", package);
    }

    // Update project.toml with new dependencies
    let updated_toml = add_dependencies_to_toml(&project_toml, &packages)?;
    fs::write("project.toml", updated_toml)
        .context("failed to write updated project.toml")?;

    println!("\n✔ Added {} package(s) to project.toml", packages.len());
    println!("Run 'zora build' to rebuild with new dependencies.");

    Ok(())
}

fn add_dependencies_to_toml(toml_content: &str, packages: &[String]) -> Result<String> {
    let mut lines: Vec<String> = toml_content.lines().map(|s| s.to_string()).collect();
    
    // Find the [deps] section
    let mut deps_index = None;
    for (i, line) in lines.iter().enumerate() {
        if line.trim() == "[deps]" {
            deps_index = Some(i);
            break;
        }
    }

    let deps_index = match deps_index {
        Some(idx) => idx,
        None => {
            // If [deps] section doesn't exist, add it at the end
            lines.push(String::new());
            lines.push("[deps]".to_string());
            lines.len() - 1
        }
    };

    // Find where to insert new dependencies (after [deps] line)
    let mut insert_index = deps_index + 1;
    
    // Skip to the end of the [deps] section
    while insert_index < lines.len() {
        let line = lines[insert_index].trim();
        if line.starts_with('[') && line.ends_with(']') {
            // Found next section
            break;
        }
        if !line.is_empty() && !line.starts_with('#') {
            insert_index += 1;
        } else {
            break;
        }
    }

    // Check which packages are already listed
    let existing_deps: Vec<String> = lines[deps_index + 1..insert_index]
        .iter()
        .filter_map(|line| {
            let trimmed = line.trim();
            if !trimmed.is_empty() && !trimmed.starts_with('#') {
                Some(trimmed.to_string())
            } else {
                None
            }
        })
        .collect();

    // Add new packages that aren't already listed
    for package in packages {
        let dep_line = format!("{} = \"*\"", package);
        if !existing_deps.iter().any(|d| d.starts_with(package)) {
            lines.insert(insert_index, dep_line);
            insert_index += 1;
        } else {
            println!("Note: {} already in project.toml", package);
        }
    }

    Ok(lines.join("\n") + "\n")
}