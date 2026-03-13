#ifndef _UI_VIEWS_TITLE_OPTIONS_SCENE_H_
#define _UI_VIEWS_TITLE_OPTIONS_SCENE_H_

#include "backends/target.h"

struct View;

typedef struct TitleOptionsSceneData {
  Target *target;
  char lineBuf[128];
} TitleOptionsSceneData;

struct View *titleOptionsGetView(void);
TitleOptionsSceneData *titleOptionsGetData(void);

#endif
