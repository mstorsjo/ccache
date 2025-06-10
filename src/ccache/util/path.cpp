// Copyright (C) 2021-2025 Joel Rosdahl and other contributors
//
// See doc/AUTHORS.adoc for a complete list of contributors.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc., 51
// Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

#include "path.hpp"

#include <ccache/util/direntry.hpp>
#include <ccache/util/filesystem.hpp>
#include <ccache/util/format.hpp>
#include <ccache/util/string.hpp>

#include <stdio.h>
#include <iostream>

#ifdef _WIN32
const char k_dev_null_path[] = "nul:";
#else
const char k_dev_null_path[] = "/dev/null";
#endif

namespace fs = util::filesystem;

namespace util {

std::string
add_exe_suffix(const std::string& program)
{
  return fs::path(program).has_extension() ? program : program + ".exe";
}

fs::path
apparent_cwd(const fs::path& actual_cwd)
{
#ifdef _WIN32
  return actual_cwd;
#else
  auto pwd = getenv("PWD");
  if (!pwd || !fs::path(pwd).is_absolute()) {
    return actual_cwd;
  }

  DirEntry pwd_de(pwd);
  DirEntry cwd_de(actual_cwd);
  return !pwd_de || !cwd_de || !pwd_de.same_inode_as(cwd_de) ? actual_cwd : pwd;
#endif
}

const char*
get_dev_null_path()
{
  return k_dev_null_path;
}

fs::path
lexically_normal(const fs::path& path)
{
  auto result = path.lexically_normal();
  return result.has_filename() ? result : result.parent_path();
}

fs::path
make_relative_path(const fs::path& actual_cwd,
                   const fs::path& apparent_cwd,
                   const fs::path& path)
{
  DEBUG_ASSERT(actual_cwd.is_absolute());
  DEBUG_ASSERT(apparent_cwd.is_absolute());
  DEBUG_ASSERT(path.is_absolute());

fprintf(stderr, "make_relative_path \"%s\" \"%s\" \"%s\"\n", actual_cwd.string().c_str(), apparent_cwd.string().c_str(), path.string().c_str());
  fs::path normalized_path = util::lexically_normal(path);
fprintf(stderr, "normalized_path %s\n", normalized_path.string().c_str());
  fs::path closest_existing_path = normalized_path;
  std::vector<fs::path> relpath_candidates;
  fs::path path_suffix;
  while (!fs::exists(closest_existing_path)) {
fprintf(stderr, "closest_existing_path %s\n", closest_existing_path.string().c_str());
    if (path_suffix.empty()) {
      path_suffix = closest_existing_path.filename();
fprintf(stderr, "path_suffix empty -> %s\n", path_suffix.string().c_str());
    } else {
      path_suffix = closest_existing_path.filename() / path_suffix;
fprintf(stderr, "path_suffix = %s\n", path_suffix.string().c_str());
    }
    closest_existing_path = closest_existing_path.parent_path();
fprintf(stderr, "closest_existing_path now %s - exists %d\n", closest_existing_path.string().c_str(), fs::exists(closest_existing_path) ? 1 : 0);
  }
fprintf(stderr, "closest_existing_path finally %s\n", closest_existing_path.string().c_str());

  const auto add_relpath_candidates = [&](auto p) {
fs::path rel = p.lexically_relative(actual_cwd);
fprintf(stderr, "%s lexically_relative(%s) = %s\n", p.string().c_str(), actual_cwd.string().c_str(), rel.string().c_str());
    relpath_candidates.push_back(rel);
    if (apparent_cwd != actual_cwd) {
      fs::path rel2 = p.lexically_relative(apparent_cwd);
fprintf(stderr, "rel2 %s\n", rel2.string().c_str());
      relpath_candidates.emplace_back(rel2);
    }
  };

  add_relpath_candidates(closest_existing_path);
  const fs::path real_closest_existing_path =
    fs::canonical(closest_existing_path).value_or(closest_existing_path);
  if (real_closest_existing_path != closest_existing_path) {
    add_relpath_candidates(real_closest_existing_path);
  }
for (const auto &p : relpath_candidates)
fprintf(stderr, "relpath_candidates: %s\n", p.string().c_str());

  // Find best (i.e. shortest existing) match:
  std::sort(relpath_candidates.begin(),
            relpath_candidates.end(),
            [](const auto& path1, const auto& path2) {
              return util::pstr(path1).str().length()
                     < util::pstr(path2).str().length();
            });
  for (const auto& relpath : relpath_candidates) {
fprintf(stderr, "inspecting %s vs %s\n", relpath.string().c_str(), closest_existing_path.string().c_str());
    if (fs::equivalent(relpath, closest_existing_path)) {
fprintf(stderr, "returning, path_suffix %s\n", path_suffix.string().c_str());
      if (path_suffix.empty()) {
fprintf(stderr, "returning relpath %s\n", relpath.string().c_str());
return relpath;
} else {
fprintf(stderr, "returning relpath %s\n", relpath.string().c_str());
fs::path ret = (relpath / path_suffix).lexically_normal();
fprintf(stderr, "ret %s\n", ret.string().c_str());
return ret;
}
    }
  }

  // No match so nothing else to do than to return the unmodified path.
fprintf(stderr, "returning path %s\n", path.string().c_str());
  return path;
}

bool
path_starts_with(const fs::path& path, const fs::path& prefix)
{
#ifdef _WIN32
  // Note: Not all paths on Windows are case insensitive, but for our purposes
  // (checking whether a path is below the base directory) users will expect
  // them to be.
  fs::path p1 = util::to_lowercase(util::lexically_normal(path).string());
  fs::path p2 = util::to_lowercase(util::lexically_normal(prefix).string());
#else
  const fs::path& p1 = path;
  const fs::path& p2 = prefix;
#endif

  // Skip empty part at the end that originates from a trailing slash.
  auto p2_end = p2.end();
  if (!p2.empty()) {
    --p2_end;
    if (!p2_end->empty()) {
      ++p2_end;
    }
  }

  return std::mismatch(p1.begin(), p1.end(), p2.begin(), p2_end).second
         == p2_end;
}

} // namespace util
