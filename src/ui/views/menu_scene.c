#include "ui/views/menu_scene.h"
#include "backends/backends.h"
#include "backends/cache.h"
#include "config/config.h"
#include "ui/draw.h"
#include "ui/font.h"
#include "ui/icons.h"
#include "ui/layout.h"
#include "ui/ui_shared.h"
#include "ui/view.h"
#include "ui/views/main_scene.h"
#include <libpad.h>
#include <stdio.h>
#include <string.h>

static View s_view;
static char s_osdBuf[96];
static int selected = 0;
static struct View *s_exitModalView;

void menuSetExitModalView(struct View *v) { s_exitModalView = v; }

static void menuOnEnter(View *v) { selected = 0; }

#define POPUP_W 240
#define POPUP_X (SCENE_VW - POPUP_W) / 2
#define POPUP_Y 120
const char *entries[] = {"Enabled devices", "Global title options", "Reload list", "Invalidate cache", "Exit"};
#define ENTRY_COUNT sizeof(entries) / sizeof(char *)

static void menuDraw(View *v, int zOrder) {
  // Dynamic modal height (ENTRY_COUNT + header + spacing)
  const int POPUP_H = SCENE_KEEPOUT + (SCENE_ROW_SIZE * (ENTRY_COUNT + 2));
  const int vx1 = POPUP_X, vy1 = POPUP_Y, vx2 = POPUP_X + POPUP_W, vy2 = POPUP_Y + POPUP_H;
  const int centerX = (vx1 + vx2) / 2;
  int leftX = vx1 + SCENE_KEEPOUT;
  int boxTop = vy1 + SCENE_KEEPOUT;

  // Background
  uiDrawRectRounded(gsGlobal, vx1, vy1, vx2, vy2, POPUP_ROUNDRECT_RADIUS, zOrder, ModalBGColor);
  // Header
  fontRenderString(FONT_DEFAULT, centerX, POPUP_Y + SCENE_KEEPOUT, FONT_ALIGN_HCENTER, 0, 0, zOrder, "Main menu", HeaderTextColor);
  // Entries
  int optionY = POPUP_Y + SCENE_KEEPOUT + SCENE_ROW_SIZE * 1.5;
  for (int i = 0; i < ENTRY_COUNT; i++) {
    uint64_t col = (i == selected) ? ColorSelected : FontMainColor;
    fontRenderString(FONT_DEFAULT, centerX, optionY, FONT_ALIGN_HCENTER, 0, 0, zOrder, entries[i], col);
    optionY += SCENE_ROW_SIZE;
  }
}

static ViewResult menuOnInput(View *v, int padInput) {
  MainSceneData *d = (MainSceneData *)v->userdata;
  if (d->exitRequested)
    return ViewResult_Pop;

  if (padInput & PAD_CIRCLE)
    return ViewResult_Pop;
  if (padInput & PAD_UP) {
    selected--;
    if (selected < 0)
      selected = 2;
    return ViewResult_Continue;
  }
  if (padInput & PAD_DOWN) {
    selected++;
    if (selected == ENTRY_COUNT)
      selected = 0;
    return ViewResult_Continue;
  }
  if (padInput & PAD_CROSS) {
    switch (selected) {
    case 0:
      // Enabled devices
      break;
    case 1:
      // Global options
      break;
    case 2:
      // Rescan all devices
      rescanAllBackendDevices();
      showOSD("List reloaded", 120);
      d->listDirty = 1;
      break;
    case 3:
      // Invalidate cache
      int n = getBackendDeviceCount();
      int done = 0;
      for (int i = 0; i < n; i++) {
        struct BackendDevice *dev = getBackendDeviceAt(i);
        if (dev && invalidateTitleIDCache(dev) == 0)
          done++;
      }
      snprintf(s_osdBuf, sizeof(s_osdBuf), "Cache invalidated on %d device(s)", done);
      showOSD(s_osdBuf, 120);
      break;
    case 4:
      // Exit
      s_exitModalView->userdata = d;
      viewStackPush(viewStack, s_exitModalView);
      break;
    }
  }
  return ViewResult_Continue;
}

struct View *menuGetView(void) {
  s_view = (View){
      .type = ViewType_Modal,
      .userdata = NULL,
      .onEnter = menuOnEnter,
      .onLeave = NULL,
      .draw = menuDraw,
      .onInput = menuOnInput,
  };
  return &s_view;
}
