#include "SignalingServer.h"
#include "ClientManager.h"
#include "MessageHandler.h"
#include "RoomManager.h"

#include <iostream>
#include <websocketpp/frame.hpp>

namespace signaling {

SignalingServer::SignalingServer() : running_(false) {}

SignalingServer::~SignalingServer() {
  if (running_) {
    stop();
  }
}

bool SignalingServer::initialize() {
  try {
    server_.init_asio();

    // Настраиваем обработчики
    server_.set_open_handler(
        [this](websocketpp::connection_hdl hdl) { onOpen(hdl); });
    server_.set_close_handler(
        [this](websocketpp::connection_hdl hdl) { onClose(hdl); });
    server_.set_message_handler(
        [this](websocketpp::connection_hdl hdl, server_t::message_ptr msg) {
          onMessage(hdl, msg);
        });

    // Создаём менеджеры
    clientManager_ = std::make_shared<ClientManager>();
    roomManager_ = std::make_shared<RoomManager>();

    // Создаём обработчик сообщений
    auto sendCallback = [this](websocketpp::connection_hdl hdl,
                               const nlohmann::json &msg) {
      sendToClient(hdl, msg);
    };
    messageHandler_ = std::make_shared<MessageHandler>(
        clientManager_, roomManager_, sendCallback);

    return true;
  } catch (const std::exception &e) {
    std::cerr << "Failed to initialize server: " << e.what() << std::endl;
    return false;
  }
}

bool SignalingServer::run(uint16_t port) {
  try {
    websocketpp::lib::error_code ec;
    server_.listen(port, ec);
    if (ec) {
      std::cerr << "Listen error: " << ec.message() << std::endl;
      return false;
    }

    std::cout << "Signaling server running on ws://0.0.0.0:" << port
              << std::endl;
    std::cout << "Features: room management, WebRTC routing" << std::endl;

    server_.start_accept();
    running_ = true;
    server_.run();

    return true;
  } catch (const std::exception &e) {
    std::cerr << "Server error: " << e.what() << std::endl;
    return false;
  }
}

void SignalingServer::stop() {
  running_ = false;
  server_.stop();
}

void SignalingServer::onOpen(websocketpp::connection_hdl hdl) {
  if (messageHandler_) {
    messageHandler_->handleOpen(hdl);
  }
}

void SignalingServer::onClose(websocketpp::connection_hdl hdl) {
  if (messageHandler_) {
    messageHandler_->handleClose(hdl);
  }
}

void SignalingServer::onMessage(websocketpp::connection_hdl hdl,
                                server_t::message_ptr msg) {
  if (msg->get_opcode() != websocketpp::frame::opcode::text) {
    return;
  }

  if (messageHandler_) {
    messageHandler_->handleMessage(hdl, msg->get_payload());
  }
}

void SignalingServer::sendToClient(websocketpp::connection_hdl hdl,
                                   const nlohmann::json &msg) {
  try {
    server_.send(hdl, msg.dump(), websocketpp::frame::opcode::text);
  } catch (const std::exception &e) {
    std::cerr << "Error sending message: " << e.what() << std::endl;
  }
}

} // namespace signaling
