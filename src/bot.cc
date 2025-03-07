#include "core.cxx"

class Telegram final {
public:
    Telegram(const Telegram &) = delete;
    Telegram &operator=(const Telegram &) = delete;

    explicit Telegram(const std::string &token) : apiUrl("https://api.telegram.org/bot" + token + "/") {}

    Json::Value createKeyboard(const std::vector<std::vector<std::string> > &rows) __attribute__((cold)) {
        Json::Value keyboard;
        keyboard["keyboard"] = Json::arrayValue;

        for (const auto &row: rows) {
            Json::Value jsonRow;
            for (const auto &button: row) {
                jsonRow.append(button);
            }
            keyboard["keyboard"].append(jsonRow);
        }

        keyboard["resize_keyboard"] = true;
        return keyboard;
    }

    void sendMessage(const std::string_view chatId, const std::string_view text, const std::string_view parseMode,
                     const Json::Value &keyboard) const __attribute__((cold)) {
        Json::Value data;
        data["chat_id"] = std::string(chatId);
        data["text"] = std::string(text);
        data["parse_mode"] = std::string(parseMode);
        data["reply_markup"] = keyboard;

        if (const auto response = fetch.post(apiUrl + "sendMessage", data)) {
            std::cout << "Response: " << *response << std::endl;
        } else {
            std::cout << "Failed to send message." << std::endl;
        }
    }

private:
    std::string apiUrl;
};

const std::string TOKEN = "7217319321:AAHDoguScESC6EbMeWKnruC0nkaf46mckdQ";
Telegram telegram(TOKEN);

using Keyboard = std::vector<std::vector<std::string> >;

Keyboard panel_main = {
    {"🆘", "⚖️"}
};
