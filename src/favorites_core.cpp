/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#include "favorites_core.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
  int MkdirP(const std::string &path)
  {
    if (path.empty())
      return 0;
    std::string current;
    for (size_t i = 0; i < path.size(); ++i)
    {
      current += path[i];
      if (path[i] == '/' || i + 1 == path.size())
      {
        if (current != "/" && current != "./")
        {
          struct stat st;
          if (stat(current.c_str(), &st) != 0)
          {
            if (mkdir(current.c_str(), 0755) != 0 && errno != EEXIST)
              return -1;
          }
        }
      }
    }
    return 0;
  }

  int CopyFile(const std::string &src, const std::string &dst)
  {
    std::ifstream in(src, std::ios::binary);
    if (!in)
      return -1;
    std::ofstream out(dst, std::ios::binary);
    if (!out)
      return -1;
    out << in.rdbuf();
    return out.good() ? 0 : -1;
  }

  int TouchFile(const std::string &path)
  {
    std::ofstream out(path, std::ios::binary);
    if (!out)
      return -1;
    out << "\n";
    out.flush();
    return out.good() ? 0 : -1;
  }

  // lstat: a symlink is removed itself, never the tree it points to
  int RmRf(const std::string &path)
  {
    struct stat st;
    if (lstat(path.c_str(), &st) != 0)
      return errno == ENOENT ? 0 : -1;
    if (!S_ISDIR(st.st_mode))
      return unlink(path.c_str());
    DIR *d = opendir(path.c_str());
    if (!d)
      return -1;
    int result = 0;
    while (const dirent *ent = readdir(d))
    {
      if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
        continue;
      if (RmRf(path + "/" + ent->d_name) != 0)
        result = -1;
    }
    closedir(d);
    return rmdir(path.c_str()) == 0 ? result : -1;
  }

  std::string Trim(const std::string &str)
  {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
      return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
  }

  bool ParseDesktopFile(const std::string &filepath, std::string &name, std::string &code, std::string &icon)
  {
    std::ifstream in(filepath);
    if (!in)
      return false;
    std::string line;
    name.clear();
    code.clear();
    icon.clear();

    while (std::getline(in, line))
    {
      line = Trim(line);
      if (line.compare(0, 5, "Name=") == 0)
      {
        name = line.substr(5);
      }
      else if (line.compare(0, 5, "Code=") == 0)
      {
        code = line.substr(5);
      }
      else if (line.compare(0, 5, "Icon=") == 0)
      {
        icon = line.substr(5);
      }
    }

    if (code.empty())
    {
      size_t slash = filepath.find_last_of('/');
      std::string filename = (slash != std::string::npos) ? filepath.substr(slash + 1) : filepath;
      if (filename.size() > 8 && filename.compare(filename.size() - 8, 8, ".desktop") == 0)
      {
        code = filename.substr(0, filename.size() - 8);
      }
    }
    if (name.empty())
    {
      name = code;
    }
    return !code.empty();
  }

  std::string ToLower(const std::string &s)
  {
    std::string res = s;
    std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c)
                   { return std::tolower(c); });
    return res;
  }

}

std::string GetFavoritesCode(const FavoritesState &state)
{
  if (!state.favCode.empty())
    return state.favCode;
  return "CLV-S-FAVORITES";
}

std::string GetFavoritesDirName(const FavoritesState &state)
{
  if (!state.favSuffix.empty())
    return state.favSuffix;
  return "favorites";
}

namespace
{
  bool DesktopHasShortcutMarker(const std::string &path)
  {
    std::ifstream in(path);
    if (!in)
      return false;
    std::string line;
    while (std::getline(in, line))
    {
      if (line.find("X-Favorites-Shortcut=1") != std::string::npos ||
          line.find("Managed by SNESC-Favorites") != std::string::npos)
      {
        return true;
      }
    }
    return false;
  }

  bool IsManagedShortcut(const std::string &dirPath, const std::string &code)
  {
    struct stat st;
    if (stat((dirPath + "/.sht").c_str(), &st) == 0)
      return true;
    if (stat((dirPath + "/.fav_shortcut").c_str(), &st) == 0)
      return true;
    return DesktopHasShortcutMarker(dirPath + "/" + code + ".desktop");
  }

