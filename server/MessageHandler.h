#pragma once

#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <websocketpp/common/connection_hdl.hpp>

namespace websocketpp {
template <typename config> class server;

namespace config {
struct asio;
}
} // namespace websocketpp

namespace signaling {

class ClientManager;
class RoomManager;
class Client;

using json = nlohmann::json;
using server_t = websocketpp::server<websocketpp::config::asio>;

class MessageHandler {
public:
  using SendCallback =
      std::function<void(websocketpp::connection_hdl, const json &)>;

  MessageHandler(std::shared_ptr<ClientManager> clientManager,
                 std::shared_ptr<RoomManager> roomManager,
                 SendCallback sendCallback);

  void handleMessage(websocketpp::connection_hdl hdl,
                     const std::string &payload);

  void handleOpen(websocketpp::connection_hdl hdl);
  void handleClose(websocketpp::connection_hdl hdl);

private:
  void handleJoin(std::shared_ptr<Client> client, const json &data);
  void handleLeave(std::shared_ptr<Client> client);
  void handleWebRTCMessage(std::shared_ptr<Client> client, const json &data);
  void handleChat(std::shared_ptr<Client> client, const json &data);

  void broadcastToRoom(const std::string &roomId,
                       websocketpp::connection_hdl exclude, const json &msg);
  void sendToUser(const std::string &roomId, const std::string &userId,
                  const json &msg);
  void sendToClient(websocketpp::connection_hdl hdl, const json &msg);

  std::shared_ptr<ClientManager> clientManager_;
  std::shared_ptr<RoomManager> roomManager_;
  SendCallback sendCallback_;
};

} // namespace signaling
