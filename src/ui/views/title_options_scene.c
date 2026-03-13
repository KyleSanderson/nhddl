#include "backends/backends.h"
#include "backends/target.h"
#include "ui/draw.h"
#include "ui/ui_shared.h"
#include "ui/font.h"
#include "ui/icons.h"
#include "ui/view.h"
#include "ui/views/title_options_scene.h"
#include <libpad.h>
#include <stdio.h>

static View s_view;
static TitleOptionsSceneData s_data;

static void titleOptionsOnEnter(View *v) {
  (void)v;
}

static void titleOptionsDraw(View *v, int zOrder) {
  TitleOptionsSceneData *d = (TitleOptionsSceneData *)v->userdata;
  Target *t = d->target;
  if (!t) return;
  fontRenderString(FONT_DEFAULT, 80, 80, 0, 0, 0, zOrder, "Title options", HeaderTextColor);
  fontRenderString(FONT_DEFAULT, 80, 120, 0, 0, 0, zOrder, t->name ? t->name : "(no name)", FontMainColor);
  snprintf(d->lineBuf, sizeof(d->lineBuf), "Favorite: %s", (t->flags & TitleFlag_Favorite) ? "Yes" : "No");
  fontRenderString(FONT_DEFAULT, 80, 160, 0, 0, 0, zOrder, d->lineBuf, ColorSelected);
  snprintf(d->lineBuf, sizeof(d->lineBuf), "Fake DEV9: %s", (t->flags & TitleFlag_FakeDEV9) ? "Yes" : "No");
  fontRenderString(FONT_DEFAULT, 80, 200, 0, 0, 0, zOrder, d->lineBuf, FontMainColor);
  fontRenderString(FONT_DEFAULT, 80, 260, 0, 0, 0, zOrder, "Cross = toggle Favorite   Triangle = toggle Fake DEV9   Circle = back", FontMainColor);
}

static ViewResult titleOptionsOnInput(View *v, int padInput) {
  TitleOptionsSceneData *d = (TitleOptionsSceneData *)v->userdata;
  Target *t = d->target;
  if (!t) return ViewResult_Pop;
  if (padInput & PAD_CIRCLE)
    return ViewResult_Pop;
  if (padInput & PAD_CROSS) {
    uint32_t f = t->flags ^ TitleFlag_Favorite;
    updateTargetFlagsAndPersist(t->device, t, f);
    return ViewResult_Continue;
  }
  if (padInput & PAD_TRIANGLE) {
    uint32_t f = t->flags ^ TitleFlag_FakeDEV9;
    updateTargetFlagsAndPersist(t->device, t, f);
    return ViewResult_Continue;
  }
  return ViewResult_Continue;
}

struct View *titleOptionsGetView(void) {
  s_view = (View){
    .type = ViewType_Scene,
    .userdata = &s_data,
    .onEnter = titleOptionsOnEnter,
    .onLeave = NULL,
    .draw = titleOptionsDraw,
    .onInput = titleOptionsOnInput,
  };
  return &s_view;
}

TitleOptionsSceneData *titleOptionsGetData(void) {
  return &s_data;
}
