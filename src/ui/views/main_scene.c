#include "ui/views/main_scene.h"
#include "backends/backends.h"
#include "backends/cache.h"
#include "config/config.h"
#include "config/neutrino_args.h"
#include "devices/utils.h"
#include "dprintf.h"
#include "neutrino/neutrino.h"
#include "ui/draw.h"
#include "ui/font.h"
#include "ui/icons.h"
#include "ui/layout.h"
#include "ui/scale.h"
#include "ui/title_list.h"
#include "ui/ui_shared.h"
#include "ui/view.h"
#include "ui/views/title_options_scene.h"
#include <gsKit.h>
#include <gsToolkit.h>
#include <libpad.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OVERLAY_DURATION_FRAMES 120

#define HEADER_Y (SCENE_KEEPOUT)
#define LIST_LEFT (SCENE_KEEPOUT * 2)
#define LIST_TOP (SCENE_KEEPOUT + SCENE_ROW_SIZE)
#define COVER_W 140
#define COVER_H 200
#define COVER_LEFT (SCENE_VW - LIST_LEFT - COVER_W)
#define LIST_RIGHT COVER_LEFT - SCENE_KEEPOUT
#define COVER_TOP (LIST_TOP + (SCENE_CONTENT_BOTTOM - LIST_TOP) / 2 - COVER_H / 2)
#define COVER_BORDER_MARGIN 2
#define ART_PATH "/ART"

static View s_view;
static MainSceneData s_data;
static struct View *s_menuView;
static struct View *s_viewOptionsView;
static struct View *s_titleOptionsView;
static void *s_titleOptionsData;
static int s_autolaunchCanceled;

void uiLaunchTarget();

void mainSceneSetMenuView(struct View *v) { s_menuView = v; }
void mainSceneSetViewOptionsView(struct View *v) { s_viewOptionsView = v; }
void mainSceneSetTitleOptionsView(struct View *v) { s_titleOptionsView = v; }
void mainSceneSetTitleOptionsData(void *data) { s_titleOptionsData = data; }

void mainSceneInit(void) {
  titleListViewInit(&s_data.listView);
  s_data.filterDeviceMask = 0;
  s_data.favoritesOnTop = 0;
  s_data.sortAscending = 1;
  s_data.exitRequested = 0;
  s_data.coverTexture = NULL;
  s_data.coverTarget = NULL;
  s_data.autoLaunchCountdown = getAutolaunchTimeout() * scaleGetRefreshRateHz();
  if (!s_data.autoLaunchCountdown)
    s_autolaunchCanceled = 1;
  s_data.selectedIndex = -1;
}

void mainSceneCleanup(void) { titleListViewFree(&s_data.listView); }

static void rebuildMainList(MainSceneData *d) {
  titleListViewClear(&d->listView);
  if (titleListViewBuildFromBackendsFiltered(&d->listView, d->filterDeviceMask, d->favoritesOnly) != 0) {
    DPRINTF("ui_main: failed to build title list\n");
    return;
  }
  titleListViewSort(&d->listView, d->sortAscending, d->favoritesOnTop);
  d->selectedIndex = 0;
  d->scrollOffset = 0;
  d->listDirty = 0;
}

static void loadMainSceneCover(MainSceneData *d, Target *t) {
  if (!t || !t->device || !t->id)
    return;
  struct BackendDevice *dev = t->device;
  if (dev->metadev)
    dev = dev->metadev;
  if (!d->coverTexture) {
    d->coverTexture = calloc(1, sizeof(GSTEXTURE));
    if (!d->coverTexture)
      return;
    d->coverTexture->Delayed = 1;
    d->coverTexture->Filter = GS_FILTER_LINEAR;
  }
  if (d->coverTexture->Mem) {
    free(d->coverTexture->Mem);
    d->coverTexture->Mem = NULL;
  }
  if (d->coverTexture->Clut) {
    free(d->coverTexture->Clut);
    d->coverTexture->Clut = NULL;
  }
  d->coverTarget = t;
  snprintf(d->lineBuf, sizeof(d->lineBuf), "%s%s/%s_COV.png", dev->mountpoint, ART_PATH, t->id);
  gsKit_TexManager_invalidate(gsGlobal, d->coverTexture);
  if (loadPNGFromFile(gsGlobal, d->coverTexture, d->lineBuf)) {
    d->coverTexture->Width = 0;
    d->coverTexture->Height = 0;
    if (d->coverTexture->Mem)
      free(d->coverTexture->Mem);
    if (d->coverTexture->Clut)
      free(d->coverTexture->Clut);
    return;
  }
  gsKit_TexManager_bind(gsGlobal, d->coverTexture);
}