  bool CreateDesktopShortcut(const GameInfo &game, const std::string &destDir)
  {
    std::string srcPath = game.desktopPath;
    std::string dstPath = destDir + "/" + game.code + ".desktop";

    std::ifstream in(srcPath);
    if (!in)
      return false;

    std::string originalDir;
    size_t lastSlash = srcPath.find_last_of('/');
    if (lastSlash != std::string::npos)
    {
      originalDir = srcPath.substr(0, lastSlash);
    }

    std::ostringstream out;
    std::string line;
    bool hasMarker = false;

    while (std::getline(in, line))
    {
      std::string trimmed = Trim(line);

      if (trimmed.compare(0, 5, "Icon=") == 0)
      {
        std::string iconVal = trimmed.substr(5);
        if (!iconVal.empty() && iconVal[0] != '/' && !originalDir.empty())
        {
          out << "Icon=" << originalDir << "/" << iconVal << "\n";
          continue;
        }
      }

      if (trimmed.find("X-Favorites-Shortcut") != std::string::npos)
      {
        hasMarker = true;
      }

      out << line << "\n";
    }

    if (!hasMarker)
    {
      out << "X-Favorites-Shortcut=1\n";
    }

    std::ofstream outFile(dstPath, std::ios::binary);
    if (!outFile)
      return false;
    outFile << out.str();
    return outFile.good();
  }

  std::string GetFavoritesListPath(const FavoritesState &state)
  {
    if (state.gamesRoot.empty())
      return "";
    return state.gamesRoot + "/.favorites";
  }

  std::set<std::string> LoadPersistentFavorites(const FavoritesState &state)
  {
    std::set<std::string> codes;
    std::string primaryPath = GetFavoritesListPath(state);
    std::ifstream in(primaryPath);
    if (!in)
    {
      std::string fallbackPath = state.gamesRoot + "/" + GetFavoritesDirName(state) + "/.favorites_list";
      in.open(fallbackPath);
    }
    if (in)
    {
      std::string line;
      while (std::getline(in, line))
      {
        std::string code = Trim(line);
        if (!code.empty() && code[0] != '#')
        {
          codes.insert(code);
        }
      }
    }
    return codes;
  }

  bool SavePersistentFavorites(const FavoritesState &state, const std::set<std::string> &codes)
  {
    std::string primaryPath = GetFavoritesListPath(state);
    if (primaryPath.empty())
      return false;
    std::ofstream out(primaryPath);
    if (!out)
      return false;
    out << "# SNESC Favorites persistent list - do not edit manually\n";
    for (const auto &code : codes)
    {
      out << code << "\n";
    }
    out.flush();
    return out.good();
  }

}

