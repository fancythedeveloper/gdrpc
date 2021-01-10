#include "game_hooks.hpp"
#include "pch.h"

bool setupDone = false;

// this handles x button close
LONG_PTR oWindowProc;
LRESULT CALLBACK nWindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                             LPARAM lparam) {
  switch (msg) {
  case WM_CLOSE:
    // properly shutdown
    // idk what the correct function for this is tbh
    get_game_loop()->close();
    break;
  }
  return CallWindowProc((WNDPROC)oWindowProc, hwnd, msg, wparam, lparam);
}

HMODULE GetCurrentModule() {
  HMODULE hModule = NULL;
  GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    (LPCTSTR)GetCurrentModule, &hModule);

  return hModule;
}

typedef int(__thiscall *MenuLayerInitF)(void *menuLayer);
MenuLayerInitF mli;

int __fastcall MenuLayerInitH(void *menuLayer) {
  if (!setupDone) {
    if (auto logger = get_game_loop()->get_logger()) {
      logger->trace(FMT_STRING("menu layer setup called"));
    }

    get_game_loop()->register_on_initialize([] {
      // now we can log
      if (auto logger = get_game_loop()->get_logger()) {
        logger->info(FMT_STRING("initialized"));
      }
    });

    CreateThread(NULL, 0, mainThread, GetCurrentModule(), 0, NULL);
    setupDone = true;
  }
  return mli(menuLayer);
}

typedef void *(__fastcall *PlayLayerCreateF)(GJGameLevel *gameLevel);
PlayLayerCreateF plc;

void *__fastcall PlayLayerCreateH(GJGameLevel *gameLevel) {
  int levelID = gameLevel->levelID;

  Game_Loop *game_loop = get_game_loop();

  if (auto logger = game_loop->get_logger()) {
    logger->debug(FMT_STRING("PlayLayer::create called:\n\
levelID: {} @ {:#x}"),
                  levelID,
                  reinterpret_cast<int>(gameLevel));
  }

  if (game_loop->get_state() != playerState::editor ||
      game_loop->get_reset_timestamp()) {
    game_loop->set_update_timestamp(true);
  }

  game_loop->set_state(playerState::level);
  game_loop->set_update_presence(true);
  game_loop->set_gamelevel(gameLevel);

  return plc(gameLevel);
}

typedef void(__fastcall *PlayLayerOnQuitF)(void *playLayer);
PlayLayerOnQuitF ploq;

void __fastcall PlayLayerOnQuitH(void *playLayer) {
  Game_Loop *game_loop = get_game_loop();

  if (auto logger = game_loop->get_logger()) {
    logger->debug(FMT_STRING("PlayLayer::onQuit called"));
  }

  game_loop->set_state(playerState::menu);
  game_loop->set_update_timestamp(true);
  game_loop->set_update_presence(true);

  return ploq(playLayer);
}

typedef void *(__thiscall *PlayLayerShowNewBestF)(void *playLayer, char, float,
                                                  int, char, char, char);
PlayLayerShowNewBestF plsnb;

// to heck with your calling conventions
void *__fastcall PlayLayerShowNewBestH(void *playLayer, void *_edx, char p1,
                                       float p2, int p3, char p4, char p5,
                                       char p6) {
  Game_Loop *game_loop = get_game_loop();

  auto current_gamelevel = game_loop->get_gamelevel();
  int levelID = current_gamelevel->levelID;
  int new_best = current_gamelevel->normalPercent;

  if (auto logger = game_loop->get_logger()) {
    logger->debug(FMT_STRING("PlayLayer::showNewBest called\n\
levelID: {}, got {}%"),
                  levelID, new_best);
  }

  game_loop->set_update_presence(true);

  return plsnb(playLayer, p1, p2, p3, p4, p5, p6);
}

typedef void(__thiscall *EditorPauseLayerOnExitEditorF)(void *editorPauseLayer,
                                                        void *);
EditorPauseLayerOnExitEditorF eploee;

// thanks blaze for the other argument
void __fastcall EditorPauseLayerOnExitEditorH(void *editorPauseLayer,
                                              void *_edx, void *p1) {

  Game_Loop *game_loop = get_game_loop();

  if (auto logger = game_loop->get_logger()) {
    logger->debug(FMT_STRING("EditorPauseLayer::onExitEditor called"));
  }

  game_loop->set_state(playerState::menu);
  game_loop->set_update_timestamp(true);
  game_loop->set_update_presence(true);

  return eploee(editorPauseLayer, p1);
}

typedef void *(__fastcall *LevelEditorLayerCreateF)(GJGameLevel *gameLevel);
LevelEditorLayerCreateF lelc;

