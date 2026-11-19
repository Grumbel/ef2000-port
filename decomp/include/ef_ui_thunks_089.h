/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_UI_THUNKS_089_H
#define EF_UI_THUNKS_089_H

#include "ef_types.h"

void QuitEF2000Yes(void);
void QuitEF2000No(void);
void guiClosedownHelp(void);
void UpdateMagnify(void);
void gnStartup(void);
void gnClosedown(void);
void rxmissStartup(void);
void gnNetAbort(void);
void rxmissMainloop(void);
void rxmissClosedown(void);
void txmissStartup(void);
void txmissMainloop(void);
void txmissClosedown(void);
void gnReadyToFly(void);
void gpwWeaponsAccepted(void);
void gpwGenerateCampaign(void);
void gpwClickCampaign(void);
void InitPickMiss(void);
void InitSim2(void);
void simShowDebrief(void);
uint32_t gpwSimTypeSelected(void);
void gpwContinueArcade(void);
void InitArcade2(void);
void arcNextDifficulty(void);
void gpwTestClicked(void);
void gpwClickTimeAdjust(void);
void gpwCloseDebrief(void);
void gpwNextFLight(void);
void QuitEF2000(void);
void sumaircraftgraph(void);
void sumgroundgraph(void);
void sumstaticgraph(void);
void sumgenbar(void);
void sumgencamp(void);
void openservicesell(void);
void entergamename(void);
void simrecallmiss(void);
void gpwUpdateTime(void);
void InitCustom(void);
void ChooseEnemy(void);
void gpwExitSimulator(void);
void onopenParams(void);
void gpwSelectLevel(void);
void kotsClickTeam(void);
void InitCampaign(void);
void gpwLoadSaveCampaign(void);
void gpwRedrawMap(void);
void gpwExitCampaign(void);
void gcClickMap(void);
void onopenDirectPlaySettings(void);

#endif
