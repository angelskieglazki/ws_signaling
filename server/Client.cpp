#include "Client.h"

namespace signaling {

Client::Client(websocketpp::connection_hdl hdl, std::string userId)
    : handle_(hdl), userId_(std::move(userId)) {}

websocketpp::connection_hdl Client::getHandle() const { return handle_; }

const std::string &Client::getUserId() const { return userId_; }

const std::string &Client::getRoomId() const { return roomId_; }

void Client::setRoomId(const std::string &roomId) { roomId_ = roomId; }

bool Client::isInRoom() const { return !roomId_.empty(); }

} // namespace signaling
