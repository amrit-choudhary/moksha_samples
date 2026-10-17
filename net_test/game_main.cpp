/**
 * Net test entry. --duration=<seconds> quits after that long, logging a summary first; the headless build
 * (Sample_NetTest_Headless) runs as a bot, so each one is a load-test client. --serverIP/--serverPort pick the server.
 */

#include <cstdlib>

#include "moksha/client/app/app_config.h"
#include "moksha/client/game/game_factory.h"
#include "net_test/game/game_net_test.h"

ME::Game* ME::CreateGame(const ME::AppArgs& args, ME::AppConfig& config) {
    config.windowTitle = "Net Test";

    double durationSeconds = 0.0;
    const char* duration = args.GetValue("duration");
    if (duration != nullptr) {
        durationSeconds = std::atof(duration);
    }
    return new Samples::GameNetTest(durationSeconds);
}

void ME::DestroyGame(ME::Game* game) {
    delete game;
}
