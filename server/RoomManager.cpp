#include "RoomManager.h"
#include "Client.h"
#include "Room.h"

namespace signaling {

std::shared_ptr<Room> RoomManager::getOrCreateRoom(const std::string &roomId) {
  auto it = rooms_.find(roomId);
  if (it != rooms_.end()) {
    return it->second;
  }
  auto room = std::make_shared<Room>(roomId);
  rooms_[roomId] = room;
  return room;
}


void RoomManager::addClientToRoom(const std::string &roomId,
                                  std::shared_ptr<Client> client) {
  auto room = getOrCreateRoom(roomId);
  room->addClient(client->getHandle());
  client->setRoomId(roomId);
}

bool RoomManager::removeClientFromRoom(const std::string &roomId,
                                       websocketpp::connection_hdl hdl) {
  auto roomIt = rooms_.find(roomId);
  if (roomIt != rooms_.end()) {
    roomIt->second->removeClient(hdl);
    if (roomIt->second->isEmpty()) {
      rooms_.erase(roomIt);
      return true;
    }
  }
  return false;
}

std::shared_ptr<Room> RoomManager::findRoom(const std::string &roomId) const {
  auto it = rooms_.find(roomId);
  if (it != rooms_.end()) {
    return it->second;
  }
  return nullptr;
}

} // namespace signaling
