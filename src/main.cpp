#include "Commands.hpp"
#include "Workspace.hpp"
#include "Builder.hpp"
#include "Script.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (std::strncmp(argv[i], "--debug=", 8) == 0)
            Zora::set_log_level(std::atoi(argv[i] + 8));
        else if (std::strcmp(argv[i], "--debug") == 0 && i + 1 < argc)
            Zora::set_log_level(std::atoi(argv[++i]));
    }

    if (argc < 2) { printHelp(); return 1; }

    // init and new run before Zora.toml check
    if (std::strcmp(argv[1], "init") == 0) {
        Zora::Workspace empty{};
        cmd_init(empty, argc - 2, argv + 2);
        return 0;
    }
    if (std::strcmp(argv[1], "new") == 0) {
        Zora::Workspace empty{};
        cmd_new(empty, argc - 2, argv + 2);
        return 0;
    }

    if (!fs::exists("Zora.toml")) {
        std::puts("error: no Zora.toml found in current directory");
        return 1;
    }

    auto ws = Zora::load_workspace(fs::absolute("Zora.toml"));
    const char* cmd = argv[1];

    // parse -p flag shared by build, clean, fetch
    std::string only;
    for (int i = 2; i < argc; i++)
        if (std::strcmp(argv[i], "-p") == 0 && i + 1 < argc)
            only = argv[++i];

    if (std::strcmp(cmd, "help") == 0) {
        printHelp();

    } else if (std::strcmp(cmd, "glob") == 0) {
        printGlob(ws);

    } else if (std::strcmp(cmd, "build") == 0) {
        if (!run_hook(ws, "build", true)) return 1;
        bool ok = Zora::build_workspace(ws, only);
        run_hook(ws, "build", false);
        return ok ? 0 : 1;

    } else if (std::strcmp(cmd, "clean") == 0) {
        if (!run_hook(ws, "clean", true)) return 1;
        clean(ws, only);
        run_hook(ws, "clean", false);

    } else if (std::strcmp(cmd, "run") == 0) {
        const char* target_name = nullptr;
        int extra_start = argc;
        for (int i = 2; i < argc; i++) {
            if (std::strcmp(argv[i], "--") == 0) { extra_start = i + 1; break; }
            if (argv[i][0] != '-' && !target_name) target_name = argv[i];
        }
        if (!run_hook(ws, "run", true)) return 1;
        run(ws, target_name, argc - extra_start, argv + extra_start);
        run_hook(ws, "run", false);

    } else if (std::strcmp(cmd, "fetch") == 0) {
        if (!run_hook(ws, "fetch", true)) return 1;
        fetch(ws, only);
        run_hook(ws, "fetch", false);

    } else if (std::strcmp(cmd, "script") == 0) {
        cmd_script(ws, argc - 2, argv + 2);

    } else if (std::strcmp(cmd, "dep") == 0) {
        dep_cmd(ws, argc - 2, argv + 2);

    } else {
        std::printf("error: unknown command '%s'\n", cmd);
        return 1;
    }

    return 0;
}
