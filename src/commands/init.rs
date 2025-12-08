use anyhow::{bail, Context, Result};
use colored::Colorize;
use std::fs;
use std::path::Path;
use tera::{Context as TeraContext, Tera};

const MAIN_C_TEMPLATE: &str = r#"#include <stdio.h>

int main(void) {
    printf("Hello from {{ name }}!\n");
    return 0;
}
"#;

const MAIN_CPP_TEMPLATE: &str = r#"#include <iostream>

int main() {
    std::cout << "Hello from {{ name }}!" << std::endl;
    return 0;
}
"#;

const LIB_C_TEMPLATE: &str = r#"#include "{{ name }}.h"

void {{ name }}_hello(void) {
    printf("Hello from {{ name }} library!\n");
}
"#;

const LIB_H_TEMPLATE: &str = r#"#ifndef {{ name_upper }}_H
#define {{ name_upper }}_H

#include <stdio.h>

void {{ name }}_hello(void);

#endif // {{ name_upper }}_H
"#;

const LIB_CPP_TEMPLATE: &str = r#"#include "{{ name }}.hpp"
#include <iostream>

namespace {{ name }} {

void hello() {
    std::cout << "Hello from {{ name }} library!" << std::endl;
}

} // namespace {{ name }}
"#;

const LIB_HPP_TEMPLATE: &str = r#"#ifndef {{ name_upper }}_HPP
#define {{ name_upper }}_HPP

namespace {{ name }} {

void hello();

} // namespace {{ name }}

#endif // {{ name_upper }}_HPP
"#;

const PROJECT_TOML_TEMPLATE: &str = r#"name = "{{ name }}"
version = "0.1.0"
type = "{{ project_type }}"
language = "{{ language }}"

[sources]
dirs = ["src"]

[includes]
dirs = ["include"]

[build]
flags = ["-Wall", "-Wextra", "-Wpedantic"]
optimization = "2"

[deps]

[scripts]
# Custom build scripts can be defined here
# prebuild = "echo 'Running prebuild'"
# postbuild = "echo 'Build complete'"

{% if is_lib %}
[tests]
dirs = ["tests"]
{% endif %}
"#;

const GITIGNORE_TEMPLATE: &str = r#"# Build artifacts
/target
/.build
/build
compile_commands.json

# vcpkg
/vcpkg_installed

# IDE
.vscode/
.idea/
*.swp
*.swo
*~

# OS
.DS_Store
Thumbs.db

# Compiled files
*.o
*.obj
*.a
*.lib
*.so
*.dll
*.dylib
*.exe
"#;

const README_TEMPLATE: &str = r#"# {{ name }}

{{ description }}

## Building

```bash
zora build
```

## Running

```bash
zora run
```

## Testing

```bash
zora test
```

## Dependencies

Add dependencies using:

```bash
zora add <package-name>
```
"#;

pub fn run(name_opt: Option<String>, cpp: bool, lib: bool) -> Result<()> {
    let cwd = std::env::current_dir().context("failed to get current directory")?;
    let project_name = match name_opt {
        Some(n) => n,
        None => cwd
            .file_name()
            .and_then(|s| s.to_str())
            .map(|s| s.to_string())
            .unwrap_or_else(|| "zora-project".to_string()),
    };

    if Path::new("project.toml").exists() {
        bail!("project.toml already exists in this directory");
    }

    println!("{}", "Initializing project...".bright_cyan());

    // Create directories
    fs::create_dir_all("src").context("failed to create src/")?;
    fs::create_dir_all("include").context("failed to create include/")?;
    
    if lib {
        fs::create_dir_all("tests").context("failed to create tests/")?;
    }

    let language = if cpp { "cpp" } else { "c" };
    let project_type = if lib { "lib" } else { "exec" };
    let ext = if cpp { "cpp" } else { "c" };
    let header_ext = if cpp { "hpp" } else { "h" };

    // Render templates
    let mut ctx = TeraContext::new();
    ctx.insert("name", &project_name);
    ctx.insert("name_upper", &project_name.to_uppercase());
    ctx.insert("language", language);
    ctx.insert("project_type", project_type);
    ctx.insert("is_lib", &lib);
    ctx.insert("description", &format!("A {} {} project", 
        if cpp { "C++" } else { "C" },
        if lib { "library" } else { "executable" }
    ));

    // Write source files
    if lib {
        // Library
        let lib_src = if cpp {
            Tera::one_off(LIB_CPP_TEMPLATE, &ctx, false)?
        } else {
            Tera::one_off(LIB_C_TEMPLATE, &ctx, false)?
        };

        let lib_header = if cpp {
            Tera::one_off(LIB_HPP_TEMPLATE, &ctx, false)?
        } else {
            Tera::one_off(LIB_H_TEMPLATE, &ctx, false)?
        };

        fs::write(format!("src/{}.{}", project_name, ext), lib_src)
            .context("failed to write library source")?;
        fs::write(format!("include/{}.{}", project_name, header_ext), lib_header)
            .context("failed to write library header")?;

        println!("  {} {}", "Created".green(), format!("src/{}.{}", project_name, ext));
        println!("  {} {}", "Created".green(), format!("include/{}.{}", project_name, header_ext));
    } else {
        // Executable
        let main_src = if cpp {
            Tera::one_off(MAIN_CPP_TEMPLATE, &ctx, false)?
        } else {
            Tera::one_off(MAIN_C_TEMPLATE, &ctx, false)?
        };

        fs::write(format!("src/main.{}", ext), main_src)
            .context("failed to write main source")?;
        
        println!("  {} {}", "Created".green(), format!("src/main.{}", ext));
    }

    // Write project.toml
    let project_toml = Tera::one_off(PROJECT_TOML_TEMPLATE, &ctx, false)?;
    fs::write("project.toml", project_toml)
        .context("failed to write project.toml")?;
    println!("  {} project.toml", "Created".green());

    // Write .gitignore
    let gitignore = Tera::one_off(GITIGNORE_TEMPLATE, &ctx, false)?;
    fs::write(".gitignore", gitignore)
        .context("failed to write .gitignore")?;
    println!("  {} .gitignore", "Created".green());

    // Write README
    let readme = Tera::one_off(README_TEMPLATE, &ctx, false)?;
    fs::write("README.md", readme)
        .context("failed to write README.md")?;
    println!("  {} README.md", "Created".green());

    println!("\n{} Initialized {} project: {}", 
        "✓".green().bold(), 
        if lib { "library" } else { "executable" },
        project_name.bright_yellow()
    );
    
    println!("\n{}", "Next steps:".bright_cyan());
    println!("  {} Build your project", "→".bright_blue());
    println!("  {} Run your project", "→".bright_blue());
    if lib {
        println!("  {} Run tests", "→".bright_blue());
    }

    Ok(())
}