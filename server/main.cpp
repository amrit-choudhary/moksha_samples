/**
 * Samples_Server: the engine's game server with the net test sample's scores and quiz and the rally sample's match
 * on top. One process serves both: SamplesServer hands each game verb to the listener that owns it.
 */

#include "moksha/server/server_app.h"
#include "server/net_test_server.h"
#include "server/rally_server.h"

namespace Samples {

class SamplesServer : public ME::IGameServerListener {
   public:
    void OnStart(ME::ServerApp& app) override {
        netTest.OnStart(app);
        rally.OnStart(app);
    }

    void OnEnd() override {
        rally.OnEnd();
        netTest.OnEnd();
    }

    void OnClientConnected(uint8_t clientID) override {
        netTest.OnClientConnected(clientID);
        rally.OnClientConnected(clientID);
    }

    // Verbs from RALLY_INPUT on are the rally's; the ones before it are the net test's.
    void OnGameMessage(uint8_t clientID, uint8_t verb, ME::ByteReader& reader) override {
        if (verb >= static_cast<uint8_t>(NetProtocol::Verb::RALLY_INPUT)) {
            rally.OnGameMessage(clientID, verb, reader);
        } else {
            netTest.OnGameMessage(clientID, verb, reader);
        }
    }

   private:
    NetTestServer netTest;
    RallyServer rally;
};

}  // namespace Samples

int main(int argc, char** argv) {
    Samples::SamplesServer listener;
    ME::ServerApp app;
    return app.Run(argc, argv, listener);
}
