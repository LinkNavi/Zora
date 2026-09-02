#pragma once
#include "Logger.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

namespace Zora {

namespace fs = std::filesystem;

// ── terminal helpers
// ──────────────────────────────────────────────────────────

struct RawTerm {
  struct termios old;
  RawTerm() {
    tcgetattr(STDIN_FILENO, &old);
    struct termios raw = old;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
  }
  ~RawTerm() { tcsetattr(STDIN_FILENO, TCSANOW, &old); }
};

inline char read_key() {
  char c = 0;
  read(STDIN_FILENO, &c, 1);
  return c;
}

// returns 'U'=up 'D'=down 'R'=right 'L'=left, or the raw char
inline char read_action() {
  char c = read_key();
  if (c == '\033') {
    char s[2];
    read(STDIN_FILENO, &s[0], 1);
    read(STDIN_FILENO, &s[1], 1);
    if (s[0] == '[') {
      if (s[1] == 'A')
        return 'U';
      if (s[1] == 'B')
        return 'D';
      if (s[1] == 'C')
        return 'R';
      if (s[1] == 'D')
        return 'L';
    }
    return 0;
  }
  return c;
}

// ── header-only detection
// ─────────────────────────────────────────────────────

inline bool is_header_ext(const fs::path &p) {
  auto e = p.extension().string();
  return e == ".hpp" || e == ".h" || e == ".hh" || e == ".tpp" || e == ".inl";
}

inline bool dir_is_headers_only(const fs::path &dir) {
  std::error_code ec;
  for (auto &e : fs::recursive_directory_iterator(dir, ec))
    if (e.is_regular_file() && !is_header_ext(e.path()))
      return false;
  return true;
}

inline bool repo_is_header_only(const fs::path &repo) {
  // no CMakeLists or meson.build at root
  return !fs::exists(repo / "CMakeLists.txt") &&
         !fs::exists(repo / "meson.build");
}

// returns predicted candidate include dirs
inline std::vector<fs::path> predict_include_dirs(const fs::path &repo) {
  std::vector<fs::path> candidates;
  static const char *known_names[] = {"include",        "inc",     "headers",
                                      "single_include", "Include", nullptr};

  std::error_code ec;
  for (auto &e : fs::directory_iterator(repo, ec)) {
    if (!e.is_directory())
      continue;
    auto name = e.path().filename().string();
    for (int i = 0; known_names[i]; i++)
      if (name == known_names[i]) {
        candidates.push_back(e.path());
        break;
      }
    if (dir_is_headers_only(e.path()))
      if (std::find(candidates.begin(), candidates.end(), e.path()) ==
          candidates.end())
        candidates.push_back(e.path());
  }

  // headers in repo root
  bool root_has_headers = false;
  for (auto &e : fs::directory_iterator(repo, ec))
    if (e.is_regular_file() && is_header_ext(e.path())) {
      root_has_headers = true;
      break;
    }
  if (root_has_headers)
    candidates.push_back(repo);

  return candidates;
}

// ── file browser TUI
// ──────────────────────────────────────────────────────────

struct BrowserEntry {
  fs::path path;
  bool is_dir;
  bool marked = false;
};

inline std::vector<BrowserEntry> list_dir(const fs::path &dir) {
  std::vector<BrowserEntry> entries;
  std::error_code ec;
  for (auto &e : fs::directory_iterator(dir, ec)) {
    bool is_dir = e.is_directory();
    bool is_hdr = e.is_regular_file() && is_header_ext(e.path());
    if (is_dir || is_hdr)
      entries.push_back({e.path(), is_dir, false});
  }
  std::sort(entries.begin(), entries.end(), [](auto &a, auto &b) {
    if (a.is_dir != b.is_dir)
      return a.is_dir > b.is_dir;
    return a.path.filename() < b.path.filename();
  });
  return entries;
}

struct BrowserState {
  fs::path repo_root;
  fs::path current_dir;
  std::vector<BrowserEntry> entries;
  int cursor = 0;
  int scroll = 0;
  std::vector<fs::path> marked; // selected include roots
  std::vector<fs::path> predicted;
  bool show_predicted = false;
  std::string search;
  bool searching = false;
};

static const int VISIBLE_ROWS = 12;

inline void browser_render(BrowserState &s) {
  // clear
  std::printf("\033[H\033[J");

  // header
  std::printf(
      "\033[1mSelect include dirs for dep \033[1;36m%s\033[0m\033[1m:\033[0m\n",
      s.repo_root.filename().c_str());

  // breadcrumb
  auto rel = fs::relative(s.current_dir, s.repo_root);
  std::printf("  \033[2m/\033[0m %s\n", rel == "." ? "" : rel.c_str());

  // predicted strip
  if (!s.predicted.empty()) {
    std::printf("  \033[2m[p] predicted:");
    for (auto &p : s.predicted)
      std::printf(" %s", fs::relative(p, s.repo_root).c_str());
    std::printf("\033[0m\n");
  }

  std::printf("  \033[2m────────────────────────────────────────────\033[0m\n");

  // filter entries by search
  std::vector<BrowserEntry *> visible;
  for (auto &e : s.entries) {
    auto name = e.path.filename().string();
    if (!s.search.empty() && name.find(s.search) == std::string::npos)
      continue;
    visible.push_back(&e);
  }

  // clamp scroll
  if (s.cursor >= s.scroll + VISIBLE_ROWS)
    s.scroll = s.cursor - VISIBLE_ROWS + 1;
  if (s.cursor < s.scroll)
    s.scroll = s.cursor;

  int shown = 0;
  for (int i = s.scroll; i < (int)visible.size() && shown < VISIBLE_ROWS;
       i++, shown++) {
    auto &e = *visible[i];
    auto name = e.path.filename().string();
    bool sel = i == s.cursor;
    bool marked =
        std::find(s.marked.begin(), s.marked.end(), e.path) != s.marked.end();

    const char *cursor_str = sel ? "\033[1;36m❯\033[0m" : " ";
    const char *mark_str = marked ? "\033[1;32m󰄬 \033[0m" : "  ";
    const char *icon = e.is_dir ? "\033[33m\033[0m " : "\033[34m\033[0m ";
    const char *name_col =
        sel ? "\033[1;37m" : (e.is_dir ? "\033[33m" : "\033[0m");

    std::printf("  %s %s%s %s%s\033[0m%s\n", cursor_str, mark_str, icon,
                name_col, name.c_str(), e.is_dir ? "/" : "");
  }

  if (visible.empty())
    std::printf("  \033[2m(empty)\033[0m\n");

  std::printf("  \033[2m────────────────────────────────────────────\033[0m\n");

  // marked list
  if (!s.marked.empty()) {
    std::printf("  \033[1;32mMarked:\033[0m");
    for (auto &m : s.marked)
      std::printf(" %s", fs::relative(m, s.repo_root).c_str());
    std::printf("\n");
  }

  // hint bar
  if (s.searching)
    std::printf("  \033[1m/\033[0m %s_\n", s.search.c_str());
  else
    std::printf("  \033[2m[↑↓] move  [→] open  [←] back  [space] mark  [/] "
                "search  [p] predicted  [enter] confirm  [q] cancel\033[0m\n");

  std::fflush(stdout);
}

// run the browser, return marked dirs (empty = cancelled)
inline std::vector<fs::path>
browse_include_dirs(const fs::path &repo,
                    const std::vector<fs::path> &predicted) {
  BrowserState s;
  s.repo_root = repo;
  s.current_dir = repo;
  s.entries = list_dir(repo);
  s.predicted = predicted;
  s.marked = predicted; // start with predicted pre-marked

  std::vector<fs::path> dir_stack; // breadcrumb stack

  RawTerm raw;
  std::printf("\033[?25l"); // hide cursor
  std::printf("\033[2J");

  auto get_visible = [&]() -> std::vector<BrowserEntry *> {
    std::vector<BrowserEntry *> v;
    for (auto &e : s.entries)
      if (s.search.empty() ||
          e.path.filename().string().find(s.search) != std::string::npos)
        v.push_back(&e);
    return v;
  };

  while (true) {
    browser_render(s);
    char a = read_action();

    auto visible = get_visible();
    if (visible.empty() && a != 'L' && a != 'p' && a != '\n' && a != 'q')
      continue;

    if (s.searching) {
      if (a == '\n' || a == 27) {
        s.searching = false;
      } else if (a == 127 && !s.search.empty())
        s.search.pop_back();
      else if (a >= 32 && a < 127) {
        s.search += a;
        s.cursor = 0;
      }
      continue;
    }

    if (a == 'U') {
      if (s.cursor > 0)
        s.cursor--;
    } else if (a == 'D') {
      if (s.cursor < (int)visible.size() - 1)
        s.cursor++;
    } else if (a == 'R') {
      // drill in
      if (!visible.empty() && visible[s.cursor]->is_dir) {
        dir_stack.push_back(s.current_dir);
        s.current_dir = visible[s.cursor]->path;
        s.entries = list_dir(s.current_dir);
        s.cursor = 0;
        s.scroll = 0;
        s.search.clear();
      }
    } else if (a == 'L') {
      // go up
      if (!dir_stack.empty()) {
        s.current_dir = dir_stack.back();
        dir_stack.pop_back();
        s.entries = list_dir(s.current_dir);
        s.cursor = 0;
        s.scroll = 0;
        s.search.clear();
      }
    } else if (a == ' ') {
      // toggle mark on current entry
      if (!visible.empty()) {
        auto &path = visible[s.cursor]->path;
        auto it = std::find(s.marked.begin(), s.marked.end(), path);
        if (it != s.marked.end())
          s.marked.erase(it);
        else
          s.marked.push_back(path);
      }
    } else if (a == 'p') {
      // jump to predicted
      s.marked = predicted;
      s.current_dir = repo;
      s.entries = list_dir(repo);
      s.cursor = 0;
      s.scroll = 0;
      s.search.clear();
    } else if (a == '/') {
      s.searching = true;
      s.search.clear();
      s.cursor = 0;
    } else if (a == '\n') {
      break; // confirm
    } else if (a == 'q' || a == 3) {
      s.marked.clear();
      break; // cancel
    }
  }

  std::printf("\033[?25h\033[2J\033[H"); // show cursor, clear
  return s.marked;
}

// ── install header-only dep
// ───────────────────────────────────────────────────

// flatten: if dir contains exactly one subdir and no loose headers, return that
// subdir
inline fs::path maybe_flatten(const fs::path &dir) {
  std::error_code ec;
  std::vector<fs::path> subdirs;
  bool has_loose = false;
  for (auto &e : fs::directory_iterator(dir, ec)) {
    if (e.is_directory())
      subdirs.push_back(e.path());
    else if (e.is_regular_file() && is_header_ext(e.path()))
      has_loose = true;
  }
  if (!has_loose && subdirs.size() == 1)
    return subdirs[0];
  return dir;
}

inline void copy_headers(const fs::path &src_dir, const fs::path &dest_dir) {
  std::error_code ec;
  fs::create_directories(dest_dir, ec);
  for (auto &e : fs::recursive_directory_iterator(src_dir, ec)) {
    if (!e.is_regular_file() || !is_header_ext(e.path()))
      continue;
    auto rel = fs::relative(e.path(), src_dir);
    auto dest = dest_dir / rel;
    fs::create_directories(dest.parent_path(), ec);
    fs::copy_file(e.path(), dest, fs::copy_options::overwrite_existing, ec);
  }
}

// main entry: install a header-only dep
// returns the include flag to inject, e.g. "-I/path/to/zora-install/include"
inline bool install_header_only(const fs::path &repo,
                                const fs::path &install_dir,
                                const std::string &dep_name,
                                bool interactive = true) {
  auto predicted = predict_include_dirs(repo);

  // filter out build dirs
  predicted.erase(std::remove_if(predicted.begin(), predicted.end(),
                                 [](const fs::path &p) {
                                   auto n = p.filename().string();
                                   return n == "zora-install" ||
                                          n == "zora-cmake-build" ||
                                          n == "zora-meson-build" ||
                                          n == "build" || n == ".git";
                                 }),
                  predicted.end());

  std::vector<fs::path> selected;

  if (!interactive || predicted.size() == 1) {
    // auto-pick: prefer "include" over "single_include"
    for (auto &p : predicted)
      if (p.filename() == "include") {
        selected = {p};
        break;
      }
    if (selected.empty() && !predicted.empty())
      selected = {predicted[0]};
  } else {
    selected = browse_include_dirs(repo, predicted);
  }

  if (selected.empty()) {
    std::puts("  Cancelled.");
    return false;
  }

  // install: copy each selected dir under install/include/<dep_name>/
  fs::path inc_root = install_dir / "include" / dep_name;
  fs::create_directories(inc_root);

  for (auto& sel : selected) {
      if (!interactive) {
          // non-interactive: copy contents directly, no subdir wrapper
          // nlohmann_json/json.hpp, nlohmann_json/nlohmann/json.hpp etc.
          copy_headers(sel, inc_root);
          std::printf("  Installed %s -> include/%s/\n",
              fs::relative(sel, repo).c_str(), dep_name.c_str());
      } else {
          // interactive: preserve folder name so internal includes resolve
          fs::path dest = inc_root / sel.filename();
          copy_headers(sel, dest);
          std::printf("  Installed %s -> include/%s/%s/\n",
              fs::relative(sel, repo).c_str(), dep_name.c_str(),
              sel.filename().c_str());
      }
  }

  // stamp
  fs::path stamp = install_dir / ".built";
  std::FILE *f = std::fopen(stamp.string().c_str(), "w");
  if (f)
    std::fclose(f);

  std::printf("  Done. Use: #include \"%s/header.hpp\"\n", dep_name.c_str());
  return true;
}

} // namespace Zora
