#pragma once
#include "Zora.hpp"
#include "toml.hpp"
#include "ErrorFormatter.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace Zora {

// Parses path as a Zora.toml. On malformed TOML this used to let
// toml::parse_error (a std::runtime_error) escape uncaught, crashing the
// whole zora binary with a generic "terminate called after throwing..."
// message and no indication of what in the file was wrong. Instead, print
// a proper file:line:col diagnostic (matching compiler-error style) with a
// source snippet, and exit cleanly - a bad Zora.toml is a user mistake,
// not a Zora bug, so it shouldn't look like one.
inline ZoraFile parse_zora_file(const std::filesystem::path& path) {
    toml::table tbl;
    try {
        tbl = toml::parse_file(path.string());
    } catch (const toml::parse_error& err) {
        std::printf("\n%sFailed to parse%s %s\n\n", Color::Red, Color::Reset, path.string().c_str());
        print_toml_error(path.string(),
                          static_cast<int64_t>(err.source().begin.line),
                          static_cast<int64_t>(err.source().begin.column),
                          std::string(err.description()));
        std::exit(1);
    }

    ZoraFile zf;

    if (auto proj = tbl["project"].as_table()) {
        if (auto v = proj->get_as<std::string>("name"))    zf.name    = v->get();
        if (auto v = proj->get_as<std::string>("version")) zf.version = v->get();
    }

    if (auto ws = tbl["workspace"].as_table()) {
        if (auto members = ws->get_as<toml::array>("members"))
            for (auto& m : *members)
                if (auto s = m.as_string())
                    zf.workspace_members.push_back(s->get());
    }

    if (auto targets = tbl["target"].as_table()) {
        for (auto& [tname, tval] : *targets) {
            auto* t = tval.as_table();
            if (!t) continue;

            Target target;
            target.name = std::string(tname.str());
            if (auto v = t->get_as<std::string>("type")) target.type = v->get();
            else target.type = "bin";

            auto read_str_array = [](toml::table* tbl, const char* key,
                                     std::vector<std::string>& out) {
                if (auto arr = tbl->get_as<toml::array>(key))
                    for (auto& v : *arr)
                        if (auto s = v.as_string())
                            out.push_back(s->get());
            };

            read_str_array(t, "sources",        target.sources);
            read_str_array(t, "include_dirs",   target.include_dirs);
            read_str_array(t, "compiler_flags", target.compiler_flags);

            if (auto deps = t->get_as<toml::array>("deps")) {
                for (auto& dval : *deps) {
                    auto* d = dval.as_table();
                    if (!d) continue;
                    Dep dep;
                    if (auto v = d->get_as<std::string>("name"))   dep.name   = v->get();
                    if (auto v = d->get_as<std::string>("git"))    dep.git    = v->get();
                    if (auto v = d->get_as<std::string>("path"))   dep.path   = v->get();
                    if (auto v = d->get_as<std::string>("system")) dep.system = v->get();
                    if (auto v = d->get_as<bool>("header_only"))
                        dep.header_only = v->get() ? 1 : 0;
                    read_str_array(d, "build_args", dep.build_args);
                    target.deps.push_back(std::move(dep));
                }
            }
            zf.targets.push_back(std::move(target));
        }
    }

    if (auto scripts = tbl["scripts"].as_table()) {
        for (auto& [sname, sval] : *scripts) {
            auto* s = sval.as_table();
            if (!s) continue;

            Script script;
            script.name = std::string(sname.str());
            if (auto v = s->get_as<std::string>("path")) script.path = v->get();
            if (auto v = s->get_as<bool>("required"))    script.required = v->get();

            auto read_str_array = [](toml::table* tbl, const char* key,
                                     std::vector<std::string>& out) {
                if (auto arr = tbl->get_as<toml::array>(key))
                    for (auto& v : *arr)
                        if (auto s = v.as_string())
                            out.push_back(s->get());
            };

            read_str_array(s, "args",   script.default_args);
            read_str_array(s, "before", script.before);
            read_str_array(s, "after",  script.after);

            zf.scripts.push_back(std::move(script));
        }
    }

    return zf;
}

} // namespace Zora
