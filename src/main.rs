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
        #[arg(short, long)]
        name: Option<String>,
        #[arg(long)]
        cpp: bool,
        #[arg(long)]
        lib: bool,
    },

    /// Create a new zora project
    New {
        path: String,
        #[arg(long)]
        cpp: bool,
        #[arg(long)]
        lib: bool,
        #[arg(long)]
        name: Option<String>,
    },

    /// Build the project
    Build {
        #[arg(short, long)]
        name: Option<String>,
        #[arg(short, long)]
        release: bool,
        #[arg(long)]
        profile: Option<String>,
        #[arg(short, long)]
        verbose: bool,
        #[arg(short, long)]
        jobs: Option<usize>,
        #[arg(long)]
        features: Vec<String>,
        #[arg(long)]
        all_features: bool,
        #[arg(long)]
        no_default_features: bool,
        #[arg(long)]
        target: Option<String>,
    },

    /// Build and run the project
    Run {
        #[arg(short, long)]
        name: Option<String>,
        #[arg(short, long)]
        release: bool,
        #[arg(long)]
        profile: Option<String>,
        #[arg(long)]
        features: Vec<String>,
        #[arg(trailing_var_arg = true, allow_hyphen_values = true)]
        args: Vec<String>,
    },

    /// Add vcpkg packages to the project
    Add {
        packages: Vec<String>,
        #[arg(long)]
        dev: bool,
        #[arg(long)]
        optional: bool,
        #[arg(long)]
        features: Vec<String>,
    },

    /// Remove vcpkg packages from the project
    Remove {
        packages: Vec<String>,
        #[arg(long)]
        dev: bool,
    },

    /// Clean build artifacts
    Clean {
        #[arg(long)]
        all: bool,
        #[arg(long)]
        release: bool,
        #[arg(long)]
        target_dir: bool,
    },

    /// Run tests
    Test {
        #[arg(short, long)]
        release: bool,
        #[arg(short, long)]
        test: Option<String>,
        #[arg(long)]
        no_fail_fast: bool,
        #[arg(trailing_var_arg = true)]
        testargs: Vec<String>,
    },

    /// Check project without building
    Check {
        #[arg(short, long)]
        verbose: bool,
        #[arg(long)]
        all_features: bool,
    },

    /// Format source code using clang-format
    Fmt {
        #[arg(long)]
        check: bool,
        #[arg(long)]
        all: bool,
    },

    /// Lint source code using clang-tidy
    Lint {
        #[arg(long)]
        fix: bool,
        #[arg(long)]
        allow: Vec<String>,
        #[arg(long)]
        deny: Vec<String>,
    },

    /// Show project information
    Info,

    /// List all dependencies
    Deps {
        #[arg(long)]
        tree: bool,
    },

    /// Search for packages in vcpkg
    Search {
        query: String,
    },

    /// Create a new source file
    New_ {
        #[arg(value_name = "TYPE")]
        file_type: String,
        #[arg(value_name = "NAME")]
        name: String,
    },

    /// Benchmark the project
    Bench {
        #[arg(short, long)]
        bench: Option<String>,
        #[arg(trailing_var_arg = true)]
        args: Vec<String>,
    },

    /// Generate documentation
    Doc {
        #[arg(long)]
        open: bool,
        #[arg(long)]
        no_deps: bool,
    },

    /// Watch for changes and rebuild
    Watch {
        #[arg(default_value = "build")]
        command: String,
        #[arg(short, long)]
        clear: bool,
    },

    /// Package the project for distribution
    Package {
        #[arg(short, long, default_value = "tar")]
        format: String,
        #[arg(long)]
        no_verify: bool,
    },

    /// Install the built executable
    Install {
        #[arg(long)]
        prefix: Option<String>,
        #[arg(long)]
        root: Option<String>,
    },

    /// Uninstall the executable
    Uninstall {
        #[arg(long)]
        prefix: Option<String>,
    },

    /// Update vcpkg packages
    Update {
        packages: Vec<String>,
        #[arg(long)]
        dry_run: bool,
    },

    /// Show build cache statistics
    Cache {
        #[command(subcommand)]
        action: CacheAction,
    },

    /// Display version and project info
    Version {
        #[arg(long)]
        verbose: bool,
    },

    /// Create or work with workspaces
    Workspace {
        #[command(subcommand)]
        action: WorkspaceAction,
    },

    /// Manage project features
    Features {
        #[command(subcommand)]
        action: FeatureAction,
    },

    /// Run arbitrary scripts
    Script {
        name: String,
    },

    /// Publish package to registry
    Publish {
        #[arg(long)]
        dry_run: bool,
        #[arg(long)]
        registry: Option<String>,
    },

    /// Verify project integrity
    Verify {
        #[arg(long)]
        locked: bool,
    },

    /// Generate shell completions
    Completions {
        shell: String,
    },

    /// Expand macros or show expanded code
    Expand {
        file: Option<String>,
    },

    /// Show build tree
    Tree {
        #[arg(long)]
        depth: Option<usize>,
    },
}