std::string FindGamesRoot(const std::string &overridePath)
{
  if (!overridePath.empty())
  {
    struct stat st;
    if (stat(overridePath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
    {
      if (stat((overridePath + "/000").c_str(), &st) == 0 && S_ISDIR(st.st_mode))
        return overridePath;

      const char *subDirs[] = {
          "snes-usa", "snes-eur", "snes-jpn",
          "nes-usa", "nes-eur", "nes-jpn",
          "hvcj-jpn", nullptr};
      for (int i = 0; subDirs[i]; ++i)
      {
        std::string sub = overridePath + "/" + subDirs[i];
        if (stat(sub.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
        {
          std::string sub000 = sub + "/000";
          if (stat(sub000.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            return sub;
          return sub;
        }
      }
      return overridePath;
    }
  }

  const char *candidates[] = {
      "/media/hakchi/games/snes-usa",
      "/media/hakchi/games/snes-eur",
      "/media/hakchi/games/snes-jpn",
      "/media/hakchi/games/nes-usa",
      "/media/hakchi/games/nes-eur",
      "/media/hakchi/games/nes-jpn",
      "/media/hakchi/games/hvcj-jpn",
      "/media/hakchi/games",
      "/var/games",
      "/media/data/games",
      "/usr/share/games",
      "./games",
      nullptr};

  for (int i = 0; candidates[i]; ++i)
  {
    struct stat st;
    if (stat(candidates[i], &st) == 0 && S_ISDIR(st.st_mode))
    {
      std::string base = candidates[i];
      if (base == "/media/hakchi/games")
      {
        const char *subDirs[] = {
            "snes-usa", "snes-eur", "snes-jpn",
            "nes-usa", "nes-eur", "nes-jpn",
            "hvcj-jpn", nullptr};
        for (int j = 0; subDirs[j]; ++j)
        {
          std::string sub = base + "/" + subDirs[j];
          if (stat(sub.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            return sub;
        }
      }
      return base;
    }
  }
  return "";
}

bool ScanFavoritesState(FavoritesState &state, const std::string &overrideRoot)
{
  state.games.clear();
  state.favSuffix = "favorites";
  state.favCode = "CLV-S-FAVORITES";
  state.favoriteCount = 0;

  state.gamesRoot = FindGamesRoot(overrideRoot);
  if (state.gamesRoot.empty())
  {
    return false;
  }

  std::string favDirName = GetFavoritesDirName(state);
  std::string favDir = state.gamesRoot + "/" + favDirName;

  std::set<std::string> existingFavCodes = LoadPersistentFavorites(state);

  if (existingFavCodes.empty())
  {
    DIR *favD = opendir(favDir.c_str());
    if (favD)
    {
      while (const dirent *ent = readdir(favD))
      {
        if (ent->d_name[0] == '.')
          continue;
        if (strncmp(ent->d_name, "CLV-S-", 6) == 0)
          continue;
        std::string sub = favDir + "/" + ent->d_name;
        if (IsManagedShortcut(sub, ent->d_name))
        {
          existingFavCodes.insert(ent->d_name);
        }
      }
      closedir(favD);
    }
  }

  std::set<std::string> seenCodes;

  auto ScanDir = [&](const std::string &path, const std::string &folderName, auto &self) -> void
  {
    DIR *cd = opendir(path.c_str());
    if (!cd)
      return;
    while (const dirent *ent = readdir(cd))
    {
      if (ent->d_name[0] == '.')
        continue;
      std::string entryName = ent->d_name;
      std::string fullPath = path + "/" + entryName;

      struct stat st;
      if (stat(fullPath.c_str(), &st) != 0)
        continue;

      if (S_ISDIR(st.st_mode))
      {
        if (entryName == favDirName || entryName == "favorites")
          continue;
        if (entryName.compare(0, 6, "CLV-S-") == 0)
          continue;

        std::string nextFolder = folderName.empty() ? entryName : folderName;
        self(fullPath, nextFolder, self);
      }
      else if (S_ISREG(st.st_mode))
      {
        if (entryName.size() > 8 && entryName.compare(entryName.size() - 8, 8, ".desktop") == 0)
        {
          if (entryName.compare(0, 6, "CLV-S-") == 0)
            continue;

          GameInfo game;
          if (ParseDesktopFile(fullPath, game.name, game.code, game.iconPath))
          {
            if (seenCodes.find(game.code) != seenCodes.end())
              continue;
            seenCodes.insert(game.code);

            game.desktopPath = fullPath;
            game.folderNum = folderName;
            if (existingFavCodes.find(game.code) != existingFavCodes.end())
            {
              game.isFavorite = true;
              state.favoriteCount++;
            }
            state.games.push_back(game);
          }
        }
      }
    }
    closedir(cd);
  };

  ScanDir(state.gamesRoot, "", ScanDir);

  std::sort(state.games.begin(), state.games.end(), [](const GameInfo &a, const GameInfo &b)
            { return ToLower(a.name) < ToLower(b.name); });

  return !state.games.empty();
}

bool ApplyFavorites(FavoritesState &state, const std::string &assetsDir, std::string &errorMessage)
{
  if (state.gamesRoot.empty())
  {
    errorMessage = "Invalid games root.";
    return false;
  }
  if (state.games.empty())
  {
    errorMessage = "Cannot apply favorites: games library is empty or scan failed.";
    return false;
  }

  std::string homeDir = state.gamesRoot;
  struct stat st;
  if (stat((state.gamesRoot + "/000").c_str(), &st) == 0 && S_ISDIR(st.st_mode))
  {
    homeDir = state.gamesRoot + "/000";
  }

  std::string favDirName = GetFavoritesDirName(state);
  std::string favCode = GetFavoritesCode(state);
  std::string favDir = state.gamesRoot + "/" + favDirName;
  std::string homeClvDir = homeDir + "/" + favCode;

  if (MkdirP(homeClvDir) != 0)
  {
    errorMessage = "Cannot create directory: " + homeClvDir;
    return false;
  }
  if (MkdirP(favDir) != 0)
  {
    errorMessage = "Cannot create directory: " + favDir;
    return false;
  }

  TouchFile(favDir + "/.fav");

  std::string favDesktop = homeClvDir + "/" + favCode + ".desktop";
  {
    std::ofstream out(favDesktop);
    if (!out)
    {
      errorMessage = "Failed to write: " + favDesktop;
      return false;
    }
    out << "[Desktop Entry]\n"
        << "Type=Application\n"
        << "Exec=/bin/chmenu " << favDirName << " /var/games\n"
        << "Path=/var/saves/FOLDER\n"
        << "Name=Favorites\n"
        << "Icon=/var/games/" << favCode << "/" << favCode << ".png\n\n"
        << "[X-CLOVER Game]\n"
        << "Code=" << favCode << "\n"
        << "TestID=777\n"
        << "ID=0\n"
        << "Players=1\n"
        << "Simultaneous=0\n"
        << "ReleaseDate=7777-77-77\n"
        << "SaveCount=0\n"
        << "SortRawTitle=\xd0\xad"
           "favorites\n"
        << "SortRawPublisher=ZZZZZZZZZX\n"
        << "Copyright=hakchi2 \xc2\xa9"
           "2017 Alexey 'Cluster' Avdyukhin\n";
  }

  if (!assetsDir.empty())
  {
    CopyFile(assetsDir + "/favorites.png", homeClvDir + "/" + favCode + ".png");
    CopyFile(assetsDir + "/favorites_small.png", homeClvDir + "/" + favCode + "_small.png");
  }

  std::string favHome = favDir + "/CLV-S-00000";
  if (MkdirP(favHome) != 0)
  {
    errorMessage = "Cannot create directory: " + favHome;
    return false;
  }
  {
    std::ofstream out(favHome + "/CLV-S-00000.desktop");
    if (!out)
    {
      errorMessage = "Failed to write: " + favHome + "/CLV-S-00000.desktop";
      return false;
    }
    out << "[Desktop Entry]\n"
        << "Type=Application\n"
        << "Exec=/bin/chmenu 000 /var/games\n"
        << "Path=/var/saves/FOLDER\n"
        << "Name=Back\n"
        << "Icon=/var/games/CLV-S-00000/CLV-S-00000.png\n\n"
        << "[X-CLOVER Game]\n"
        << "Code=CLV-S-00000\n"
        << "TestID=777\n"
        << "ID=0\n"
        << "Players=1\n"
        << "Simultaneous=0\n"
        << "ReleaseDate=9999-99-99\n"
        << "SaveCount=0\n"
        << "SortRawTitle=\xd0\xaf"
           "home\n"
        << "SortRawPublisher=ZZZZZZZZZZ\n"
        << "Copyright=hakchi2 \xc2\xa9"
           "2017 Alexey 'Cluster' Avdyukhin\n";
    if (!assetsDir.empty())
    {
      CopyFile(assetsDir + "/folder_home.png", favHome + "/CLV-S-00000.png");
      CopyFile(assetsDir + "/folder_home_small.png", favHome + "/CLV-S-00000_small.png");
    }
  }

  state.favoriteCount = 0;
  std::set<std::string> wantedCodes;
  int failedShortcuts = 0;
  for (const auto &game : state.games)
  {
    if (game.isFavorite)
    {
      wantedCodes.insert(game.code);
      state.favoriteCount++;
      std::string stubDir = favDir + "/" + game.code;
      if (MkdirP(stubDir) != 0 || !CreateDesktopShortcut(game, stubDir) || TouchFile(stubDir + "/.sht") != 0)
        ++failedShortcuts;
    }
  }

  DIR *d = opendir(favDir.c_str());
  if (d)
  {
    while (const dirent *ent = readdir(d))
    {
      if (ent->d_name[0] == '.')
        continue;
      if (strcmp(ent->d_name, "CLV-S-00000") == 0)
        continue;
      std::string sub = favDir + "/" + ent->d_name;
      struct stat subSt;
      if (stat(sub.c_str(), &subSt) == 0 && S_ISDIR(subSt.st_mode))
      {
        if (IsManagedShortcut(sub, ent->d_name))
        {
          if (wantedCodes.find(ent->d_name) == wantedCodes.end())
          {
            RmRf(sub);
          }
        }
      }
    }
    closedir(d);
  }

  if (!SavePersistentFavorites(state, wantedCodes))
  {
    errorMessage = "Failed to write: " + GetFavoritesListPath(state);
    return false;
  }
  if (failedShortcuts > 0)
  {
    errorMessage = "Failed to create " + std::to_string(failedShortcuts) + " shortcut(s) in " + favDir;
    return false;
  }
  return true;
}

bool RemoveFavorites(FavoritesState &state, std::string &errorMessage)
{
  if (state.gamesRoot.empty())
  {
    errorMessage = "Invalid games root.";
    return false;
  }

  std::string homeDir = state.gamesRoot;
  struct stat st;
  if (stat((state.gamesRoot + "/000").c_str(), &st) == 0 && S_ISDIR(st.st_mode))
    homeDir = state.gamesRoot + "/000";

  std::string favCode = GetFavoritesCode(state);
  std::string favDirName = GetFavoritesDirName(state);
  std::string homeClvDir = homeDir + "/" + favCode;
  std::string favDir = state.gamesRoot + "/" + favDirName;

  if (RmRf(homeClvDir) != 0)
  {
    errorMessage = "Cannot remove: " + homeClvDir;
    return false;
  }
  if (RmRf(favDir) != 0)
  {
    errorMessage = "Cannot remove: " + favDir;
    return false;
  }

  std::string persistentList = GetFavoritesListPath(state);
  if (!persistentList.empty() && unlink(persistentList.c_str()) != 0 && errno != ENOENT)
  {
    errorMessage = "Cannot remove: " + persistentList;
    return false;
  }

  for (auto &game : state.games)
  {
    game.isFavorite = false;
  }
  state.favoriteCount = 0;
  return true;
}
