#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>

namespace signaling {

class ClientManager;
class RoomManager;
class MessageHandler;

using server_t = websocketpp::server<websocketpp::config::asio>;

class SignalingServer {
public:
  SignalingServer();
  ~SignalingServer();

  bool initialize();
  bool run(uint16_t port);
  void stop();

private:
  void onOpen(websocketpp::connection_hdl hdl);
  void onClose(websocketpp::connection_hdl hdl);
  void onMessage(websocketpp::connection_hdl hdl, server_t::message_ptr msg);
  void sendToClient(websocketpp::connection_hdl hdl, const nlohmann::json &msg);

  server_t server_;
  std::shared_ptr<ClientManager> clientManager_;
  std::shared_ptr<RoomManager> roomManager_;
  std::shared_ptr<MessageHandler> messageHandler_;
  bool running_;
};

} // namespace signaling
