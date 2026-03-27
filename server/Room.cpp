#include "Room.h"

namespace signaling {

Room::Room(std::string roomId) : roomId_(std::move(roomId)) {}

const std::string &Room::getId() const { return roomId_; }

void Room::addClient(websocketpp::connection_hdl hdl) { clients_.insert(hdl); }

void Room::removeClient(websocketpp::connection_hdl hdl) {
  clients_.erase(hdl);
}

bool Room::isEmpty() const { return clients_.empty(); }

size_t Room::getClientCount() const { return clients_.size(); }

const std::set<websocketpp::connection_hdl,
               std::owner_less<websocketpp::connection_hdl>> &
Room::getClients() const {
  return clients_;
}

bool Room::hasClient(websocketpp::connection_hdl hdl) const {
  return clients_.find(hdl) != clients_.end();
}

} // namespace signaling