static void mainSceneOnEnter(View *v) {
  MainSceneData *d = (MainSceneData *)v->userdata;
  // Only rebuild and reset list position when first entering
  if (d->selectedIndex == -1) {
    d->listDirty = 1;
    rebuildMainList(d);

    // Set list view to the last launch target across all devices
    Target *last = getLastLaunchedTarget();
    d->selectedIndex = titleListViewGetIdx(&d->listView, last);
    if (d->selectedIndex < 0) { // Not found, cancel autolaunch
      s_autolaunchCanceled = 1;
      d->autoLaunchCountdown = 0;
      d->selectedIndex = 0;
    }
  }
}

static void mainSceneOnLeave(View *v) {}

static void mainSceneDraw(View *v, int zOrder) {
  MainSceneData *d = (MainSceneData *)v->userdata;
  if (d->listDirty) {
    Target *selected = titleListViewGetAt(&d->listView, d->selectedIndex);
    rebuildMainList(d);
    // Try to find the selected target after list rebuild
    d->selectedIndex = titleListViewGetIdx(&d->listView, selected);
    if (d->selectedIndex < 0)
      d->selectedIndex = 0;
  }
  int count = titleListViewCount(&d->listView);

  // Header
  fontRenderInRect(FONT_DEFAULT, 0, HEADER_Y, SCENE_VW, LIST_TOP, FONT_ALIGN_TOP | FONT_ALIGN_HCENTER, zOrder, "Title List", HeaderTextColor);
  if (count > 0) {
    snprintf(d->lineBuf, sizeof(d->lineBuf), "%d / %d", d->selectedIndex + 1, count);
    fontRenderInRect(FONT_DEFAULT, 0, HEADER_Y, SCENE_VW - LIST_LEFT, LIST_TOP, FONT_ALIGN_TOP | FONT_ALIGN_RIGHT, zOrder, d->lineBuf,
                     HeaderTextColor);
  }
  // Button prompts
  uiDrawPrompt(gsGlobal, SCENE_PROMPT_SLOT_CENTER_X(0), SCENE_PROMPT_ICON_ROW_Y, SCENE_PROMPT_TEXT_ROW_Y, zOrder, HeaderTextColor, ICON_TRIANGLE,
               "Title options");
  uiDrawPrompt(gsGlobal, SCENE_PROMPT_SLOT_CENTER_X(1), SCENE_PROMPT_ICON_ROW_Y, SCENE_PROMPT_TEXT_ROW_Y, zOrder, HeaderTextColor, ICON_SELECT,
               "View");
  uiDrawPrompt(gsGlobal, SCENE_PROMPT_SLOT_CENTER_X(2), SCENE_PROMPT_ICON_ROW_Y, SCENE_PROMPT_TEXT_ROW_Y, zOrder, HeaderTextColor, ICON_START,
               "Menu");
  uiDrawPrompt(gsGlobal, SCENE_PROMPT_SLOT_CENTER_X(3), SCENE_PROMPT_ICON_ROW_Y, SCENE_PROMPT_TEXT_ROW_Y, zOrder, HeaderTextColor, ICON_CROSS,
               "Launch");

  // Title list
  if (count == 0) {
    fontRenderInRect(FONT_DEFAULT, 0, 0, SCENE_VW, SCENE_CONTENT_BOTTOM, FONT_ALIGN_CENTER, zOrder, "No titles", FontMainColor);
    return;
  }
  // Cover art: ratio-preserving border and content so they match in widescreen.
  // In widescreen draw at 1.5x size, centered in the same slot.
  int coverW = COVER_W, coverH = COVER_H, coverLeft = COVER_LEFT, coverTop = COVER_TOP;
  if (scaleGetDisplayAspect() == ASPECT_16_9) {
    coverW = COVER_W * 1.2;
    coverH = COVER_H * 1.2;
    coverLeft = COVER_LEFT + COVER_W / 2 - coverW / 2;
    coverTop = COVER_TOP + COVER_H / 2 - coverH / 2;
  }
  uiDrawRectRatioPreserving(gsGlobal, coverLeft - COVER_BORDER_MARGIN, coverTop - COVER_BORDER_MARGIN, coverW + COVER_BORDER_MARGIN * 2,
                            coverH + COVER_BORDER_MARGIN * 2, zOrder, FontMainColor);

  Target *cur = (count > 0 && d->selectedIndex >= 0 && d->selectedIndex < count) ? titleListViewGetAt(&d->listView, d->selectedIndex) : NULL;
  if (cur && cur != d->coverTarget)
    loadMainSceneCover(d, cur);
  if (d->coverTexture && d->coverTarget && d->coverTexture->Width > 0) {
    gsKit_TexManager_bind(gsGlobal, d->coverTexture);
    // Some cover art might have inverted alpha values; work around this issue by disabling alpha test for cover art
    uiDrawTextureInVirtualRect(gsGlobal, coverLeft, coverTop, coverW, coverH, d->coverTexture, zOrder + 1, FontMainColor, 1);
  } else {
    uiDrawTextureInVirtualRect(gsGlobal, coverLeft, coverTop, coverW, coverH, NULL, zOrder + 1, BGColor, 0);
    fontRenderInRect(FONT_DEFAULT, coverLeft, coverTop, coverLeft + coverW, coverTop + coverH, FONT_ALIGN_CENTER, zOrder + 1, "No cover",
                     FontMainColor);
  }
  if (cur) {
    if (cur->id)
      fontRenderInRect(FONT_DEFAULT, coverLeft, (coverTop + coverH), (coverLeft + coverW), 0, FONT_ALIGN_CENTER, zOrder, cur->id, HeaderTextColor);
    if (cur->device && cur->device->type) {
      if (getBackendDeviceCountByType(cur->device->type) > 1)
        snprintf(d->lineBuf, sizeof(d->lineBuf), "%s %d", getDeviceString(cur->device->type), cur->device->index);
      else
        strncpy(d->lineBuf, getDeviceString(cur->device->type), sizeof(d->lineBuf));
      fontRenderInRect(FONT_DEFAULT, coverLeft, (coverTop + coverH + fontGetLineHeight(FONT_DEFAULT) + 2), (coverLeft + coverW), 0, FONT_ALIGN_CENTER,
                       zOrder, d->lineBuf, HeaderTextColor);
    }
  }

  int lineHeight = fontGetLineHeight(FONT_DEFAULT);
  if (lineHeight <= 0)
    lineHeight = 20;
  int maxVisible = (SCENE_CONTENT_BOTTOM - LIST_TOP) / lineHeight;
  if (maxVisible < 1)
    maxVisible = 1;
  if (d->selectedIndex < d->scrollOffset)
    d->scrollOffset = d->selectedIndex;
  if (d->selectedIndex >= d->scrollOffset + maxVisible)
    d->scrollOffset = d->selectedIndex - maxVisible + 1;
  for (int i = 0; i < maxVisible; i++) {
    int idx = d->scrollOffset + i;
    if (idx >= count)
      break;
    Target *t = titleListViewGetAt(&d->listView, idx);
    int y = LIST_TOP + i * lineHeight;
    uint64_t col = (idx == d->selectedIndex) ? ColorSelected : FontMainColor;
    const char *name = (t && t->name) ? t->name : "(no name)";
    fontRenderString(FONT_DEFAULT, LIST_LEFT, y, 0, LIST_RIGHT - LIST_LEFT, 0, zOrder, name, col);
  }

  // Handle autolaunch
  if (d->autoLaunchCountdown > 0) {
    int sec = (d->autoLaunchCountdown + 59) / scaleGetRefreshRateHz();
    snprintf(d->lineBuf, sizeof(d->lineBuf), "Launching in %d s... (any button to cancel)", sec);
    showOSD(d->lineBuf, 1);
  }
}

