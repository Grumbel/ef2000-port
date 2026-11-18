/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_UI_THUNKS_089_H
#define EF_UI_THUNKS_089_H

#include "ef_types.h"

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
void InitSim2(void);
void simShowDebrief(void);
uint32_t gpwSimTypeSelected(void);
void gpwContinueArcade(void);
void InitArcade2(void);

#ifdef EF_DECOMP_HOST_TEST

/* Host fixtures for the callbacks whose guest call ABI remains pending. */
typedef struct {
    uint32_t eax, edx, ebx, ecx;
} ef_ui_regs_089;

typedef ef_ui_regs_089 (*ef_ui_jump_fn_089)(ef_ui_regs_089);

/* The list has been audited byte-by-byte against the pinned X-32 image. */
#define EF_UI_JUMPS_089(X) \
    X(InitPickMiss,           84b74, 41028) \
    X(ChooseEnemy,            84b8c, 7ccdc) \
    X(gpwExitSimulator,       84b94, 7d2f8) \
    X(onopenParams,           84ba4, 7c770) \
    X(gpwSelectLevel,         84bbc, 494c) \
    X(arcNextDifficulty,      84bc4, 4e14) \
    X(kotsClickTeam,          84bcc, 4f18) \
    X(InitCampaign,           84bd4, 5630) \
    X(gpwLoadSaveCampaign,    84bdc, bfcc) \
    X(gpwRedrawMap,           84be4, a9a4) \
    X(gpwExitCampaign,        84bec, b1e0) \
    X(gpwTestClicked,         84bf4, c594) \
    X(gpwClickTimeAdjust,     84bfc, b544) \
    X(gpwCloseDebrief,        84c04, d550) \
    X(gpwNextFLight,          84c0c, d628) \
    X(gcClickMap,             84c14, e48c) \
    X(QuitEF2000,            84c1c, 42e8c) \
    X(onopenDirectPlaySettings,84c24, 98bec) \
    X(sumaircraftgraph,       84c2c, 99ee4) \
    X(sumgroundgraph,         84c34, 99fe0) \
    X(sumstaticgraph,         84c3c, 9a0dc) \
    X(sumgenbar,              84c44, 9a1d8) \
    X(sumgencamp,             84c54, 9bba4) \
    X(openservicesell,        84c5c, 34298) \
    X(entergamename,          84c64, 34390) \
    X(simrecallmiss,          84c6c, 7bc00) \
    X(gpwUpdateTime,          84c74, 42f7c) \
    X(InitCustom,             84c7c, 7e2a8)

/* Original 84c4c is another five-byte JMP to 9bcf8 but is not in the
 * canonical function inventory; it needs caller/boundary evidence before
 * being added as a recovered, distinct function. */

#define EF_DECLARE_UI_JUMP_089(name, addr, target) \
    ef_ui_regs_089 ef_ui_jump_##name##_##addr(ef_ui_regs_089 registers);
EF_UI_JUMPS_089(EF_DECLARE_UI_JUMP_089)
#undef EF_DECLARE_UI_JUMP_089

typedef struct {
    uint32_t original_entry, jump_target;
    const char *original_name;
    ef_ui_jump_fn_089 model;
} ef_ui_jump_entry_089;

extern const ef_ui_jump_entry_089 ef_ui_jump_table_089[];
extern const unsigned ef_ui_jump_table_size_089;

/* External guest-jump boundary; tests implement it. */
ef_ui_regs_089 ef_ui_jump_guest_089(uint32_t target, ef_ui_regs_089 registers);

/* Two named, 15-byte paired CALL/JMP wrappers adjacent to the thunk block. */
ef_ui_regs_089 ef_ui_quit_yes_84ac4(ef_ui_regs_089 registers);
ef_ui_regs_089 ef_ui_quit_no_84aec(ef_ui_regs_089 registers);

#endif

#endif
