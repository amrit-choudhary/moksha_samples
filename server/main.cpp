/**
 * Samples_Server: the engine's game server with the net test sample's scores and quiz on top.
 */

#include "moksha/server/server_app.h"
#include "server/net_test_server.h"

int main(int argc, char** argv) {
    Samples::NetTestServer listener;
    ME::ServerApp app;
    return app.Run(argc, argv, listener);
}