static ViewResult mainSceneOnInput(View *v, int padInput) {
  MainSceneData *d = (MainSceneData *)v->userdata;

  // Handle autolaunch
  if (!s_autolaunchCanceled) {
    if (!d->autoLaunchCountdown) {
      uiLaunchTarget();
      return ViewResult_Continue;
    }
    d->autoLaunchCountdown--;
    if (padInput) {
      s_autolaunchCanceled = 1;
      d->autoLaunchCountdown = 0;
    }
  }

  if (d->exitRequested)
    return ViewResult_Pop;

  int count = titleListViewCount(&d->listView);

  if (padInput & PAD_START) {
    if (s_menuView) {
      s_menuView->userdata = d;
      viewStackPush(viewStack, s_menuView);
    }
    return ViewResult_Continue;
  }
  if (padInput & PAD_SELECT) {
    if (s_viewOptionsView && count > 0) {
      s_viewOptionsView->userdata = d;
      viewStackPush(viewStack, s_viewOptionsView);
    }
    return ViewResult_Continue;
  }
  if (padInput & PAD_TRIANGLE) {
    if (count > 0 && d->selectedIndex >= 0 && d->selectedIndex < count && s_titleOptionsView && s_titleOptionsData) {
      TitleOptionsSceneData *opt = (TitleOptionsSceneData *)s_titleOptionsData;
      opt->target = titleListViewGetAt(&d->listView, d->selectedIndex);
      viewStackPush(viewStack, s_titleOptionsView);
    }
    return ViewResult_Continue;
  }
  if (count > 0) {
    if (padInput & PAD_UP) {
      d->selectedIndex--;
      if (d->selectedIndex < 0)
        d->selectedIndex = count - 1;
      return ViewResult_Continue;
    }
    if (padInput & PAD_DOWN) {
      d->selectedIndex++;
      if (d->selectedIndex >= count)
        d->selectedIndex = 0;
      return ViewResult_Continue;
    }
    if ((padInput & PAD_CROSS) && d->selectedIndex >= 0 && d->selectedIndex < count) {
      uiLaunchTarget();
      return ViewResult_Continue;
    }
  }
  return ViewResult_Continue;
}

