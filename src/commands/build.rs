use anyhow::{bail, Context, Result};
use colored::Colorize;
use indicatif::{ProgressBar, ProgressStyle};
use std::fs;
use std::path::Path;
use std::process::Command;
use tera::{Context as TeraContext, Tera};

use crate::config::ProjectConfig;

const PROJECT_CMAKE_TEMPLATE: &str = r#"
cmake_minimum_required(VERSION 3.10)
project({{ name }} {{ language }})

{% if use_vcpkg %}
set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
{% endif %}

{% if cpp_std %}
set(CMAKE_CXX_STANDARD {{ cpp_std }})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
{% endif %}

{% if c_std %}
set(CMAKE_C_STANDARD {{ c_std }})
set(CMAKE_C_STANDARD_REQUIRED ON)
{% endif %}

file(GLOB_RECURSE SOURCES 
{% for source_dir in source_dirs %}
    "${PROJECT_SOURCE_DIR}/../../{{ source_dir }}/*.c"
    "${PROJECT_SOURCE_DIR}/../../{{ source_dir }}/*.cpp"
{% endfor %}
)


{% if is_library %}
add_library({{ name }} ${SOURCES})
{% else %}
add_executable({{ name }} ${SOURCES})
{% endif %}

{% for include_dir in include_dirs %}
target_include_directories({{ name }} PRIVATE "${PROJECT_SOURCE_DIR}/../../{{ include_dir }}")
{% endfor %}

{% if vcpkg_packages %}
# Find and link vcpkg packages
{% for package in vcpkg_packages %}
find_package({{ package }} REQUIRED)
target_link_libraries({{ name }} PRIVATE {{ package }}::{{ package }})
{% endfor %}
{% endif %}

{% if build_flags %}
# Custom build flags
target_compile_options({{ name }} PRIVATE 
{% for flag in build_flags %}
    "{{ flag }}"
{% endfor %}
)
{% endif %}

{% if defines %}
# Preprocessor definitions
{% for key, value in defines %}
target_compile_definitions({{ name }} PRIVATE {{ key }}={{ value }})
{% endfor %}
{% endif %}

{% if link_libs %}
# Additional libraries
target_link_libraries({{ name }} PRIVATE 
{% for lib in link_libs %}
    {{ lib }}
{% endfor %}
)
{% endif %}

{% if lib_dirs %}
# Library directories
{% for lib_dir in lib_dirs %}
target_link_directories({{ name }} PRIVATE "{{ lib_dir }}")
{% endfor %}
{% endif %}
"#;

#[derive(Clone, Copy)]
pub enum BuildMode {
    Debug,
    Release,
}

impl BuildMode {
    pub fn as_str(&self) -> &str {
        match self {
            BuildMode::Debug => "debug",
            BuildMode::Release => "release",
        }
    }

    pub fn cmake_build_type(&self) -> &str {
        match self {
            BuildMode::Debug => "Debug",
            BuildMode::Release => "Release",
        }
    }
}

