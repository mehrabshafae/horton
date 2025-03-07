#pragma once

//=================================================================
#include <iostream>
#include <stdexcept>
#include <string>
// #include <cmath>
#include <memory> // C++11
#include <any> // C++17
#include <future> // C++11
#include <utility>
#include <functional>
#include <chrono> // C++11
#include <exception>
#include <sstream>
// #include <format>
#include <mutex>
#include <string_view> // C++17
#include <optional> // C++17
#include <vector>
// #include <atomic>
#include <thread>
#include <cstdlib>
#include <sys/resource.h>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <cstdarg>
#include <cstdio>
#include <unistd.h>
#include <limits.h>

#include <coroutine>

//--------------------------------------------
#include <omp.h>
// #include <curl/curl.h>
#include <json/json.h>
#include "asio.hpp"
#include <cxxopts.hpp>

//--------------------------------------------
using namespace std::string_literals;
// using namespace std::chrono_literals;

//=================================================================
#define highclock_start const auto start_time = std::chrono::high_resolution_clock::now()
#define highclock_end const auto end_time = std::chrono::high_resolution_clock::now(); const std::chrono::duration<double> elapsed = end_time - start_time; std::cout << std::fixed << std::setprecision(6); echo("high-clock end: " << elapsed.count() << "s")
//--------------------------------------------
#define print(x) std::cout << x << std::endl
//--------------------------------------------
#define CORE_ECHO_0() \
    critical([&]() { if(!SILENT) std::cout << std::endl; })
#define CORE_ECHO_1(x) \
    critical([&]() { if(!SILENT) std::cout << x << std::endl; })
#define CORE_ECHO_2(x, mode) \
    critical([&]() { if(!SILENT) { if(mode != std::string("flush")) { std::cout << x << std::endl; } else { std::cout << x << std::flush; } } })
#define CORE_ECHO_X(x, A, B, FUNC, ...) FUNC
#define echo(...) CORE_ECHO_X(, ##__VA_ARGS__, \
                          CORE_ECHO_2(__VA_ARGS__), \
                          CORE_ECHO_1(__VA_ARGS__), \
                          CORE_ECHO_0(__VA_ARGS__))
//--------------------------------------------
#define error_log(x) critical([&]() { std::cout.flush(); std::cerr << x << std::endl;  })
//--------------------------------------------
#define die(x)   \
   error_log(x); \
   std::exit(EXIT_FAILURE)

#define quit() std::exit(EXIT_SUCCESS)
//--------------------------------------------
#define panic(msg) throw std::runtime_error(msg)
//--------------------------------------------
#define fn __attribute__((always_inline)) inline void
//--------------------------------------------
#define sleep(x) std::this_thread::sleep_for(std::chrono::seconds(x))
//--------------------------------------------
// #define CROW_STATIC_DIRECTORY "dontlikeme/"
//=================================================================
static highclock_start;
//=================================================================
class FileManager final {
public:
    FileManager(const FileManager &) = delete;
    FileManager &operator=(const FileManager &) = delete;

    FileManager() noexcept : publicDir("./public_html") {}

    std::optional<std::filesystem::path> get_valid_path(const std::string& path) const noexcept {
        try {
            auto filePath = std::filesystem::absolute(path).lexically_normal();
            const auto baseDir = std::filesystem::absolute(publicDir).lexically_normal();

            if (!filePath.has_root_path() || !baseDir.has_root_path() ||
                filePath.string().rfind(baseDir.string(), 0) != 0 ||
                !is_valid_file(filePath)) {
                return std::nullopt;
            }

            return filePath;
        } catch (...) {
            return std::nullopt;
        }
    }

