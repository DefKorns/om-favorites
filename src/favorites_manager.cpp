/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#include "framework/badge.h"
#include "framework/sdl_helper.h"
#include "framework/controller.h"
#include "framework/powerwatch.h"
#include "framework/draw_helpers.h"
#include "framework/list_jump.h"
#include "framework/list_screen.h"
#include "framework/panel_dialog.h"
#include "framework/text_layout.h"
#include "framework/utf8.h"
#include "framework/uitheme.h"
#include "framework/texture.h"
#include "framework/sdl_context.h"
#include "localization.h"
#include "favorites_core.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <vector>

#ifndef MOD_VERSION
#define MOD_VERSION "dev"
#endif

namespace
{
  const std::string DefaultOptionsLocation = "/etc/options_menu/";
  const int ExitBack = 0;
  const int ExitLeaveMenu = 1;
  // the home menu caches its folders: the launcher reloads it via chmenu
  const int ExitReloadMenu = 2;

  const Color White{255, 255, 255};
  const Color Warning{0xE8, 0x9C, 0x30};
  const int MaxSafeFavorites = 30;

  const int ListW = 600;
  const int ListRightX = UiTheme::ListX + ListW - 40;
  const int ControlRightX = ListRightX - 16;
  const int DetailX = UiTheme::ListX + ListW + 16;
  const int DetailW = UiTheme::FrameX + UiTheme::FrameW - 16 - DetailX;

  const int ListFontSize = 16;
  const int PanelFontSize = 16;
  const int PanelHeaderFontSize = 18;
  const int NoteFontSize = 15;

  void DrawDot(SDL_Renderer *renderer, int x, int centerY)
  {
    DrawRoundedFillRect(renderer, {x, centerY - 4, 8, 8}, UiTheme::Accent, 4);
  }
}

