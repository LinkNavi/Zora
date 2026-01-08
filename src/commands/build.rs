use anyhow::{bail, Context, Result};
use colored::Colorize;
use indicatif::{ProgressBar, ProgressStyle};
use std::collections::HashSet;
use std::fs;
use std::path::Path;
use std::process::Command;
use tera::{Context as TeraContext, Tera};

use crate::config::ProjectConfig;

// Add BuildMode enum
#[derive(Debug, Clone, Copy)]
pub enum BuildMode {
    Dev,
    Release,
}

impl BuildMode {
    pub fn as_str(&self) -> &str {
        match self {
            BuildMode::Dev => "dev",
            BuildMode::Release => "release",
        }
    }
}

impl From<&str> for BuildMode {
    fn from(s: &str) -> Self {
        match s {
            "release" => BuildMode::Release,
            _ => BuildMode::Dev,
        }
    }
}

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
{% for package in vcpkg_packages %}
find_package({{ package }} REQUIRED)
target_link_libraries({{ name }} PRIVATE {{ package }}::{{ package }})
{% endfor %}
{% endif %}

{% if build_flags %}
target_compile_options({{ name }} PRIVATE 
{% for flag in build_flags %}
    "{{ flag }}"
{% endfor %}
)
{% endif %}

{% if defines %}
{% for key, value in defines %}
target_compile_definitions({{ name }} PRIVATE {{ key }}={{ value }})
{% endfor %}
{% endif %}

{% if link_libs %}
target_link_libraries({{ name }} PRIVATE 
{% for lib in link_libs %}
    {{ lib }}
{% endfor %}
)
{% endif %}

{% if lib_dirs %}
{% for lib_dir in lib_dirs %}
target_link_directories({{ name }} PRIVATE "{{ lib_dir }}")
{% endfor %}
{% endif %}

