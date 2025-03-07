#include "core.cxx"
// #include "bot.cc"

// fn sp() {
//     CROW_ROUTE(server, "/json")
//     ([]{
//         crow::json::wvalue x({{"message", "Hello, World!"}});
//         x["message2"] = "Hello, World.. Again!";
//         return x;
//     });

//     server.port(80)
//     .server_name("Core")
//     .multithreaded().run();
// }

fn $init() {
    argopt.add_options()
            ("t,test", "Run in test mode")
            ("d,debug", "Enable debugging")
            ("server", "Enable server mode")
            ("dev", "Enable development mode");
}

static int TEST = 0, DEBUG = 0, DEV = 0, SERVER = 0;

fn $head(const int argc, char *argv[]) {
    const auto result = argopt.parse(argc, argv);

    if (result.count("test")) {
        TEST = 1;
        print("Running in test mode.");
    }

    if (result.count("debug")) {
        DEBUG = 1;
        print("Debugging enabled.");
    }

    if (result.count("dev")) {
        DEV = 1;
        print("Development enabled.");
    }

    if (result.count("server")) {
        SERVER = 1;
        print("Server enabled.");
    }
}

using asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket) : socket_(std::move(socket)) {}

    void start() {
        read_request();
    }

private:
    void read_request() {
        auto self(shared_from_this());
        socket_.async_read_some(asio::buffer(buffer_),
            [this, self](asio::error_code ec, std::size_t bytes) {
                if (!ec) {
                    if (check_request(bytes)) {
                        send_response();
                    }
                }
            });
    }

    bool check_request(std::size_t bytes) {
        std::string_view request(buffer_.data(), bytes);
        return request.starts_with("GET / ");
    }

    void send_response() {
        constexpr std::string_view response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 2\r\n\r\n"
            "Hi";

        auto self(shared_from_this());
        asio::async_write(socket_, asio::buffer(response),
            [this, self](asio::error_code ec, std::size_t) {
                socket_.close();
            });
    }

    tcp::socket socket_;
    std::array<char, 1024> buffer_{};
};

class Server {
public:
    Server(asio::io_context& io, short port)
        : acceptor_(io, {tcp::v4(), port}) {
        accept();
    }

private:
    void accept() {
        acceptor_.async_accept(
            [this](asio::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket))->start();
                }
                accept();
            });
    }

    tcp::acceptor acceptor_;
};

fn $run() {
    // fire.task([]() -> void {
    //     if (!TEST) return;
    //     constexpr static auto chat_id = "5941066026"; // Replace with your chat ID
    //     constexpr static auto message = "Choose an option:";
    //     constexpr static auto parse_mode = "Markdown"; // or "HTML"
    //     Json::Value keyboard = telegram.createKeyboard(panel_main);
    //     telegram.sendMessage(chat_id, message, parse_mode, keyboard);
    // });
    fire.task([]() -> void {
        if (!SERVER) return;
        // sp();
        try {
            asio::io_context io;
                Server server(io, 8080);
                io.run();
            } catch (std::exception& e) {
                std::cerr << "Exception: " << e.what() << std::endl;
            }
    });
    fire.task([]() -> void {
        try {
         // auto response = fetch.get("https://example.com");
            std::string str = exec("cd "s + getDirectory() + "; ls");
            echo("res: " << "'" << str << "'");

    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

});
}

fn $end() { /* Cleanup */ }