int main(int argc, char **argv)
{
  std::string optionsLocation = DefaultOptionsLocation;
  std::string customGamesPath = "";
  bool cliMode = false;

  for (int i = 1; i < argc; ++i)
  {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
    {
      std::cout << "Favorites Manager " << MOD_VERSION << "\n"
                << "Usage: " << argv[0] << " [options]\n"
                << "  --optionsPath <path>   Set OptionsMenu directory\n"
                << "  --gamesPath <path>     Set games directory\n"
                << "  --list                 List games in console (CLI mode)\n"
                << "  --help                 Show this help\n";
      return 0;
    }
    else if (strcmp(argv[i], "--optionsPath") == 0 && i + 1 < argc)
    {
      optionsLocation = argv[++i];
    }
    else if (strcmp(argv[i], "--gamesPath") == 0 && i + 1 < argc)
    {
      customGamesPath = argv[++i];
    }
    else if (strcmp(argv[i], "--list") == 0)
    {
      cliMode = true;
    }
    else if (argv[i][0] == '/')
    {
      optionsLocation = argv[i];
    }
  }

  if (!optionsLocation.empty() && optionsLocation.back() != '/')
    optionsLocation += '/';

  if (cliMode)
  {
    FavoritesState state;
    ScanFavoritesState(state, customGamesPath);
    std::cout << "Games Root: " << state.gamesRoot << "\n";
    std::cout << "Favorites Folder: " << GetFavoritesDirName(state) << " (" << GetFavoritesCode(state) << ")\n";
    std::cout << "Games count: " << state.games.size() << "\n\n";
    for (size_t i = 0; i < state.games.size(); ++i)
    {
      std::cout << (state.games[i].isFavorite ? "[*] " : "[ ] ")
                << state.games[i].code << " - " << state.games[i].name << "\n";
    }
    return 0;
  }

  LoadLanguageFromConfig(optionsLocation);
  SetTTFFontPath(optionsLocation);
  UiTheme::LoadThemeConfig(optionsLocation);

  SDL_Context sdl_context(std::chrono::milliseconds(33), false);
  auto renderer = sdl_context.renderer;
  Controller controller(1);

  DropPendingInput(controller);

  const Color bg = UiTheme::Bg;
  SetDrawColor(renderer, bg);

  ScreenChrome chrome(optionsLocation, renderer, "Favorites Manager", MOD_VERSION, "Favorites Manager - by DefKorns");
  chrome.SetSectionTitle(Translate("FAVORITES_TITLE"));

  const int RowBoxH = UiTheme::RowPitch - 6;
  Texture checkboxOn(optionsLocation + UiTheme::AssetCheckboxOn, renderer);
  SetColorMod(checkboxOn.texture.get(), UiTheme::Accent);
  Texture checkboxOff(optionsLocation + UiTheme::AssetCheckboxOff, renderer);
  SetColorMod(checkboxOff.texture.get(), UiTheme::CheckboxOff);

  const int CheckboxSize = 20;
  const int CheckboxX = ControlRightX - CheckboxSize;
  const int RowTextMaxW = CheckboxX - 16 - UiTheme::RowTextX;
  const int displayCount = std::max(1, (UiTheme::FooterDividerY - UiTheme::ListBottomMargin - UiTheme::RowFirstY) / UiTheme::RowPitch);

  ScrollArrows scrollArrows(optionsLocation, renderer);

  const int PanelPad = 16;
  const int PanelInnerX = DetailX + PanelPad;
  const int PanelInnerW = DetailW - 2 * PanelPad;
  const int BoxGap = 16;
  const int StatPitch = 26;
  const int StatsBoxH = 4 * StatPitch + 2 * PanelPad - 6;
  const int DetailBottomY = UiTheme::FooterDividerY - UiTheme::ListBottomMargin;
  const SDL_Rect statsBox{DetailX, DetailBottomY - StatsBoxH, DetailW, StatsBoxH};

  const int NoteLinePitch = 20;
  const int NoteTextOffset = 46;
  const std::vector<std::string> noteText = WrapToWidth(Translate("FAVORITES_NOTE"), NoteFontSize, DetailW - NoteTextOffset - PanelPad);
  const int NoteBoxH = static_cast<int>(noteText.size()) * NoteLinePitch + 2 * 12;
  const SDL_Rect noteBox{DetailX, statsBox.y - BoxGap - NoteBoxH, DetailW, NoteBoxH};
  NoteBox note(renderer, noteBox);
  note.SetText(Translate("FAVORITES_NOTE"));

  const SDL_Rect summaryBox{DetailX, UiTheme::RowFirstY, DetailW, noteBox.y - BoxGap - UiTheme::RowFirstY};
  const int SummaryDividerY = summaryBox.y + 46;
  const int SummaryListY = summaryBox.y + 62;
  const int SummaryRowPitch = 28;
  const int SummaryMaxRows = (summaryBox.y + summaryBox.h - PanelPad - SummaryListY) / SummaryRowPitch;
  const int BulletTextGap = 18;

  BadgePainter badges(optionsLocation, renderer);
  auto MakeBadge = [&](const std::string &letter, const std::string &labelKey, Color rim, Color fill) -> Badge
  {
    return badges.Make(letter, Translate(labelKey), rim, fill);
  };

  Badge badgeMark = MakeBadge("A", "HINT_MARK", UiTheme::BadgeADark, UiTheme::BadgeA);
  Badge badgeUnmark = MakeBadge("A", "HINT_UNMARK", UiTheme::BadgeADark, UiTheme::BadgeA);
  Badge badgeApply = badges.MakePill("Start", Translate("HINT_APPLY"));
  Badge badgeInvert = MakeBadge("Y", "HINT_INVERT", UiTheme::BadgeYDark, UiTheme::BadgeY);
  Badge badgeClear = badges.MakePill("Select", Translate("HINT_CLEAR"));
  Badge badgeBack = MakeBadge("B", "HINT_BACK", UiTheme::BadgeBDark, UiTheme::BadgeB);
  Badge badgeCancel = MakeBadge("B", "HINT_CANCEL", UiTheme::BadgeBDark, UiTheme::BadgeB);
  Badge badgeConfirm = MakeBadge("A", "HINT_CONFIRM", UiTheme::BadgeADark, UiTheme::BadgeA);
  Badge badgeOk = MakeBadge("A", "HINT_OK", UiTheme::BadgeADark, UiTheme::BadgeA);
  Badge confirmPill = badges.MakePill("Start + Select", Translate("HINT_CONFIRM"));

  auto DrawBadge = [&](Badge &badge, int rightEdgeX) -> int
  {
    return badges.Draw(badge, rightEdgeX, UiTheme::BadgeBandY);
  };

  auto RunDialog = [&](const std::function<void()> &drawUnder, PanelDialog::Kind kind, const std::string &titleKey,
                       const std::vector<std::string> &bodyLines, const std::string &noteKey) -> DialogResult
  {
    PanelDialogText text;
    text.title = Translate(titleKey);
    text.body = bodyLines;
    if (!noteKey.empty())
      text.note = Translate(noteKey);
    std::vector<Badge *> shown;
    if (kind == PanelDialog::Kind::Acknowledge)
      shown = {&badgeOk};
    else if (kind == PanelDialog::Kind::Confirm)
      shown = {&badgeCancel, &badgeConfirm};
    else
      shown = {&badgeCancel, &confirmPill};
    return PanelDialog(renderer, badges, kind, text, shown).Run(sdl_context, controller, drawUnder);
  };

  ShowBusy(sdl_context, [&]()
           { chrome.Draw(); }, Translate("FAVORITES_WORKING"));

  FavoritesState state;
  bool scanOk = ScanFavoritesState(state, customGamesPath);

  if (!scanOk || state.games.empty())
  {
    auto DrawEmptyScreen = [&]()
    {
      chrome.Draw();
    };
    const DialogResult answer = RunDialog(DrawEmptyScreen, PanelDialog::Kind::Acknowledge, "FAVORITES_NO_GAMES_FOUND", {Translate("FAVORITES_CHECK_USB_NOTE")}, "");
    return answer == DialogResult::Quit ? ExitLeaveMenu : ExitBack;
  }

  int currentId = 0;
  int listOffset = 0;
  const int selectAllRows = 1;

  std::vector<Texture> gameTextures;
  gameTextures.reserve(state.games.size());
  for (const auto &game : state.games)
  {
    std::string label = TruncateToWidth(game.name, ListFontSize, RowTextMaxW);
    gameTextures.emplace_back(label, ListFontSize, renderer, UiTheme::RowTextX, 0, false, ToAbgr(White), true);
  }

  Texture selectAllText(Translate("FAVORITES_SELECT_ALL"), ListFontSize, renderer, UiTheme::RowTextX, 0, false, ToAbgr(White), true);
  Texture summaryHeader(Translate("FAVORITES_HEADER"), PanelHeaderFontSize, renderer, PanelInnerX, summaryBox.y + 14, false, ToAbgr(UiTheme::Text), true);
  Texture noneMarkedText(Translate("FAVORITES_NONE_MARKED"), PanelFontSize, renderer, PanelInnerX, SummaryListY, false, ToAbgr(UiTheme::Text), true);
  std::vector<Texture> noneHintLines = MakeLines(WrapToWidth(Translate("FAVORITES_NONE_HINT"), PanelFontSize, PanelInnerW), PanelFontSize, renderer, UiTheme::TextDim);

  int markedCount = 0;
  std::string resultKey;
  Texture summaryCount;
  Texture moreText;
  std::vector<Texture> summaryNames;

  std::vector<Texture> statLabels;
  std::vector<Texture> statValues;

  auto RefreshStats = [&]()
  {
    statLabels.clear();
    statValues.clear();

    const std::vector<std::pair<std::string, std::string>> rows{
        {"FAVORITES_STAT_GAMES", std::to_string(state.games.size())},
        {"FAVORITES_STAT_MARKED", std::to_string(markedCount)},
        {"FAVORITES_STAT_FOLDER", GetFavoritesCode(state)},
        {"FAVORITES_STAT_STATUS", !resultKey.empty()             ? Translate(resultKey)
                                  : markedCount > MaxSafeFavorites ? Translate("FAVORITES_WARNING")
                                  : markedCount > 0              ? Translate("FAVORITES_READY")
                                                                 : Translate("FAVORITES_EMPTY")}};

    for (const auto &row : rows)
    {
      statValues.emplace_back(row.second, 15, renderer, 0, 0, false, ToAbgr(White), true);
      const int labelMaxW = PanelInnerW - statValues.back().rect.w - 12;
      statLabels.emplace_back(TruncateToWidth(Translate(row.first), 15, labelMaxW), 15, renderer, PanelInnerX, 0, false, ToAbgr(UiTheme::TextDim), true);
    }
  };

  auto RefreshSummary = [&]()
  {
    markedCount = static_cast<int>(std::count_if(state.games.begin(), state.games.end(), [](const GameInfo &g)
                                                 { return g.isFavorite; }));

    std::string countStr = std::to_string(markedCount) + " / " + std::to_string(state.games.size());
    Color countColor = (markedCount > MaxSafeFavorites) ? Warning : UiTheme::Accent;
    summaryCount = Texture(countStr, 14, renderer, summaryBox.x + summaryBox.w - PanelPad, summaryBox.y + 16, false, ToAbgr(countColor), true);
    summaryCount.rect.x -= summaryCount.rect.w;

    summaryNames.clear();
    int count = 0;
    for (const auto &game : state.games)
    {
      if (!game.isFavorite)
        continue;
      if (count < SummaryMaxRows - 1 || (count == SummaryMaxRows - 1 && markedCount == SummaryMaxRows))
      {
        std::string itemText = TruncateToWidth(game.name, PanelFontSize, PanelInnerW - BulletTextGap);
        summaryNames.emplace_back(itemText, PanelFontSize, renderer, PanelInnerX + BulletTextGap, 0, false, ToAbgr(UiTheme::Text), true);
      }
      count++;
    }

    if (markedCount > SummaryMaxRows)
    {
      std::string moreStr = "+ " + std::to_string(markedCount - (SummaryMaxRows - 1)) + " " + Translate("FAVORITES_MORE");
      moreText = Texture(moreStr, PanelFontSize, renderer, PanelInnerX + BulletTextGap, 0, false, ToAbgr(UiTheme::TextDim), true);
    }
    else
    {
      moreText = Texture();
    }

    RefreshStats();
  };

  RefreshSummary();

  auto FolderExists = [&]()
  {
    struct stat st;
    const std::string favDir = state.gamesRoot + "/" + GetFavoritesDirName(state);
    return stat(favDir.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
  };
  bool folderExists = FolderExists();
  bool menuChanged = false;

  auto DrawScreen = [&]()
  {
    chrome.Draw();

    const int totalRows = static_cast<int>(state.games.size()) + selectAllRows;
    for (int i = 0; i < displayCount && listOffset + i < totalRows; ++i)
    {
      const int row = listOffset + i;
      const bool selected = (row == currentId);
      SDL_Rect rowRect = DrawListRow(renderer, i, ListRightX, RowBoxH, selected);
      const int textY = rowRect.y + (RowBoxH - ListFontSize) / 2 + UiTheme::RowTextYNudge;

      if (row == 0)
      {
        selectAllText.rect.y = textY;
        selectAllText.Draw(renderer);
        Texture &cb = (markedCount == static_cast<int>(state.games.size())) ? checkboxOn : checkboxOff;
        cb.rect.x = CheckboxX;
        cb.rect.y = rowRect.y + (RowBoxH - CheckboxSize) / 2;
        cb.Draw(renderer);
      }
      else
      {
        const int gameIndex = row - selectAllRows;
        gameTextures[gameIndex].rect.y = textY;
        gameTextures[gameIndex].Draw(renderer);

        Texture &cb = state.games[gameIndex].isFavorite ? checkboxOn : checkboxOff;
        cb.rect.x = CheckboxX;
        cb.rect.y = rowRect.y + (RowBoxH - CheckboxSize) / 2;
        cb.Draw(renderer);
      }
    }

    scrollArrows.Draw(listOffset > 0, listOffset + displayCount < totalRows);

    DrawStrokeRect(renderer, summaryBox, UiTheme::Border, UiTheme::BorderWidth, UiTheme::BoxRadius);
    summaryHeader.Draw(renderer);
    summaryCount.Draw(renderer);
    DrawHLine(renderer, PanelInnerX, PanelInnerX + PanelInnerW, SummaryDividerY, UiTheme::Border, 1);

    if (markedCount == 0)
    {
      noneMarkedText.Draw(renderer);
      int y = SummaryListY + 30;
      for (Texture &line : noneHintLines)
      {
        line.rect.x = PanelInnerX;
        line.rect.y = y;
        line.Draw(renderer);
        y += 22;
      }
    }
    else
    {
      int y = SummaryListY;
      for (size_t i = 0; i < summaryNames.size(); ++i)
      {
        DrawDot(renderer, PanelInnerX + 2, y + SummaryRowPitch / 2 - 4);
        summaryNames[i].rect.y = y + 2;
        summaryNames[i].Draw(renderer);
        y += SummaryRowPitch;
      }
      if (moreText.texture.get())
      {
        moreText.rect.y = y + 2;
        moreText.Draw(renderer);
      }
    }

    note.Draw();

    DrawStrokeRect(renderer, statsBox, UiTheme::Border, UiTheme::BorderWidth, UiTheme::BoxRadius);
    int statY = statsBox.y + PanelPad - 2;
    for (size_t i = 0; i < statLabels.size(); ++i)
    {
      statLabels[i].rect.y = statY;
      statLabels[i].Draw(renderer);
      statValues[i].rect.x = statsBox.x + statsBox.w - PanelPad - statValues[i].rect.w;
      statValues[i].rect.y = statY;
      statValues[i].Draw(renderer);
      statY += StatPitch;
    }

    int rightEdge = UiTheme::BadgeClusterRightX;
    const bool currentRowMarked = (currentId == 0) ? (markedCount == static_cast<int>(state.games.size()))
                                                   : state.games[currentId - selectAllRows].isFavorite;
    rightEdge = DrawBadge(currentRowMarked ? badgeUnmark : badgeMark, rightEdge);
    rightEdge = DrawBadge(badgeInvert, rightEdge);
    if (markedCount > 0 || folderExists)
      rightEdge = DrawBadge(badgeApply, rightEdge);
    if (folderExists)
      rightEdge = DrawBadge(badgeClear, rightEdge);
    rightEdge = DrawBadge(badgeBack, rightEdge);
    DrawFooterDivider(renderer, rightEdge);
  };

  // true when a dialog asked to leave the screen
  auto RunClear = [&]() -> bool
  {
    const DialogResult answer = RunDialog(DrawScreen, PanelDialog::Kind::ConfirmDestructive, "FAVORITES_CLEAR_CONFIRM", {Translate("FAVORITES_CLEAR_NOTE")}, "");
    if (answer != DialogResult::Confirmed)
      return answer == DialogResult::Quit;
    std::string errorMsg;
    bool ok = RemoveFavorites(state, errorMsg);
    folderExists = FolderExists();
    menuChanged = true;
    if (!ok)
      return RunDialog(DrawScreen, PanelDialog::Kind::Acknowledge, "FAVORITES_FAILED", {errorMsg}, "") == DialogResult::Quit;
    for (auto &g : state.games)
      g.isFavorite = false;
    resultKey = "FAVORITES_CLEARED";
    RefreshSummary();
    return false;
  };

  std::vector<std::string> gameLabels;
  for (const auto &game : state.games)
    gameLabels.push_back(game.name);
  ListJumpInput jumpInput;

  for (;;)
  {
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
      if (e.type == SDL_QUIT)
        return ExitLeaveMenu;
    }

    controller.Update();
    if (sdl_context.powerwatch->buttonPress())
      return ExitLeaveMenu;

    const int totalRows = static_cast<int>(state.games.size()) + selectAllRows;
    const ListJump jump = jumpInput.Poll(controller, SDL_GetTicks());

    if (controller.HeldRepeat(UP) && currentId > 0)
    {
      --currentId;
      if (currentId < listOffset)
        listOffset = currentId;
    }
    else if (controller.HeldRepeat(DOWN) && currentId + 1 < totalRows)
    {
      ++currentId;
      if (currentId >= listOffset + displayCount)
        listOffset = currentId - displayCount + 1;
    }
    else if (jump != ListJump::None)
    {
      int windowRow = currentId - listOffset;
      ApplyListJump(jump, gameLabels, selectAllRows, displayCount, listOffset, windowRow);
      currentId = listOffset + windowRow;
    }
    else if (controller.GetButtonStatus(A))
    {
      if (currentId == 0)
      {
        bool markAll = (markedCount != static_cast<int>(state.games.size()));
        for (auto &g : state.games)
          g.isFavorite = markAll;
      }
      else
      {
        int gameIdx = currentId - selectAllRows;
        state.games[gameIdx].isFavorite = !state.games[gameIdx].isFavorite;
      }
      resultKey.clear();
      RefreshSummary();
    }
    else if (controller.GetButtonStatus(Y))
    {
      for (auto &g : state.games)
      {
        g.isFavorite = !g.isFavorite;
      }
      resultKey.clear();
      RefreshSummary();
    }
    else if ((markedCount > 0 || folderExists) && (controller.GetButtonStatus(X) || controller.GetButtonStatus(START)))
    {
      if (markedCount == 0)
      {
        if (RunClear())
          return ExitLeaveMenu;
        continue;
      }
      std::vector<std::string> promptLines = {
          Translate("FAVORITES_APPLY_BODY_1") + ": " + std::to_string(markedCount),
          Translate("FAVORITES_APPLY_BODY_2") + " (" + GetFavoritesCode(state) + ")"};
      if (markedCount > MaxSafeFavorites)
      {
        promptLines.push_back(Translate("FAVORITES_LIMIT_WARNING"));
      }

      const DialogResult answer = RunDialog(DrawScreen, PanelDialog::Kind::Confirm, "FAVORITES_APPLY_CONFIRM", promptLines, "FAVORITES_APPLY_NOTE");
      if (answer == DialogResult::Quit)
        return ExitLeaveMenu;
      if (answer == DialogResult::Confirmed)
      {
        std::string assetsDir = optionsLocation + "favorites_manager/assets";
        std::string errorMsg;
        bool ok = ApplyFavorites(state, assetsDir, errorMsg);
        folderExists = FolderExists();
        menuChanged = true;
        if (ok)
        {
          resultKey = "FAVORITES_SAVED";
          RefreshStats();
        }
        else if (RunDialog(DrawScreen, PanelDialog::Kind::Acknowledge, "FAVORITES_FAILED", {errorMsg}, "") == DialogResult::Quit)
          return ExitLeaveMenu;
      }
    }
    else if (controller.GetButtonStatus(B))
    {
      return menuChanged ? ExitReloadMenu : ExitBack;
    }
    else if (folderExists && controller.GetButtonStatus(SELECT))
    {
      if (RunClear())
        return ExitLeaveMenu;
    }

    sdl_context.StartFrame();
    DrawScreen();
    SetDrawColor(renderer, bg);
    sdl_context.EndFrame();
  }
}
