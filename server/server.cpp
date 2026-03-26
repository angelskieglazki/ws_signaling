#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>

typedef websocketpp::server<websocketpp::config::asio> server_t;
typedef server_t::connection_ptr connection_ptr;
typedef websocketpp::connection_hdl connection_hdl;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using json = nlohmann::json;

// Структура для хранения информации о клиенте
struct ClientInfo {
    std::string userId;
    std::string roomId;
};

// Хранилище клиентов и комнат
std::map<connection_hdl, ClientInfo, std::owner_less<connection_hdl>> clients;
std::map<std::string, std::set<connection_hdl, std::owner_less<connection_hdl>>> rooms;
int userCounter = 0;

// Генерация уникального ID
std::string generateUserId() {
    return "user" + std::to_string(++userCounter);
}

// Отправка сообщения конкретному клиенту
void sendToClient(server_t* srv, connection_hdl hdl, const json& msg) {
    try {
        srv->send(hdl, msg.dump(), websocketpp::frame::opcode::text);
    } catch (const std::exception& e) {
        std::cerr << "Error sending message: " << e.what() << std::endl;
    }
}

// Broadcast сообщения всем в комнате кроме отправителя
void broadcastToRoom(server_t* srv, const std::string& roomId, connection_hdl exclude, const json& msg) {
    auto it = rooms.find(roomId);
    if (it == rooms.end()) return;
    
    for (auto& client : it->second) {
        if (client.owner_before(exclude) || exclude.owner_before(client)) { // !=
            sendToClient(srv, client, msg);
        }
    }
}

// Отправка сообщения конкретному пользователю в комнате
void sendToUser(server_t* srv, const std::string& roomId, const std::string& userId, const json& msg) {
    auto it = rooms.find(roomId);
    if (it == rooms.end()) return;
    
    for (auto& client : it->second) {
        auto clientIt = clients.find(client);
        if (clientIt != clients.end() && clientIt->second.userId == userId) {
            sendToClient(srv, client, msg);
            return;
        }
    }
}

void on_open(server_t* srv, connection_hdl hdl) {
    std::string userId = generateUserId();
    clients[hdl] = {userId, ""};
    
    std::cout << "Client " << userId << " connected. Total: " << clients.size() << std::endl;
    
    // Отправляем приветствие с userId
    json welcome = {
        {"type", "welcome"},
        {"userId", userId},
        {"message", "Signaling ready"}
    };
    sendToClient(srv, hdl, welcome);
}

void on_close(server_t* srv, connection_hdl hdl) {
    auto it = clients.find(hdl);
    if (it != clients.end()) {
        std::string userId = it->second.userId;
        std::string roomId = it->second.roomId;
        
        // Удаляем из комнаты
        if (!roomId.empty()) {
            auto roomIt = rooms.find(roomId);
            if (roomIt != rooms.end()) {
                roomIt->second.erase(hdl);
                if (roomIt->second.empty()) {
                    rooms.erase(roomIt);
                } else {
                    // Уведомляем остальных о выходе
                    json leaveMsg = {
                        {"type", "user-left"},
                        {"userId", userId}
                    };
                    broadcastToRoom(srv, roomId, hdl, leaveMsg);
                }
            }
        }
        
        clients.erase(it);
        std::cout << "Client " << userId << " disconnected. Total: " << clients.size() << std::endl;
    }
}

void on_message(server_t* srv, connection_hdl hdl, server_t::message_ptr msg) {
    if (msg->get_opcode() != websocketpp::frame::opcode::text) {
        return;
    }

    std::string payload = msg->get_payload();
    std::cout << "Received: " << payload << std::endl;
    
    try {
        json data = json::parse(payload);
        std::string type = data.value("type", "");
        
        auto clientIt = clients.find(hdl);
        if (clientIt == clients.end()) return;
        
        std::string userId = clientIt->second.userId;
        std::string roomId = clientIt->second.roomId;
        
        if (type == "join") {
            // Выход из текущей комнаты
            if (!roomId.empty()) {
                auto roomIt = rooms.find(roomId);
                if (roomIt != rooms.end()) {
                    roomIt->second.erase(hdl);
                    json leaveMsg = {
                        {"type", "user-left"},
                        {"userId", userId}
                    };
                    broadcastToRoom(srv, roomId, hdl, leaveMsg);
                }
            }
            
            // Вход в новую комнату
            std::string newRoomId = data.value("room", "");
            if (newRoomId.empty()) {
                json error = {{"type", "error"}, {"message", "Room ID required"}};
                sendToClient(srv, hdl, error);
                return;
            }
            
            clientIt->second.roomId = newRoomId;
            rooms[newRoomId].insert(hdl);
            
            // Собираем список участников
            std::vector<std::string> participants;
            for (auto& conn : rooms[newRoomId]) {
                auto connIt = clients.find(conn);
                if (connIt != clients.end()) {
                    participants.push_back(connIt->second.userId);
                }
            }
            
            // Отправляем подтверждение
            json response = {
                {"type", "room-joined"},
                {"room", newRoomId},
                {"participants", participants}
            };
            sendToClient(srv, hdl, response);
            
            // Уведомляем других участников
            json joinMsg = {
                {"type", "user-joined"},
                {"userId", userId}
            };
            broadcastToRoom(srv, newRoomId, hdl, joinMsg);
            
            std::cout << "User " << userId << " joined room " << newRoomId 
                      << " (" << participants.size() << " participants)" << std::endl;
        }
        else if (type == "leave") {
            if (!roomId.empty()) {
                auto roomIt = rooms.find(roomId);
                if (roomIt != rooms.end()) {
                    roomIt->second.erase(hdl);
                    json leaveMsg = {
                        {"type", "user-left"},
                        {"userId", userId}
                    };
                    broadcastToRoom(srv, roomId, hdl, leaveMsg);
                }
                clientIt->second.roomId = "";
                
                json response = {{"type", "left"}};
                sendToClient(srv, hdl, response);
            }
        }
        else if (type == "offer" || type == "answer" || type == "ice-candidate") {
            // Маршрутизация WebRTC signaling сообщений
            std::string to = data.value("to", "");
            if (!to.empty() && !roomId.empty()) {
                // Добавляем from если не указан
                if (!data.contains("from")) {
                    data["from"] = userId;
                }
                sendToUser(srv, roomId, to, data);
            }
        }
        else if (type == "chat") {
            // Прочие сообщения - broadcast в комнату
            if (!roomId.empty()) {
                std::cout << "Broadcasting message: " << payload << std::endl;
                broadcastToRoom(srv, roomId, hdl, data);
            }
        }
        
    } catch (const json::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        json error = {{"type", "error"}, {"message", "Invalid JSON"}};
        sendToClient(srv, hdl, error);
    }
}

int main() {
    server_t server;

    try {
        server.init_asio();
        
        server.set_open_handler(bind(&on_open, &server, ::_1));
        server.set_close_handler(bind(&on_close, &server, ::_1));
        server.set_message_handler(bind(&on_message, &server, ::_1, ::_2));

        websocketpp::lib::error_code ec;
        server.listen(9002, ec);
        if (ec) {
            std::cerr << "Listen error: " << ec.message() << std::endl;
            return 1;
        }

        std::cout << "Signaling server running on ws://0.0.0.0:9002" << std::endl;
        std::cout << "Features: room management, WebRTC routing" << std::endl;

        server.start_accept();
        server.run();

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
