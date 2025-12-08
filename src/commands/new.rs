use anyhow::{bail, Context, Result};
use colored::Colorize;
use std::fs;
use std::path::Path;
use tera::{Context as TeraContext, Tera};

use crate::config::ProjectConfig;

const SOURCE_TEMPLATE: &str = r#"#include "{{ name }}.h"

void {{ name }}_function(void) {
    // Implementation
}
"#;

const HEADER_TEMPLATE: &str = r#"#ifndef {{ name_upper }}_H
#define {{ name_upper }}_H

void {{ name }}_function(void);

#endif // {{ name_upper }}_H
"#;

const TEST_TEMPLATE: &str = r#"#include <assert.h>
#include <stdio.h>
#include "{{ name }}.h"

int main(void) {
    printf("Running tests for {{ name }}...\n");
    
    // Add your tests here
    // assert(some_condition);
    
    printf("All tests passed!\n");
    return 0;
}
"#;

pub fn run(file_type: &str, name: &str) -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found. Run 'zora init' first.");
    }

    let config = ProjectConfig::load()?;
    let ext = if config.is_cpp() { "cpp" } else { "c" };
    let header_ext = if config.is_cpp() { "hpp" } else { "h" };

    let mut ctx = TeraContext::new();
    ctx.insert("name", name);
    ctx.insert("name_upper", &name.to_uppercase());

    match file_type {
        "source" | "src" => {
            let content = Tera::one_off(SOURCE_TEMPLATE, &ctx, false)?;
            let path = format!("src/{}.{}", name, ext);
            fs::write(&path, content)?;
            println!("{} {}", "Created".green(), path);
        }
        "header" | "hdr" => {
            let content = Tera::one_off(HEADER_TEMPLATE, &ctx, false)?;
            let path = format!("include/{}.{}", name, header_ext);
            fs::write(&path, content)?;
            println!("{} {}", "Created".green(), path);
        }
        "test" => {
            fs::create_dir_all("tests")?;
            let content = Tera::one_off(TEST_TEMPLATE, &ctx, false)?;
            let path = format!("tests/test_{}.{}", name, ext);
            fs::write(&path, content)?;
            println!("{} {}", "Created".green(), path);
        }
        _ => bail!("Unknown file type '{}'. Use: source, header, or test", file_type),
    }

    Ok(())
}
