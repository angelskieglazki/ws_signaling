#include "ClientManager.h"
#include "Client.h"

namespace signaling {

ClientManager::ClientManager() : userCounter_(0) {}

std::shared_ptr<Client>
ClientManager::registerClient(websocketpp::connection_hdl hdl) {
  std::string userId = generateUserId();
  auto client = std::make_shared<Client>(hdl, userId);
  clients_[hdl] = client;
  return client;
}

std::shared_ptr<Client>
ClientManager::removeClient(websocketpp::connection_hdl hdl) {
  auto it = clients_.find(hdl);
  if (it != clients_.end()) {
    auto client = it->second;
    clients_.erase(it);
    return client;
  }
  return nullptr;
}

std::shared_ptr<Client>
ClientManager::findClient(websocketpp::connection_hdl hdl) const {
  auto it = clients_.find(hdl);
  if (it != clients_.end()) {
    return it->second;
  }
  return nullptr;
}

std::shared_ptr<Client>
ClientManager::findClientByUserId(const std::string &userId) const {
  for (const auto &[hdl, client] : clients_) {
    if (client->getUserId() == userId) {
      return client;
    }
  }
  return nullptr;
}

size_t ClientManager::getClientCount() const { return clients_.size(); }

void ClientManager::leaveRoom(websocketpp::connection_hdl hdl) {
  auto it = clients_.find(hdl);
  if (it != clients_.end()) {
    it->second->setRoomId("");
  }
}

void ClientManager::setClientRoom(websocketpp::connection_hdl hdl,
                                  const std::string &roomId) {
  auto it = clients_.find(hdl);
  if (it != clients_.end()) {
    it->second->setRoomId(roomId);
  }
}

std::string ClientManager::generateUserId() {
  return "user" + std::to_string(++userCounter_);
}

} // namespace signaling
