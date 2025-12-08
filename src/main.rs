use anyhow::Context;
use clap::{Parser, Subcommand};

mod commands;
mod config;

#[derive(Parser)]
#[command(name = "zora", about = "Zora — a powerful C/C++ build system", version)]
struct Cli {
    #[command(subcommand)]
    cmd: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Initialize a new zora project
    Init {
        /// Project name (defaults to current directory name)
        #[arg(short, long)]
        name: Option<String>,
        
        /// Initialize as a C++ project instead of C
        #[arg(long)]
        cpp: bool,
        
        /// Initialize as a library instead of executable
        #[arg(long)]
        lib: bool,
    },

    /// Build the project
    Build {
        /// Project name (defaults to current directory name)
        #[arg(short, long)]
        name: Option<String>,
        
        /// Build in release mode with optimizations
        #[arg(short, long)]
        release: bool,
        
        /// Enable verbose output
        #[arg(short, long)]
        verbose: bool,
        
        /// Number of parallel jobs (default: number of CPUs)
        #[arg(short, long)]
        jobs: Option<usize>,
    },

    /// Build and run the project
    Run {
        /// Project name (defaults to current directory name)
        #[arg(short, long)]
        name: Option<String>,
        
        /// Run in release mode with optimizations
        #[arg(short, long)]
        release: bool,

        /// Arguments to pass to the program
        #[arg(trailing_var_arg = true, allow_hyphen_values = true)]
        args: Vec<String>,
    },

    /// Add vcpkg packages to the project
    Add {
        /// Package names to install from vcpkg
        packages: Vec<String>,
    },

    /// Remove vcpkg packages from the project
    Remove {
        /// Package names to remove
        packages: Vec<String>,
    },

    /// Clean build artifacts
    Clean {
        /// Also clean vcpkg packages
        #[arg(long)]
        all: bool,
    },

    /// Run tests
    Test {
        /// Run in release mode
        #[arg(short, long)]
        release: bool,
        
        /// Run specific test
        #[arg(short, long)]
        test: Option<String>,
    },

    /// Check project without building
    Check {
        /// Enable verbose output
        #[arg(short, long)]
        verbose: bool,
    },

    /// Format source code using clang-format
    Fmt {
        /// Check formatting without modifying files
        #[arg(long)]
        check: bool,
    },

    /// Lint source code using clang-tidy
    Lint {
        /// Automatically fix issues
        #[arg(long)]
        fix: bool,
    },

    /// Show project information
    Info,

    /// List all dependencies
    Deps {
        /// Show dependency tree
        #[arg(long)]
        tree: bool,
    },

    /// Create a new source file
    New {
        /// File type (source, header, test)
        #[arg(value_name = "TYPE")]
        file_type: String,
        
        /// File name
        #[arg(value_name = "NAME")]
        name: String,
    },

    /// Benchmark the project
    Bench {
        /// Run specific benchmark
        #[arg(short, long)]
        bench: Option<String>,
    },

    /// Generate documentation
    Doc {
        /// Open documentation in browser
        #[arg(long)]
        open: bool,
    },

    /// Watch for changes and rebuild
    Watch {
        /// Command to run on changes (build, test, run)
        #[arg(default_value = "build")]
        command: String,
    },

    /// Package the project for distribution
    Package {
        /// Output format (tar, zip)
        #[arg(short, long, default_value = "tar")]
        format: String,
    },

    /// Install the built executable
    Install {
        /// Installation directory
        #[arg(long)]
        prefix: Option<String>,
    },

    /// Update vcpkg packages
    Update {
        /// Update specific packages only
        packages: Vec<String>,
    },

    /// Show build cache statistics
    Cache {
        #[command(subcommand)]
        action: CacheAction,
    },
}

#[derive(Subcommand)]
enum CacheAction {
    /// Show cache statistics
    Stats,
    /// Clear the cache
    Clear,
}

fn main() -> anyhow::Result<()> {
    let cli = Cli::parse();

    match cli.cmd {
        Commands::Init { name, cpp, lib } => {
            commands::init::run(name, cpp, lib)?
        },
        
        Commands::Build { name, release, verbose, jobs } => {
            let mode = if release {
                commands::build::BuildMode::Release
            } else {
                commands::build::BuildMode::Debug
            };
            commands::build::run(name, mode, verbose, jobs)?
        },
        
        Commands::Run { name, release, args } => {
            let mode = if release {
                commands::build::BuildMode::Release
            } else {
                commands::build::BuildMode::Debug
            };
            commands::run::run(name, mode, false, None, args)?
        },

        Commands::Add { packages } => {
            commands::add::run(packages)?
        },

        Commands::Remove { packages } => {
            commands::remove::run(packages)?
        },

        Commands::Clean { all } => {
            commands::clean::run(all)?
        },

        Commands::Test { release, test } => {
            let mode = if release {
                commands::build::BuildMode::Release
            } else {
                commands::build::BuildMode::Debug
            };
            commands::test::run(mode, test)?
        },

        Commands::Check { verbose } => {
            commands::check::run(verbose)?
        },

        Commands::Fmt { check } => {
            commands::fmt::run(check)?
        },

        Commands::Lint { fix } => {
            commands::lint::run(fix)?
        },

        Commands::Info => {
            commands::info::run()?
        },

        Commands::Deps { tree } => {
            commands::deps::run(tree)?
        },

        Commands::New { file_type, name } => {
            commands::new::run(&file_type, &name)?
        },

        Commands::Bench { bench } => {
            commands::bench::run(bench)?
        },

        Commands::Doc { open } => {
            commands::doc::run(open)?
        },

        Commands::Watch { command } => {
            commands::watch::run(&command)?
        },

        Commands::Package { format } => {
            commands::package::run(&format)?
        },

        Commands::Install { prefix } => {
            commands::install::run(prefix)?
        },

        Commands::Update { packages } => {
            commands::update::run(packages)?
        },

        Commands::Cache { action } => {
            match action {
                CacheAction::Stats => commands::cache::stats()?,
                CacheAction::Clear => commands::cache::clear()?,
            }
        },
    }

    Ok(())
}