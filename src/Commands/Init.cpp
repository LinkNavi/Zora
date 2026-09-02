#include "Commands.hpp"
#include "Logger.hpp"
#include "TUI.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static void write_main(const fs::path& src_dir, const std::string& name, bool hylian) {
    fs::create_directories(src_dir);
    if (hylian) {
        std::ofstream f(src_dir / "main.hy");
        f << "include {\n"
          << "    std.io,\n"
          << "}\n\n"
          << "void main() {\n"
          << "    println(\"Hello from " << name << "!\");\n"
          << "}\n";
        return;
    }
    std::ofstream f(src_dir / "main.cpp");
    f << "#include <cstdio>\n\n"
      << "int main() {\n"
      << "    std::puts(\"Hello from " << name << "!\");\n"
      << "    return 0;\n"
      << "}\n";
}

static void write_zora_toml(const fs::path& dir, const std::string& name,
                             bool is_workspace_root = false, bool hylian = false) {
    std::ofstream f(dir / "Zora.toml");
    if (is_workspace_root) {
        f << "[workspace]\n"
          << "members = []\n";
    } else if (hylian) {
        f << "[project]\n"
          << "name = \"" << name << "\"\n"
          << "version = \"0.1.0\"\n\n"
          << "[target." << name << "]\n"
          << "type = \"bin\"\n"
          << "# Hylian compiles from a single entry point and follows its own\n"
          << "# include{} graph from there - list just the entry file, not a glob.\n"
          << "sources = [\"src/main.hy\"]\n";
    } else {
        f << "[project]\n"
          << "name = \"" << name << "\"\n"
          << "version = \"0.1.0\"\n\n"
          << "[target." << name << "]\n"
          << "type = \"bin\"\n"
          << "sources = [\"src/**/*.cpp\"]\n"
          << "include_dirs = [\"include\"]\n";
    }
}

static void scaffold_project(const fs::path& dir, const std::string& name, bool hylian = false) {
    fs::create_directories(dir / "src");
    write_zora_toml(dir, name, false, hylian);
    write_main(dir / "src", name, hylian);
    std::printf("  \033[1;32m✓\033[0m %s/Zora.toml\n", dir.filename().c_str());
    if (hylian) {
        std::printf("  \033[1;32m✓\033[0m %s/src/main.hy\n", dir.filename().c_str());
        return;
    }
    fs::create_directories(dir / "include");
    std::printf("  \033[1;32m✓\033[0m %s/src/main.cpp\n", dir.filename().c_str());
    std::printf("  \033[1;32m✓\033[0m %s/include/\n", dir.filename().c_str());
}

// find nearest workspace root by walking up from dir
static fs::path find_workspace_root(const fs::path& from) {
    fs::path cur = fs::absolute(from);
    while (cur != cur.parent_path()) {
        fs::path toml = cur / "Zora.toml";
        if (fs::exists(toml)) {
            std::ifstream f(toml);
            std::string line;
            while (std::getline(f, line))
                if (line.find("[workspace]") != std::string::npos)
                    return cur;
        }
        cur = cur.parent_path();
    }
    return "";
}

// add member to workspace Zora.toml
static void add_workspace_member(const fs::path& ws_root,
                                  const std::string& member_rel) {
    fs::path toml_path = ws_root / "Zora.toml";
    std::ifstream in(toml_path);
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    in.close();

    // find members = [...] and append
    auto pos = content.find("members = [");
    if (pos == std::string::npos) {
        // just append
        std::ofstream f(toml_path, std::ios::app);
        f << "members = [\"" << member_rel << "\"]\n";
        return;
    }

    auto close = content.find(']', pos);
    if (close == std::string::npos) return;

    // check if empty
    std::string between = content.substr(pos + 11, close - pos - 11);
    bool empty = true;
    for (char c : between) if (!std::isspace(c) && c != '\n') { empty = false; break; }

    std::string insert = empty
        ? "\"" + member_rel + "\""
        : ", \"" + member_rel + "\"";

    content.insert(close, insert);
    std::ofstream out(toml_path);
    out << content;
}