struct View *mainSceneGetView(void) {
  s_view = (View){
      .type = ViewType_Scene,
      .userdata = &s_data,
      .onEnter = mainSceneOnEnter,
      .onLeave = mainSceneOnLeave,
      .draw = mainSceneDraw,
      .onInput = mainSceneOnInput,
  };
  return &s_view;
}

MainSceneData *mainSceneGetData(void) { return &s_data; }

void uiLaunchTarget() {
  Target *t = titleListViewGetAt(&s_data.listView, s_data.selectedIndex);
  ArgumentList *globalArgs = calloc(1, sizeof(ArgumentList));
  ArgumentList *titleArgs = calloc(1, sizeof(ArgumentList));
  if (globalArgs && titleArgs) {
    loadGlobalNeutrinoArguments(globalArgs, t->device);
    loadTitleNeutrinoArguments(titleArgs, t);
    ArgumentList *merged = mergeNeutrinoArguments(globalArgs, titleArgs);
    if (merged) {
      updateLastLaunchedTitle(t);
      int res = launchTarget(t, merged);
      freeArgumentList(merged);
      if (res == 0)
        return;
      snprintf(s_data.lineBuf, sizeof(s_data.lineBuf), "Launch failed: %d", res);
      showOSD(s_data.lineBuf, OVERLAY_DURATION_FRAMES);
    }
    freeArgumentList(globalArgs);
    freeArgumentList(titleArgs);
  }
  free(globalArgs);
  free(titleArgs);
}
