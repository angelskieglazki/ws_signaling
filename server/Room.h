#pragma once

#include <set>
#include <string>
#include <websocketpp/common/connection_hdl.hpp>

namespace signaling {

class Room {
  using Clients = std::set<websocketpp::connection_hdl,
                           std::owner_less<websocketpp::connection_hdl>>;

public:
  explicit Room(std::string roomId);

  const std::string &getId() const;

  void addClient(websocketpp::connection_hdl hdl);

  void removeClient(websocketpp::connection_hdl hdl);

  bool isEmpty() const;

  size_t getClientCount() const;

  const Clients &getClients() const;

  bool hasClient(websocketpp::connection_hdl hdl) const;

private:
  std::string roomId_;
  Clients clients_;
};

} // namespace signaling
