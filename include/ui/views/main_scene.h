#ifndef _UI_VIEWS_MAIN_SCENE_H_
#define _UI_VIEWS_MAIN_SCENE_H_

#include "backends/target.h"
#include "ui/title_list.h"
#include <gsKit.h>
#include <stdint.h>

struct View;

typedef struct MainSceneData {
  TitleListView listView;
  char lineBuf[128];
  int exitRequested;
  int selectedIndex;
  int scrollOffset;
  uint32_t filterDeviceMask; // 0 = all devices; else bit i set = include device i
  int favoritesOnly;
  int favoritesOnTop;
  int sortAscending;
  int listDirty;
  int autoLaunchCountdown; // In Hz, depends on refresh rate
  GSTEXTURE *coverTexture;
  Target *coverTarget;
} MainSceneData;

struct View *mainSceneGetView(void);
MainSceneData *mainSceneGetData(void);

void mainSceneInit(void);
void mainSceneCleanup(void);

// Call before pushing main scene (from ui_main initViews). Pass views to push from main.
void mainSceneSetMenuView(struct View *v);
void mainSceneSetViewOptionsView(struct View *v);
void mainSceneSetTitleOptionsView(struct View *v);
void mainSceneSetTitleOptionsData(void *data);

#endif
