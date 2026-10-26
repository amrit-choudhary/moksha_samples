/**
 * Systems check entry: the engine creates this sample's game through MOKSHA_GAME. settings.ini replaces the title,
 * which the config check relies on.
 */

#include "moksha/client/game/game_factory.h"
#include "systems_check/game/game_systems_check.h"

MOKSHA_GAME(Samples::GameSystemsCheck, "Systems Check")