pub fn run(name_opt: Option<String>, mode: BuildMode, verbose: bool, jobs: Option<usize>) -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found. Run 'zora init' first.");
    }

    let config = ProjectConfig::load()?;

    let pb = ProgressBar::new_spinner();
    pb.set_style(
        ProgressStyle::default_spinner()
            .template("{spinner:.cyan} {msg}")
            .unwrap()
    );

    pb.set_message("Preparing build...");

    // Determine build directory
    let build_dir = format!(".build/{}", mode.as_str());
    fs::create_dir_all(&build_dir).context("failed to create build directory")?;

    // Determine project name
    let project_name = name_opt.unwrap_or_else(|| config.name.clone());

    // Prepare CMake context
    let mut ctx = TeraContext::new();
    ctx.insert("name", &project_name);
    ctx.insert("language", if config.is_cpp() { "CXX" } else { "C" });
    ctx.insert("source_dirs", &config.sources.dirs);
    ctx.insert("include_dirs", &config.includes.dirs);
    ctx.insert("is_library", &config.is_library());
    ctx.insert("use_vcpkg", &!config.deps.is_empty());
    
    // C++ standard
    if config.is_cpp() && !config.std.is_empty() {
        ctx.insert("cpp_std", &config.std);
    }
    
    // C standard
    if !config.is_cpp() && !config.std.is_empty() {
        ctx.insert("c_std", &config.std);
    }

    // Build configuration
    if !config.build.flags.is_empty() {
        ctx.insert("build_flags", &config.build.flags);
    }
    if !config.build.defines.is_empty() {
        ctx.insert("defines", &config.build.defines);
    }
    if !config.build.libs.is_empty() {
        ctx.insert("link_libs", &config.build.libs);
    }
    if !config.build.lib_dirs.is_empty() {
        ctx.insert("lib_dirs", &config.build.lib_dirs);
    }

    // vcpkg packages
    if !config.deps.is_empty() {
        let packages: Vec<&String> = config.deps.keys().collect();
        ctx.insert("vcpkg_packages", &packages);
    }

    pb.set_message("Generating CMake files...");

    // Render CMakeLists.txt
    let cmake_content = Tera::one_off(PROJECT_CMAKE_TEMPLATE, &ctx, false)
        .context("failed to render CMakeLists.txt template")?;

    let cmake_path = Path::new(&build_dir).join("CMakeLists.txt");
    fs::write(&cmake_path, cmake_content)
        .context("failed to write CMakeLists.txt")?;

    if verbose {
        println!("  {} {}", "Generated".green(), cmake_path.display());
    }

    pb.set_message("Configuring project...");

    // Run CMake configuration
    let mut cmake_config = Command::new("cmake");
    cmake_config
        .args(&[
            "-S", &build_dir,
            "-B", &build_dir,
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            &format!("-DCMAKE_BUILD_TYPE={}", mode.cmake_build_type()),
        ]);

    if verbose {
        cmake_config.arg("-DCMAKE_VERBOSE_MAKEFILE=ON");
    }

    let status = cmake_config
        .status()
        .context("failed to run cmake configuration")?;

    if !status.success() {
        pb.finish_and_clear();
        bail!("CMake configuration failed");
    }

    pb.set_message(format!("Building {} [{}]...", project_name, mode.as_str()));

    // Build the project
    let mut cmake_build = Command::new("cmake");
    cmake_build.args(&["--build", &build_dir]);

    if let Some(j) = jobs {
        cmake_build.arg("-j").arg(j.to_string());
    } else {
        // Use number of CPUs
        let num_cpus = num_cpus::get();
        cmake_build.arg("-j").arg(num_cpus.to_string());
    }

    if verbose {
        cmake_build.arg("--verbose");
    }

    let status = cmake_build
        .status()
        .context("failed to run cmake build")?;

    if !status.success() {
        pb.finish_and_clear();
        bail!("Build failed");
    }

    // Copy artifacts to target directory
    let target_dir = format!("target/{}", mode.as_str());
    fs::create_dir_all(&target_dir).context("failed to create target directory")?;

    if config.is_library() {
        // Copy library files
        for entry in fs::read_dir(&build_dir)? {
            let entry = entry?;
            let path = entry.path();
            if let Some(ext) = path.extension() {
                let ext_str = ext.to_str().unwrap_or("");
                if ["a", "so", "dll", "dylib", "lib"].contains(&ext_str) {
                    let target_file = Path::new(&target_dir).join(path.file_name().unwrap());
                    fs::copy(&path, &target_file)?;
                    if verbose {
                        println!("  {} {}", "Copied".green(), target_file.display());
                    }
                }
            }
        }
    } else {
        // Copy executable
        let exe_name = if cfg!(windows) {
            format!("{}.exe", project_name)
        } else {
            project_name.clone()
        };
        
        let built_exe = Path::new(&build_dir).join(&exe_name);
        let target_exe = Path::new(&target_dir).join(&exe_name);
        
        if built_exe.exists() {
            fs::copy(&built_exe, &target_exe)
                .context("failed to copy executable to target directory")?;
            
            #[cfg(unix)]
            {
                use std::os::unix::fs::PermissionsExt;
                let mut perms = fs::metadata(&target_exe)?.permissions();
                perms.set_mode(0o755);
                fs::set_permissions(&target_exe, perms)?;
            }
            
            if verbose {
                println!("  {} {}", "Copied".green(), target_exe.display());
            }
        }
    }

    // Symlink compile_commands.json
    let src = Path::new(&build_dir).join("compile_commands.json");
    let dst = Path::new("compile_commands.json");

    if dst.exists() {
        fs::remove_file(dst).ok();
    }

    #[cfg(unix)]
    {
        if src.exists() {
            std::os::unix::fs::symlink(&src, dst)?;
        }
    }
    #[cfg(windows)]
    {
        if src.exists() {
            std::os::windows::fs::symlink_file(&src, dst)?;
        }
    }

    pb.finish_and_clear();

    println!("{} {} built successfully [{}]", 
        "✓".green().bold(), 
        project_name.bright_yellow(),
        mode.as_str()
    );

    Ok(())
}

pub fn get_executable_path(name_opt: Option<String>, mode: BuildMode) -> Result<std::path::PathBuf> {
    let config = ProjectConfig::load()?;
    let project_name = name_opt.unwrap_or_else(|| config.name.clone());

    let exe_name = if cfg!(windows) {
        format!("{}.exe", project_name)
    } else {
        project_name
    };

    let target_dir = format!("target/{}", mode.as_str());
    Ok(Path::new(&target_dir).join(exe_name))
}

// Helper function to get number of CPUs
mod num_cpus {
    pub fn get() -> usize {
        std::thread::available_parallelism()
            .map(|n| n.get())
            .unwrap_or(1)
    }
}