    std::optional<std::string> read_file_content(const std::filesystem::path& path) const noexcept {
        try{
            std::ifstream file(path, std::ios::binary);
            if (!file) {
                return std::nullopt;
            }

            std::string result((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            return file.bad() ? std::nullopt : std::optional<std::string>(std::move(result));
        } catch (...) {
            return std::nullopt;
        }
    }

    __attribute__((hot)) std::optional<std::string> readFile(const std::string& filePath) noexcept {
        try{
            const auto filePathOpt = get_valid_path(filePath);
            if (!filePathOpt) {
                return std::nullopt;
            }

            const auto& filePathStr = filePathOpt->string();
            auto it = cache.find(filePathStr);
            if (it != cache.end()) {
                return it->second;
            }

            auto result = read_file_content(*filePathOpt);
            if (result) {
                auto [it, inserted] = cache.emplace(filePathStr, std::move(*result));
                return it->second;
            }

            return std::nullopt;
        } catch (...) {
            return std::nullopt;
        }
    }

private:
    bool is_valid_file(const std::filesystem::path& path) const noexcept {
        std::error_code ec;
        const auto status = std::filesystem::status(path, ec);
        return !ec && std::filesystem::is_regular_file(status);
    }

    std::string publicDir;
    std::unordered_map<std::string, std::string> cache;
};
static FileManager fileManager;
//=================================================================
static fn critical(const std::function<void()> &&func) {
#pragma omp critical
    {
        func();
    }
}

class Fire final {
public:
    Fire() = default;
    Fire(const Fire &) = delete;
    Fire &operator=(const Fire &) = delete;

    __attribute__((hot)) void task(const std::function<void()> &&func) const noexcept {
#pragma omp task
        {
            try {
                func();
            } catch (const std::runtime_error &e) {
                error_log("[thread " << omp_get_thread_num() << "] Runtime error: " << e.what());
            }
            catch (const std::exception &e) {
                error_log("[thread " << omp_get_thread_num() << "] Caught a general exception: " << e.what());
            }
            catch (...) {
                error_log("[thread " << omp_get_thread_num() << "] An unknown error occurred.");
            }
        }
    }

    void taskgroup(std::function<void()> &&func) const noexcept {
#pragma omp parallel
        {
#pragma omp single
            {
#pragma omp taskgroup
                {
                    task(std::move(func));
                }
            }
        }
    }

    friend void critical(const std::function<void()> &&func);

    ~Fire() {
#pragma omp taskwait
    }
};
static const Fire fire;
//=================================================================
// we need to wait and set timeout for runtime app
std::string exec(std::string cmd) {
    std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "ERROR";
    char buffer[128];
    std::string result = "";
    while (!feof(pipe.get())) {
        if (fgets(buffer, 128, pipe.get()) != NULL)
            result += buffer;
    }
    return result;
}

std::string whereIs() {
    char buffer[PATH_MAX];
    ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length != -1) {
        buffer[length] = '\0';
        return std::string(buffer);
    }
    return "";
}

std::string getDirectory() {
    std::string executablePath = whereIs();
    std::filesystem::path path(executablePath);
    return path.parent_path().string();
}
/*
const char* multiline = R"(This is a
multiline string)";
C++ 23 -| maybe
*/
// class CurlClient final {
// public:
//     CurlClient(const CurlClient &) = delete;
//     CurlClient &operator=(const CurlClient &) = delete;

// private:
//     CurlClient() {
//     }

//     ~CurlClient() {
//     }
// };
// static CurlClient fetch;
//=================================================================
// static crow::SimpleApp server;
//=================================================================

//--------------------------------------------
int SILENT = 0;
//--------------------------------------------
cxxopts::Options argopt("Core", "The Marboris Core");
//--------------------------------------------

static void user_time() noexcept {
    echo();
    const auto end_time = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double> elapsed = end_time - start_time;

    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);

    const auto user_time = static_cast<double>(usage.ru_utime.tv_sec) +
                            static_cast<double>(usage.ru_utime.tv_usec) / 1e6;

    if (elapsed.count() <= 2.0) {
        std::cout << std::fixed << std::setprecision(6);
        echo("core time: " << user_time << "s user total, ", "flush");
    }
    echo("uptime: " << elapsed.count() << "s");
}
//--------------------------------------------
class InitClass final {
public:
    InitClass(const InitClass &) = delete;
    InitClass &operator=(const InitClass &) = delete;

    InitClass() noexcept {
        //=================================================================
        argopt.allow_unrecognised_options();
        argopt.add_options()
            ("h,help", "Show help")
            ("v,version", "Show version")
            ("silent,quiet,sick", "Enable silent mode");
        //=================================================================
        #ifdef STATIC_LIB_ENABLED
               omp_set_dynamic(0);
               omp_set_num_threads(omp_get_num_procs());
        #else
                omp_set_dynamic(1);
        #endif
                omp_set_nested(1);
        //=================================================================
        atexit(user_time);
        //=================================================================
        // CROW_ROUTE(server, "/home/<path>")
        // ([](const crow::request& req, crow::response& res, std::string_view path) {
        //     std::filesystem::path file_path = "./public_html/" + std::string(path);

        //     auto file_content = fileManager.readFile(file_path);
        //     if (!file_content) {
        //         file_path += "/index.html";
        //         file_content = fileManager.readFile(file_path);
        //         if (!file_content) {
        //             res.code = 404;
        //             res.write("File not found");
        //             res.end();
        //             return;
        //         }
        //     }

        //     if (file_path.extension() != ".html") {
        //         res.code = 403;
        //         res.write("Access forbidden: only HTML files are allowed.");
        //         res.end();
        //         return;
        //     }

        //     res.set_header("Content-Type", "text/html");

        //     res.write(*file_content);
        //     res.end();
        // });
        //=================================================================
    }
};
static const InitClass init_class;
//=================================================================
fn $head(int argc, char *argv[]) __attribute__((used));
fn $run() __attribute__((used));
fn $init() __attribute__((constructor, used));
fn $end() __attribute__((destructor, used));
//=================================================================
int main(const int argc, char *argv[]) {
    try {
        //=================================================================
        const auto result = argopt.parse(argc, argv);

        if (result.count("help")) {
            print(argopt.help());
            return 0;
        }

        if (result.count("version")) {
            print("Core Version: " << CORE_VERSION);
            return 0;
        }

        if (result.count("silent")) {
            SILENT = 1;
        }
        //=================================================================
        $head(argc, argv);
        fire.taskgroup($run);
        //=================================================================
    } catch (const std::runtime_error &e) {
        error_log("[main] Runtime error: " << e.what());
        return 1;
    }catch (const std::exception &e) {
        error_log("[main] Caught a general exception: " << e.what());
        return 1;
    }catch (...) {
        error_log("[main] An unknown error occurred.");
        return 1;
    }
    //=================================================================
    return 0;
}
