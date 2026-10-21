/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#include "framework/badge.h"
#include "framework/dialog.h"
#include "framework/sdl_helper.h"
#include "framework/controller.h"
#include "framework/powerwatch.h"
#include "framework/draw_helpers.h"
#include "framework/list_jump.h"
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

  int TextWidth(const std::string &text, int fontSize)
  {
    if (CanRenderWithTTF(text, fontSize))
      return MeasureTTFWidth(text, fontSize);
    return Utf8Length(text) * fontSize;
  }

  std::string TruncateToWidth(const std::string &text, int fontSize, int maxWidth)
  {
    if (TextWidth(text, fontSize) <= maxWidth)
      return text;
    int n = Utf8Length(text);
    while (n > 0 && TextWidth(TruncateUtf8(text, n) + "...", fontSize) > maxWidth)
      --n;
    return TruncateUtf8(text, n) + "...";
  }

  std::vector<std::string> WrapToWidth(const std::string &text, int fontSize, int maxWidth)
  {
    std::vector<std::string> lines;
    std::string line;
    size_t pos = 0;
    while (pos < text.size())
    {
      size_t end = text.find(' ', pos);
      std::string word = text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
      pos = (end == std::string::npos) ? text.size() : end + 1;

      std::string candidate = line.empty() ? word : line + " " + word;
      if (TextWidth(candidate, fontSize) <= maxWidth)
      {
        line = candidate;
        continue;
      }
      if (!line.empty())
        lines.push_back(line);
      line = word;
      while (TextWidth(line, fontSize) > maxWidth && Utf8Length(line) > 1)
      {
        int n = Utf8Length(line) - 1;
        while (n > 1 && TextWidth(TruncateUtf8(line, n), fontSize) > maxWidth)
          --n;
        std::string head = TruncateUtf8(line, n);
        lines.push_back(head);
        line = line.substr(head.size());
      }
    }
    if (!line.empty())
      lines.push_back(line);
    return lines;
  }

  std::vector<Texture> MakeLines(const std::vector<std::string> &lines, int fontSize, SDL_Renderer *renderer, Color color)
  {
    std::vector<Texture> textures;
    for (const std::string &line : lines)
      textures.emplace_back(line, fontSize, renderer, 0, 0, false, ToAbgr(color), true);
    return textures;
  }

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

  auto DropPendingInput = [&]()
  {
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
    }
    controller.Reset();
  };
  DropPendingInput();

  const Color bg = UiTheme::Bg;
  SetDrawColor(renderer, bg);

  Texture gearIcon(optionsLocation + UiTheme::AssetGear, renderer, UiTheme::GearX, UiTheme::GearY);
  Texture appTitleText("Favorites Manager", UiTheme::TitleFontSize, renderer, UiTheme::TitleX, UiTheme::TitleY, false, ToAbgr(UiTheme::Text), true);
  appTitleText.rect.y -= appTitleText.rect.h / 2;
  Texture appVersionText(MOD_VERSION, UiTheme::VersionFontSize, renderer, appTitleText.rect.x + appTitleText.rect.w + UiTheme::VersionGap, UiTheme::TitleY, false, ToAbgr(UiTheme::Text), true);
  appVersionText.rect.y -= appVersionText.rect.h / 2;
  Texture creditText("Favorites Manager - by DefKorns", 16, renderer, UiTheme::CreditX, UiTheme::CreditY, false, ToAbgr(UiTheme::Text), true);
  Texture titleText(Translate("FAVORITES_TITLE"), UiTheme::SectionTitleFontSize, renderer, UiTheme::SectionTitleX, UiTheme::SectionTitleY, false, ToAbgr(UiTheme::Text), true);

  const int RowBoxH = UiTheme::RowPitch - 6;
  Texture checkboxOn(optionsLocation + UiTheme::AssetCheckboxOn, renderer);
  SetColorMod(checkboxOn.texture.get(), UiTheme::Accent);
  Texture checkboxOff(optionsLocation + UiTheme::AssetCheckboxOff, renderer);
  SetColorMod(checkboxOff.texture.get(), UiTheme::CheckboxOff);

  const int CheckboxSize = 20;
  const int CheckboxX = ControlRightX - CheckboxSize;
  const int RowTextMaxW = CheckboxX - 16 - UiTheme::RowTextX;
  const int displayCount = std::max(1, (UiTheme::FooterDividerY - UiTheme::ListBottomMargin - UiTheme::RowFirstY) / UiTheme::RowPitch);

  Texture scrollUp(optionsLocation + UiTheme::AssetChevronUp, renderer, UiTheme::ScrollX, UiTheme::ScrollUpY);
  SetColorMod(scrollUp.texture.get(), UiTheme::ScrollArrow);
  Texture scrollDown = scrollUp;
  scrollDown.rect.y = UiTheme::ScrollDownY;

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
  const int NoteIconCX = noteBox.x + 26;
  const int NoteTextX = noteBox.x + NoteTextOffset;

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

  auto DrawBadge = [&](Badge &badge, int rightEdgeX) -> int
  {
    return badges.Draw(badge, rightEdgeX, UiTheme::BadgeBandY);
  };

  auto DrawFooterDivider = [&](int rightEdge)
  {
    int dividerX = rightEdge - UiTheme::BadgeDividerGapFromCluster;
    DrawVLine(renderer, dividerX, UiTheme::FooterY + 10, UiTheme::FooterY + UiTheme::FooterH - 10, UiTheme::Border, 2);
  };

  auto DrawChrome = [&]()
  {
    DrawStrokeRect(renderer, UiTheme::OuterRect, UiTheme::Border, UiTheme::BorderWidth, UiTheme::BorderRadius);
    gearIcon.Draw(renderer);
    appTitleText.Draw(renderer);
    appVersionText.Draw(renderer);
    DrawHLine(renderer, UiTheme::HeaderDividerX, UiTheme::HeaderDividerX + UiTheme::HeaderDividerW, UiTheme::HeaderDividerY, UiTheme::Border, UiTheme::BorderWidth);
    DrawHLine(renderer, UiTheme::OuterRect.x, UiTheme::OuterRect.x + UiTheme::OuterRect.w, UiTheme::FooterDividerY, UiTheme::Border, UiTheme::BorderWidth);
    int accentBarY = titleText.rect.y + (titleText.rect.h - UiTheme::SectionAccentBarH) / 2;
    DrawFillRect(renderer, {UiTheme::SectionTitleX - UiTheme::SectionAccentBarW - 14, accentBarY, UiTheme::SectionAccentBarW, UiTheme::SectionAccentBarH}, UiTheme::Accent);
    titleText.Draw(renderer);
    creditText.Draw(renderer);
  };

  auto DrawRowFrame = [&](int index, bool selected) -> SDL_Rect
  {
    SDL_Rect rowRect{UiTheme::ListX, UiTheme::RowFirstY + index * UiTheme::RowPitch, ListRightX - UiTheme::ListX, RowBoxH};
    if (selected)
    {
      DrawRoundedFillRect(renderer, rowRect, UiTheme::SelectedRowBg, UiTheme::BoxRadius);
      DrawStrokeRect(renderer, rowRect, UiTheme::Accent, 2, UiTheme::BoxRadius);
    }
    else
    {
      DrawHLine(renderer, rowRect.x, rowRect.x + rowRect.w, rowRect.y + rowRect.h + 3, UiTheme::Border, 1);
    }
    return rowRect;
  };

  auto DrawNote = [&](std::vector<Texture> &lines)
  {
    DrawStrokeRect(renderer, noteBox, UiTheme::Border, UiTheme::BorderWidth, UiTheme::BoxRadius);
    int noteCenterY = noteBox.y + noteBox.h / 2;
    DrawStrokeRect(renderer, {NoteIconCX - 9, noteCenterY - 9, 18, 18}, UiTheme::TextDim, 2, 9);
    DrawFillRect(renderer, {NoteIconCX - 1, noteCenterY - 5, 2, 2}, UiTheme::TextDim);
    DrawFillRect(renderer, {NoteIconCX - 1, noteCenterY - 1, 2, 6}, UiTheme::TextDim);
    int y = noteCenterY - (static_cast<int>(lines.size()) * NoteLinePitch) / 2;
    for (Texture &line : lines)
    {
      line.rect.x = NoteTextX;
      line.rect.y = y + (NoteLinePitch - line.rect.h) / 2;
      line.Draw(renderer);
      y += NoteLinePitch;
    }
  };

  bool quitRequested = false;
  auto RunDialog = [&](const std::function<void()> &drawUnder, const std::string &titleKey, const std::vector<std::string> &bodyLines,
                       const std::string &noteKey, bool confirm) -> bool
  {
    DropPendingInput();
    const int boxW = 600;
    const int bodyPitch = 30;
    const int badgeRowH = Dialog::BadgeRowGap + UiTheme::BadgeOuterSize;
    std::vector<Texture> body;
    for (const std::string &line : bodyLines)
      body.emplace_back(TruncateToWidth(line, PanelFontSize, boxW - 80), PanelFontSize, renderer, 0, 0, false, ToAbgr(UiTheme::Text), true);
    std::vector<Texture> dialogNoteLines = noteKey.empty() ? std::vector<Texture>()
                                                           : MakeLines(WrapToWidth(Translate(noteKey), NoteFontSize, boxW - 48), NoteFontSize, renderer, UiTheme::TextDim);
    const int titlePitch = 32;
    std::vector<Texture> titleLines;
    for (const std::string &line : WrapToWidth(Translate(titleKey), Dialog::TitleFontSize, boxW - 48))
      titleLines.emplace_back(line, Dialog::TitleFontSize, renderer, 640, 0, true, ToAbgr(UiTheme::Text), true);
    const int titleExtraH = std::max(0, static_cast<int>(titleLines.size()) - 1) * titlePitch;
    const int boxH = 110 + titleExtraH + static_cast<int>(body.size()) * bodyPitch + static_cast<int>(dialogNoteLines.size()) * 20 + badgeRowH;
    const SDL_Rect box = Dialog::PanelRect(boxW, boxH);
    for (size_t i = 0; i < titleLines.size(); ++i)
      titleLines[i].rect.y = box.y + 36 + static_cast<int>(i) * titlePitch - titleLines[i].rect.h / 2;

    Badge dialogOk = MakeBadge("A", "HINT_OK", UiTheme::BadgeADark, UiTheme::BadgeA);
    Badge confirmPill = badges.MakePill("Start", Translate("HINT_CONFIRM"));
    const int badgeCenterY = box.y + box.h - Dialog::BottomPad - UiTheme::BadgeOuterSize;

    for (;;)
    {
      SDL_Event e;
      while (SDL_PollEvent(&e))
        if (e.type == SDL_QUIT)
        {
          quitRequested = true;
          return false;
        }
      controller.Update();
      if (sdl_context.powerwatch->buttonPress() || (confirm && controller.GetButtonStatus(B)))
      {
        DropPendingInput();
        return false;
      }
      if (confirm && (controller.GetButtonStatus(START) || controller.GetButtonStatus(A)))
      {
        DropPendingInput();
        return true;
      }
      if (!confirm && controller.GetButtonStatus(A))
      {
        DropPendingInput();
        return true;
      }

      sdl_context.StartFrame();
      drawUnder();
      Dialog::DrawBackdrop(renderer);
      Dialog::DrawPanel(renderer, box);
      for (Texture &t : titleLines)
        t.Draw(renderer);
      int bodyY = box.y + 36 + static_cast<int>(titleLines.size()) * titlePitch + 8;
      for (Texture &t : body)
      {
        t.rect.x = 640 - t.rect.w / 2;
        t.rect.y = bodyY;
        t.Draw(renderer);
        bodyY += bodyPitch;
      }
      int noteY = bodyY + 8;
      for (Texture &t : dialogNoteLines)
      {
        t.rect.x = 640 - t.rect.w / 2;
        t.rect.y = noteY;
        t.Draw(renderer);
        noteY += 20;
      }
      if (confirm)
      {
        badges.DrawCentered({&badgeBack, &confirmPill}, 640, badgeCenterY);
      }
      else
      {
        badges.DrawCentered({&dialogOk}, 640, badgeCenterY);
      }
      SetDrawColor(renderer, bg);
      sdl_context.EndFrame();
    }
  };

  auto ShowBusy = [&](const std::function<void()> &drawUnder, const std::string &messageKey)
  {
    sdl_context.StartFrame();
    drawUnder();
    Dialog::DrawBackdrop(renderer);
    const int boxW = 500, boxH = 120;
    const SDL_Rect box = Dialog::PanelRect(boxW, boxH);
    Dialog::DrawPanel(renderer, box);
    Texture busyText(Translate(messageKey), Dialog::TitleFontSize, renderer, 640, box.y + boxH / 2, true, ToAbgr(UiTheme::Text), true);
    busyText.rect.y -= busyText.rect.h / 2;
    busyText.Draw(renderer);
    SetDrawColor(renderer, bg);
    sdl_context.EndFrame();
  };

  ShowBusy([&]()
           { DrawChrome(); }, "FAVORITES_WORKING");

  FavoritesState state;
  bool scanOk = ScanFavoritesState(state, customGamesPath);

  if (!scanOk || state.games.empty())
  {
    auto DrawEmptyScreen = [&]()
    {
      DrawChrome();
    };
    RunDialog(DrawEmptyScreen, "FAVORITES_NO_GAMES_FOUND", {Translate("FAVORITES_CHECK_USB_NOTE")}, "", false);
    return quitRequested ? ExitLeaveMenu : ExitBack;
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
  std::vector<Texture> noteLines = MakeLines(noteText, NoteFontSize, renderer, UiTheme::TextDim);

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
    DrawChrome();

    const int totalRows = static_cast<int>(state.games.size()) + selectAllRows;
    for (int i = 0; i < displayCount && listOffset + i < totalRows; ++i)
    {
      const int row = listOffset + i;
      const bool selected = (row == currentId);
      SDL_Rect rowRect = DrawRowFrame(i, selected);
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

    if (listOffset > 0)
      scrollUp.Draw(renderer);
    if (listOffset + displayCount < totalRows)
      scrollDown.Draw(renderer, SDL_FLIP_VERTICAL);

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

    DrawNote(noteLines);

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
    DrawFooterDivider(rightEdge);
  };

  auto RunClear = [&]()
  {
    if (!RunDialog(DrawScreen, "FAVORITES_CLEAR_CONFIRM", {Translate("FAVORITES_CLEAR_NOTE")}, "", true))
      return;
    std::string errorMsg;
    bool ok = RemoveFavorites(state, errorMsg);
    folderExists = FolderExists();
    menuChanged = true;
    if (!ok)
    {
      RunDialog(DrawScreen, "FAVORITES_FAILED", {errorMsg}, "", false);
      return;
    }
    for (auto &g : state.games)
      g.isFavorite = false;
    resultKey = "FAVORITES_CLEARED";
    RefreshSummary();
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
    if (quitRequested)
      return ExitLeaveMenu;

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
        RunClear();
        continue;
      }
      std::vector<std::string> promptLines = {
          Translate("FAVORITES_APPLY_BODY_1") + ": " + std::to_string(markedCount),
          Translate("FAVORITES_APPLY_BODY_2") + " (" + GetFavoritesCode(state) + ")"};
      if (markedCount > MaxSafeFavorites)
      {
        promptLines.push_back(Translate("FAVORITES_LIMIT_WARNING"));
      }

      if (RunDialog(DrawScreen, "FAVORITES_APPLY_CONFIRM", promptLines, "FAVORITES_APPLY_NOTE", true))
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
        else
        {
          RunDialog(DrawScreen, "FAVORITES_FAILED", {errorMsg}, "", false);
        }
      }
    }
    else if (controller.GetButtonStatus(B))
    {
      return menuChanged ? ExitReloadMenu : ExitBack;
    }
    else if (folderExists && controller.GetButtonStatus(SELECT))
    {
      RunClear();
    }

    sdl_context.StartFrame();
    DrawScreen();
    SetDrawColor(renderer, bg);
    sdl_context.EndFrame();
  }
}