#[derive(Subcommand)]
enum CacheAction {
    Stats,
    Clear,
    Prune,
}

#[derive(Subcommand)]
enum WorkspaceAction {
    Init,
    Add { path: String },
    Remove { path: String },
    List,
}

#[derive(Subcommand)]
enum FeatureAction {
    List,
    Enable { features: Vec<String> },
    Disable { features: Vec<String> },
}

fn main() -> anyhow::Result<()> {
    let cli = Cli::parse();

    match cli.cmd {
        Commands::Init { name, cpp, lib } => {
            commands::init::run(name, cpp, lib)?
        },

        Commands::New { path, cpp, lib, name } => {
            commands::new_project::run(path, cpp, lib, name)?
        },
        
        Commands::Build { name, release, profile, verbose, jobs, features, all_features, no_default_features, target } => {
            let mode = profile.as_deref()
                .or(if release { Some("release") } else { Some("dev") })
                .unwrap();
            commands::build::run(name, mode, verbose, jobs, features, all_features, no_default_features, target)?
        },
        
        Commands::Run { name, release, profile, features, args } => {
            let mode = profile.as_deref()
                .or(if release { Some("release") } else { Some("dev") })
                .unwrap();
            commands::run::run(name, mode, features, args)?
        },

        Commands::Add { packages, dev, optional, features } => {
            commands::add::run(packages, dev, optional, features)?
        },

        Commands::Remove { packages, dev } => {
            commands::remove::run(packages, dev)?
        },

        Commands::Clean { all, release, target_dir } => {
            commands::clean::run(all, release, target_dir)?
        },

        Commands::Test { release, test, no_fail_fast, testargs } => {
            let mode = if release { "release" } else { "dev" };
            commands::test::run(mode, test, no_fail_fast, testargs)?
        },

        Commands::Check { verbose, all_features } => {
            commands::check::run(verbose, all_features)?
        },

        Commands::Fmt { check, all } => {
            commands::fmt::run(check, all)?
        },

        Commands::Lint { fix, allow, deny } => {
            commands::lint::run(fix, allow, deny)?
        },

        Commands::Info => {
            commands::info::run()?
        },

        Commands::Deps { tree } => {
            commands::deps::run(tree)?
        },

        Commands::Search { query } => {
            commands::search::run(query)?
        },

        Commands::New_ { file_type, name } => {
            commands::new::run(&file_type, &name)?
        },

        Commands::Bench { bench, args } => {
            commands::bench::run(bench, args)?
        },

        Commands::Doc { open, no_deps } => {
            commands::doc::run(open, no_deps)?
        },

        Commands::Watch { command, clear } => {
            commands::watch::run(&command, clear)?
        },

        Commands::Package { format, no_verify } => {
            commands::package::run(&format, no_verify)?
        },

        Commands::Install { prefix, root } => {
            commands::install::run(prefix.or(root))?
        },

        Commands::Uninstall { prefix } => {
            commands::uninstall::run(prefix)?
        },

        Commands::Update { packages, dry_run } => {
            commands::update::run(packages, dry_run)?
        },

        Commands::Cache { action } => {
            match action {
                CacheAction::Stats => commands::cache::stats()?,
                CacheAction::Clear => commands::cache::clear()?,
                CacheAction::Prune => commands::cache::prune()?,
            }
        },

        Commands::Version { verbose } => {
            commands::version::run(verbose)?
        },

        Commands::Workspace { action } => {
            match action {
                WorkspaceAction::Init => commands::workspace::init()?,
                WorkspaceAction::Add { path } => commands::workspace::add(path)?,
                WorkspaceAction::Remove { path } => commands::workspace::remove(path)?,
                WorkspaceAction::List => commands::workspace::list()?,
            }
        },

        Commands::Features { action } => {
            match action {
                FeatureAction::List => commands::features::list()?,
                FeatureAction::Enable { features } => commands::features::enable(features)?,
                FeatureAction::Disable { features } => commands::features::disable(features)?,
            }
        },

        Commands::Script { name } => {
            commands::script::run(name)?
        },

        Commands::Publish { dry_run, registry } => {
            commands::publish::run(dry_run, registry)?
        },

        Commands::Verify { locked } => {
            commands::verify::run(locked)?
        },

        Commands::Completions { shell } => {
            commands::completions::run(shell)?
        },

        Commands::Expand { file } => {
            commands::expand::run(file)?
        },

        Commands::Tree { depth } => {
            commands::tree::run(depth)?
        },
    }

    Ok(())
}