void *__fastcall LevelEditorLayerCreateH(GJGameLevel *gameLevel) {
  int levelID = gameLevel->levelID;

  Game_Loop *game_loop = get_game_loop();

  if (auto logger = game_loop->get_logger()) {
    logger->debug(FMT_STRING("LevelEditorLayer::create called:\n\
levelID: {} @ {:#x}"),
                  levelID,
                  reinterpret_cast<int>(gameLevel));
  }
  if (game_loop->get_state() != playerState::level ||
      game_loop->get_reset_timestamp()) {
    game_loop->set_update_timestamp(true);
  }
  game_loop->set_state(playerState::editor);
  game_loop->set_update_presence(true);

  game_loop->set_gamelevel(gameLevel);

  return lelc(gameLevel);
}

typedef void(__thiscall *CCDirectorEndF)(void *CCDirector);
CCDirectorEndF ccde;

void __fastcall CCDirectorEndH(void *CCDirector) {
  Game_Loop *game_loop = get_game_loop();

  if (auto logger = game_loop->get_logger()) {
    logger->debug(FMT_STRING("CCDirector::end called"));
  }

  game_loop->close();

  ccde(CCDirector);
}

// no need to export this, not putting in .h
struct game_hook {
  int *orig_addr;
  void *hook_fn;
  void **orig_fn;
  std::string fn_name;
};

void doTheHook() {
  Game_Loop *game_loop = get_game_loop();

  game_loop->initialize_config();
  auto logger = game_loop->get_logger();

  if (MH_Initialize() != MH_OK) {
    if (logger) {
      logger->critical("hook init error");
    }
    MessageBoxA(0, "Hook init error!", "Error", MB_OK);
    return;
  }

  std::string gd_name = game_loop->get_executable_name();

  HMODULE gd_handle = GetModuleHandleA(gd_name.c_str());
  HMODULE cocos_handle = LoadLibraryA("libcocos2d.dll");

  // close button calls this, x button calls wndproc
  if (!gd_handle) {
    if (logger) {
      logger->critical(FMT_STRING("failed to get gd exe by name {}"), gd_name);
    }
    MessageBoxA(0, "Failed to get Geometry Dash!", "Error", MB_OK);
    return;
  }

  if (logger) {
    logger->trace(FMT_STRING("found {} at {:#x}"), gd_name, (int)gd_handle);
  }

  if (!cocos_handle) {
    if (logger) {
      logger->critical("cocos grabbing error");
    }
    MessageBoxA(0, "Failed to get libcocos!", "Error", MB_OK);
    return;
  }

  // setup closes
  oWindowProc = SetWindowLongPtrA(GetForegroundWindow(), GWL_WNDPROC,
                                  (LONG_PTR)nWindowProc);

  // wall of hooks
  std::array<game_hook, 7> hooks{{
      {(int *)((int)gd_handle + 0xAF210),
       reinterpret_cast<void *>(&MenuLayerInitH),
       reinterpret_cast<void **>(&mli), "MenuLayer::init"},
      {(int *)((int)gd_handle + 0xE3530),
       reinterpret_cast<void *>(&PlayLayerCreateH),
       reinterpret_cast<void **>(&plc), "PlayLayer::create"},
      {(int *)((int)gd_handle + 0xF3B80),
       reinterpret_cast<void *>(&PlayLayerOnQuitH),
       reinterpret_cast<void **>(&ploq), "PlayLayer::onQuit"},
      {(int *)((int)gd_handle + 0xE5FF0),
       reinterpret_cast<void *>(&PlayLayerShowNewBestH),
       reinterpret_cast<void **>(&plsnb), "PlayLayer::showNewBest"},
      {(int *)((int)gd_handle + 0x3F380),
       reinterpret_cast<void *>(&EditorPauseLayerOnExitEditorH),
       reinterpret_cast<void **>(&eploee), "EditorPauseLayer::onExitEditor"},
      {(int *)((int)gd_handle + 0x8C220),
       reinterpret_cast<void *>(&LevelEditorLayerCreateH),
       reinterpret_cast<void **>(&lelc), "LevelEditorLayer::create"},
      {(int *)GetProcAddress(cocos_handle, "?end@CCDirector@cocos2d@@QAEXXZ"),
       reinterpret_cast<void *>(&CCDirectorEndH),
       reinterpret_cast<void **>(&ccde), "CCDirector::end"},
  }};

  for (const auto &hook : hooks) {
    MH_CreateHook(hook.orig_addr, hook.hook_fn, hook.orig_fn);
    if (logger) {
      logger->trace(FMT_STRING("hooking function {} at {:#x}"), hook.fn_name,
                    reinterpret_cast<int>(hook.orig_addr));
    }
    if (MH_EnableHook(hook.orig_addr) != MH_OK) {
      auto s =
          fmt::format(FMT_STRING("error hooking function {}"), hook.fn_name);
      MessageBoxA(0, s.c_str(), "gdrpc error", MB_OK);
      if (logger) {
        logger->critical(s);
      }
    }
  }

  if (logger) {
    logger->debug("hooks setup");
  }
}