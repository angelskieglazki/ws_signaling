#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <iostream>
#include <set>
#include <string>
#include <memory>

typedef websocketpp::server<websocketpp::config::asio> server_t;
typedef server_t::connection_ptr connection_ptr;
typedef websocketpp::connection_hdl connection_hdl;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// Храним всех подключённых клиентов
std::set<connection_hdl, std::owner_less<connection_hdl>> clients;

void on_open(server_t* srv, connection_hdl hdl) {
    clients.insert(hdl);
    std::cout << "Клиент подключился. Всего: " << clients.size() << std::endl;

    // Можно отправить приветствие или список комнат, но пока просто
    srv->send(hdl, "{\"type\":\"welcome\",\"message\":\"Signaling ready\"}", websocketpp::frame::opcode::text);
}

void on_close(server_t* srv, connection_hdl hdl) {
    clients.erase(hdl);
    std::cout << "Клиент отключился. Всего: " << clients.size() << std::endl;
}

void on_message(server_t* srv, connection_hdl hdl, server_t::message_ptr msg) {
    if (msg->get_opcode() != websocketpp::frame::opcode::text) {
        return;  // пока только текст (json)
    }

    std::string payload = msg->get_payload();
    std::cout << "Получено: " << payload << std::endl;

    // Простейший broadcast: всем кроме отправителя
    for (auto& client : clients) {
        if (client.owner_before(hdl) || hdl.owner_before(client)) {  // !=
            srv->send(client, payload, msg->get_opcode());
        }
    }
}

int main() {
    server_t server;

    try {
        // Инициализация Asio
        server.init_asio();

        // Настраиваем handlers
        server.set_open_handler(bind(&on_open, &server, ::_1));
        server.set_close_handler(bind(&on_close, &server, ::_1));
        server.set_message_handler(bind(&on_message, &server, ::_1, ::_2));

        // Отключаем логи, если не нужны (можно включить для дебага)
        // server.clear_access_channels(websocketpp::log::alevel::all);
        // server.set_access_channels(websocketpp::log::alevel::connect | websocketpp::log::alevel::disconnect);

        // Слушаем на порту 9002 (стандартный для тестов)
        websocketpp::lib::error_code ec;
        server.listen(9002, ec);
        if (ec) {
            std::cerr << "Ошибка listen: " << ec.message() << std::endl;
            return 1;
        }

        std::cout << "Signaling-сервер запущен на ws://0.0.0.0:9002" << std::endl;
        std::cout << "(для внешних подключений используй ws://твой_белый_ip:9002)" << std::endl;

        server.start_accept();
        server.run();

    } catch (const std::exception& e) {
        std::cerr << "Исключение: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}