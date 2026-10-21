/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#ifndef FAVORITES_CORE_H
#define FAVORITES_CORE_H

#include <string>
#include <vector>

struct GameInfo
{
  std::string code;
  std::string name;
  std::string desktopPath;
  std::string iconPath;
  std::string folderNum;
  bool isFavorite = false;
};

struct FavoritesState
{
  std::string gamesRoot;
  std::string favSuffix = "favorites";
  std::string favCode = "CLV-S-FAVORITES";
  std::vector<GameInfo> games;
  int favoriteCount = 0;
};

std::string GetFavoritesCode(const FavoritesState &state);

std::string GetFavoritesDirName(const FavoritesState &state);

std::string FindGamesRoot(const std::string &overridePath = "");

bool ScanFavoritesState(FavoritesState &state, const std::string &overrideRoot = "");

bool ApplyFavorites(FavoritesState &state, const std::string &assetsDir, std::string &errorMessage);

bool RemoveFavorites(FavoritesState &state, std::string &errorMessage);

#endif