void cmd_init(const Zora::Workspace& ws, int argc, char** argv) {
    fs::path cwd = fs::current_path();
    std::string name = cwd.filename().string();

    bool force_workspace = false;
    bool hylian = false;
    for (int i = 0; i < argc; i++) {
        if (std::strcmp(argv[i], "--workspace") == 0) force_workspace = true;
        if (std::strcmp(argv[i], "--hylian") == 0)   hylian = true;
    }

    if (fs::exists(cwd / "Zora.toml")) {
        std::puts("error: Zora.toml already exists in current directory");
        return;
    }

    if (force_workspace) {
        write_zora_toml(cwd, name, true);
        std::printf("  \033[1;32m✓\033[0m Zora.toml (workspace root)\n");
        std::printf("\033[1mInitialized workspace:\033[0m %s\n", name.c_str());
        return;
    }

    // check if inside a workspace
    fs::path ws_root = find_workspace_root(cwd.parent_path());
    if (!ws_root.empty()) {
        std::string rel = fs::relative(cwd, ws_root).string();
        int choice = Zora::tui_select(
            "Found workspace at " + ws_root.filename().string() + ". Add as member?",
            {"Yes — add to workspace", "No — standalone project"});
        if (choice == 0) {
            scaffold_project(cwd, name, hylian);
            add_workspace_member(ws_root, rel);
            std::printf("\033[1mAdded\033[0m %s to workspace %s\n",
                name.c_str(), ws_root.filename().c_str());
            return;
        }
    }

    scaffold_project(cwd, name, hylian);
    std::printf("\033[1mInitialized project:\033[0m %s\n", name.c_str());
}

void cmd_new(const Zora::Workspace&, int argc, char** argv) {
    // ── step 1: name ──────────────────────────────────────────────────────────
    std::string name;
    if (argc >= 1 && argv[0][0] != '-') {
        name = argv[0];
    } else {
        std::printf("\033[1mProject name:\033[0m ");
        std::fflush(stdout);
        std::getline(std::cin, name);
        if (name.empty()) { std::puts("error: name cannot be empty"); return; }
    }

    // ── step 2: type ──────────────────────────────────────────────────────────
    int type = Zora::tui_select("Project type:",
        {"Single project", "Workspace member", "Full workspace"});
    if (type < 0) { std::puts("Cancelled."); return; }

    // ── step 2b: language (a full workspace has no sources of its own,
    // so there's nothing to ask about) ────────────────────────────────────────
    bool hylian = false;
    if (type != 2) {
        int lang = Zora::tui_select("Language:", {"C++", "Hylian"});
        if (lang < 0) { std::puts("Cancelled."); return; }
        hylian = (lang == 1);
    }

    fs::path target_dir = fs::current_path() / name;
    if (fs::exists(target_dir)) {
        std::printf("error: '%s' already exists\n", name.c_str());
        return;
    }

    // ── single project ────────────────────────────────────────────────────────
    if (type == 0) {
        fs::create_directories(target_dir);
        scaffold_project(target_dir, name, hylian);
        std::printf("\n\033[1mCreated project:\033[0m %s/\n", name.c_str());
        return;
    }

    // ── full workspace ────────────────────────────────────────────────────────
    if (type == 2) {
        fs::create_directories(target_dir);
        write_zora_toml(target_dir, name, true);
        std::printf("  \033[1;32m✓\033[0m %s/Zora.toml (workspace root)\n", name.c_str());
        std::printf("\n\033[1mCreated workspace:\033[0m %s/\n", name.c_str());
        std::printf("Add members with: cd %s && zora new\n", name.c_str());
        return;
    }

    // ── workspace member ──────────────────────────────────────────────────────
    // find nearby workspaces
    std::vector<fs::path> workspaces;
    fs::path cwd = fs::current_path();

    // check current dir and parents
    fs::path cur = cwd;
    while (cur != cur.parent_path()) {
        if (fs::exists(cur / "Zora.toml")) {
            std::ifstream f(cur / "Zora.toml");
            std::string line;
            while (std::getline(f, line))
                if (line.find("[workspace]") != std::string::npos)
                    { workspaces.push_back(cur); break; }
        }
        cur = cur.parent_path();
    }

    fs::path ws_root;
    if (workspaces.empty()) {
        int choice = Zora::tui_select("No workspace found. Create one?",
            {"Create new workspace here", "Cancel"});
        if (choice != 0) { std::puts("Cancelled."); return; }

        // create workspace in cwd, then add member inside it
        write_zora_toml(cwd, cwd.filename().string(), true);
        std::printf("  \033[1;32m✓\033[0m Zora.toml (workspace root)\n");
        ws_root = cwd;
    } else if (workspaces.size() == 1) {
        ws_root = workspaces[0];
        std::printf("Using workspace: %s\n", ws_root.filename().c_str());
    } else {
        std::vector<std::string> labels;
        for (auto& w : workspaces)
            labels.push_back(fs::relative(w, cwd).string());
        int idx = Zora::tui_select("Select workspace:", labels);
        if (idx < 0) { std::puts("Cancelled."); return; }
        ws_root = workspaces[idx];
    }

    // create member inside workspace
    fs::path member_dir = ws_root / name;
    if (fs::exists(member_dir)) {
        std::printf("error: '%s' already exists in workspace\n", name.c_str());
        return;
    }

    fs::create_directories(member_dir);
    scaffold_project(member_dir, name, hylian);
    add_workspace_member(ws_root, name);

    std::printf("\n\033[1mCreated member:\033[0m %s in workspace %s/\n",
        name.c_str(), ws_root.filename().c_str());
}
