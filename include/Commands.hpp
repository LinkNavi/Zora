#pragma once
#include "Workspace.hpp"
#include <string>

void printHelp();
void printGlob(const Zora::Workspace& ws);
void clean(const Zora::Workspace& ws, const std::string& only = "");
void run(const Zora::Workspace& ws, const char* target_name, int extra_argc, char** extra_argv);
void fetch(const Zora::Workspace& ws, const std::string& only = "");
void cmd_init(const Zora::Workspace& ws, int argc, char** argv);
void cmd_new(const Zora::Workspace& ws, int argc, char** argv);
void cmd_script(const Zora::Workspace& ws, int argc, char** argv);
bool run_hook(const Zora::Workspace& ws, const std::string& hook, bool is_before);
void dep_cmd(const Zora::Workspace& ws, int argc, char** argv);
