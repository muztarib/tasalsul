#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <string>
#include <sstream>
#include <atomic>
#include <mutex>
#include <memory>

#include "../include/tsdb.hpp"
#include "../include/server.hpp"


class TSDBServer {
public:
    TSDBServer(int port_, size_t head_cap_ = 1024, bool strict_ts_ = true) 
        : port(port_), strict_ts(strict_ts_), running(false), db(head_cap_, strict_ts_) {}

    void start() {
        running = true;
        int sd = socket(AF_INET, SOCK_STREAM, 0);
        if (sd < 0) {
            perror("socket");
            return;
        }
        int opt = 1;
        setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        if (bind(sd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind");
            close(sd);
            return;
        }
        if (listen(sd, SOMAXCONN) < 0) {
            perror("listen");
            close(sd);
            return;
        }
        // Accept loop
        while (running) {
            int client_fd = -1;
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            client_fd = accept(sd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) {
                if (errno == EINTR) continue;
                perror("accept");
                break;
            }
            // Handle client in a new thread
            std::thread t(&TSDBServer::handle_client, this, client_fd);
            t.detach();
        }
        close(sd);
    }

    void stop() {
        running = false;
    }

private:
    int port;
    bool strict_ts;
    std::atomic<bool> running;
    TSDB db;
    void handle_client(int client_fd) {
        FILE* fp = fdopen(client_fd, "r+");
        if (!fp) {
            close(client_fd);
            return;
        }
        char *line = nullptr;
        size_t len = 0;
        while (true) {
            line = nullptr; // reset for getline
            ssize_t read = getline(&line, &len, fp);
            if (read <= 0) break;
            std::string cmd(line, read);
            // Remove trailing newline
            if (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r')) {
                while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r')) cmd.pop_back();
            }
            std::string resp = process_command(cmd);
            if (!resp.empty()) {
                fprintf(fp, "%s\n", resp.c_str());
                fflush(fp);
            }
            if (cmd.rfind("QUIT", 0) == 0) break;
        }
        free(line);
        fclose(fp);
        close(client_fd);
    }

    std::string process_command(const std::string& line) {
        std::istringstream iss(line);
        std::string cmd;
        if (!(iss >> cmd)) return "";
        if (cmd == "PUT") {
            std::string metric; int64_t ts; double val;
            if (!(iss >> metric >> ts >> val)) return "ERR bad PUT syntax";
            if (!db.put(metric, ts, val)) return "ERR out_of_order";
            return "OK";
        } else if (cmd == "GET") {
            std::string metric; int64_t from, to;
            if (!(iss >> metric >> from >> to)) return "ERR bad GET syntax";
            auto pts = db.get(metric, from, to);
            std::ostringstream oss;
            for (const auto& p : pts) {
                oss << p.ts << " " << p.val << "\n";
            }
            return oss.str();
        } else if (cmd == "STATS") {
            std::string metric; if (!(iss >> metric)) return "ERR bad STATS syntax";
            return db.stats(metric);
        } else if (cmd == "QUIT") {
            return "BYE";
        } else if (cmd == "FLUSH") {
            // Not implemented in Phase 1; respond with OK for compatibility.
            //TODO: implement this in later phase
            return "OK";
        } else {
            return "ERR unknown_command";
        }
    }

    // (private members already declared above)
};

// Public API: start the server on a given port with strict_ts flag.
void start_server(int port, bool strict_ts) {
    TSDBServer server(port, 1024, strict_ts);
    server.start();
}