{% if lto %}
set_property(TARGET {{ name }} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
{% endif %}
"#;

pub fn run(
    name_opt: Option<String>,
    mode: &str,
    verbose: bool,
    jobs: Option<usize>,
    features: Vec<String>,
    all_features: bool,
    no_default_features: bool,
    target: Option<String>,
) -> Result<()> {
    if !ProjectConfig::exists() {
        bail!("project.toml not found. Run 'zora init' first.");
    }

    let config = ProjectConfig::load()?;
    let profile = config.get_profile(mode);

    let pb = ProgressBar::new_spinner();
    pb.set_style(
        ProgressStyle::default_spinner()
            .template("{spinner:.cyan} {msg}")
            .unwrap()
    );

    pb.set_message("Preparing build...");

    // Determine enabled features
    let mut enabled_features: HashSet<String> = HashSet::new();
    
    if !no_default_features {
        enabled_features.extend(config.default_features.iter().cloned());
    }
    
    if all_features {
        enabled_features.extend(config.features.keys().cloned());
    } else {
        enabled_features.extend(features.into_iter());
    }

    // Build directory
    let build_dir = format!(".build/{}", mode);
    fs::create_dir_all(&build_dir).context("failed to create build directory")?;

    let project_name = name_opt.unwrap_or_else(|| config.name.clone());

    // Prepare CMake context
    let mut ctx = TeraContext::new();
    ctx.insert("name", &project_name);
    ctx.insert("language", if config.is_cpp() { "CXX" } else { "C" });
    ctx.insert("source_dirs", &config.sources.dirs);
    ctx.insert("include_dirs", &config.includes.dirs);
    ctx.insert("is_library", &config.is_library());
    ctx.insert("use_vcpkg", &!config.deps.is_empty());
    ctx.insert("lto", &profile.lto);
    
    if config.is_cpp() && !config.std.is_empty() {
        ctx.insert("cpp_std", &config.std);
    }
    
    if !config.is_cpp() && !config.std.is_empty() {
        ctx.insert("c_std", &config.std);
    }

    // Merge profile flags with build flags
    let mut all_flags = profile.flags.clone();
    all_flags.extend(config.build.flags.clone());
    
    if !all_flags.is_empty() {
        ctx.insert("build_flags", &all_flags);
    }

    // Merge profile defines with build defines
    let mut all_defines = profile.defines.clone();
    all_defines.extend(config.build.defines.clone());
    
    // Add feature defines
    for feature in &enabled_features {
        all_defines.insert(
            format!("FEATURE_{}", feature.to_uppercase().replace("-", "_")),
            "1".to_string()
        );
    }
    
    if !all_defines.is_empty() {
        ctx.insert("defines", &all_defines);
    }

    if !config.build.libs.is_empty() {
        ctx.insert("link_libs", &config.build.libs);
    }
    if !config.build.lib_dirs.is_empty() {
        ctx.insert("lib_dirs", &config.build.lib_dirs);
    }

    if !config.deps.is_empty() {
        let packages: Vec<String> = config.deps.keys().cloned().collect();
        ctx.insert("vcpkg_packages", &packages);
    }

    pb.set_message("Generating CMake files...");

    let cmake_content = Tera::one_off(PROJECT_CMAKE_TEMPLATE, &ctx, false)
        .context("failed to render CMakeLists.txt template")?;

    let cmake_path = Path::new(&build_dir).join("CMakeLists.txt");
    fs::write(&cmake_path, cmake_content)
        .context("failed to write CMakeLists.txt")?;

    if verbose {
        println!("  {} {}", "Generated".green(), cmake_path.display());
    }

    pb.set_message("Configuring project...");

    let mut cmake_config = Command::new("cmake");
    cmake_config
        .args(&[
            "-S", &build_dir,
            "-B", &build_dir,
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            &format!("-DCMAKE_BUILD_TYPE={}", if mode == "release" { "Release" } else { "Debug" }),
        ]);

    if let Some(t) = target {
        cmake_config.arg(format!("-DCMAKE_SYSTEM_NAME={}", t));
    }

    if verbose {
        cmake_config.arg("-DCMAKE_VERBOSE_MAKEFILE=ON");
    }

    let status = cmake_config.status().context("failed to run cmake")?;

    if !status.success() {
        pb.finish_and_clear();
        bail!("CMake configuration failed");
    }

    pb.set_message(format!("Building {} [{}]...", project_name, mode));

    let mut cmake_build = Command::new("cmake");
    cmake_build.args(&["--build", &build_dir]);

    if let Some(j) = jobs {
        cmake_build.arg("-j").arg(j.to_string());
    } else {
        let num_cpus = std::thread::available_parallelism()
            .map(|n| n.get())
            .unwrap_or(1);
        cmake_build.arg("-j").arg(num_cpus.to_string());
    }

    if verbose {
        cmake_build.arg("--verbose");
    }

    let status = cmake_build.status().context("failed to run cmake build")?;

    if !status.success() {
        pb.finish_and_clear();
        bail!("Build failed");
    }

    // Copy artifacts
    let target_dir = format!("target/{}", mode);
    fs::create_dir_all(&target_dir)?;

    if config.is_library() {
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
        let exe_name = if cfg!(windows) {
            format!("{}.exe", project_name)
        } else {
            project_name.clone()
        };
        
        let built_exe = Path::new(&build_dir).join(&exe_name);
        let target_exe = Path::new(&target_dir).join(&exe_name);
        
        if built_exe.exists() {
            fs::copy(&built_exe, &target_exe)?;
            
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

    // Create compile_commands.json symlink
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

    let feature_str = if !enabled_features.is_empty() {
        format!(" with features: {}", enabled_features.iter()
            .map(|s| s.as_str())
            .collect::<Vec<_>>()
            .join(", "))
    } else {
        String::new()
    };

    println!("{} {} built successfully [{}]{}", 
        "✓".green().bold(), 
        project_name.bright_yellow(),
        mode,
        feature_str
    );

    Ok(())
}

pub fn get_executable_path(name_opt: Option<String>, mode: &str) -> Result<std::path::PathBuf> {
    let config = ProjectConfig::load()?;
    let project_name = name_opt.unwrap_or_else(|| config.name.clone());

    let exe_name = if cfg!(windows) {
        format!("{}.exe", project_name)
    } else {
        project_name
    };

    let target_dir = format!("target/{}", mode);
    Ok(Path::new(&target_dir).join(exe_name))
}
