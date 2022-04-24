#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cerrno>
#include <string.h>
#include <string>
#include <iostream>
#include <fstream>
#include <arpa/inet.h>
#include <signal.h>
#include <syslog.h>

inline bool exists_file (const std::string& name) {
    return ( access( name.c_str(), F_OK ) != -1 );
}
// Unix OS
#define MAX_EVENTS 32

int set_nonblock(int fd)
{
    int flags;
#if defined(O_NONBLOCK)
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    return ioctl(fd, FIOBIO, &flags);
#endif
} 

std::string get_file_name(char *buf)
{
    std::string s(buf);
    size_t pos = s.find_first_of(' ');
    if(pos == std::string::npos)
        return "";
    size_t pos2 = pos;
    for(uint i = pos + 1; i < s.length(); ++i)
    {
        if(s[i] == ' ')
        {
            pos2 = i;
            break;
        }
    }
    if(pos2 == pos)
        return s.substr(pos + 1);
    return s.substr(pos + 1, pos2 - pos - 1);
}

std::string get_res(char *buf, std::string& directory)
{
    std::string filename = directory + get_file_name(buf);
    //std::cout << filename << std::endl;
    bool nofile = !exists_file(filename);
    if(nofile)
    {
        return "HTTP/1.0 404 NOT FOUND\r\nContent-Type: text/html\r\n\r\n";
    }
    else
    {
        std::fstream f(filename, std::fstream::in);
        std::string res;
        while(!f.eof())
        {
            res += (char)f.get();
        }
        f.close();
        return "HTTP/1.0 200 OK\r\n"
        "Content-length: " + std::to_string(res.size()) + "\r\n"
        "Connection: close\r\n"
        "Content-Type: text/html\r\n"
        "\r\n" + res;
    }
}

static void skeleton_daemon()
{
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Catch, ignore and handle signals */
    //TODO: Implement a working signal handler */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    chdir("./");

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }

    /* Open the log file */
    openlog ("firstdaemon", LOG_PID, LOG_DAEMON);
}

int main(int argc, char **argv) {
    std::cout << getpid() << std::endl;
    skeleton_daemon();
    char c;
    std::string host = "127.0.0.1", directory = "./";
    int port = 12345;
    for(uint i = 1; i < argc; ++i)
    {
        if(!strcmp(argv[i], "-p"))
        {
            port = atoi(argv[i + 1]);
            ++i;
        }
        else if(!strcmp(argv[i], "-h"))
        {
            host = argv[i + 1];
            ++i;
        }
        else if(!strcmp(argv[i], "-d"))
        {
            directory = argv[i + 1];
            ++i;
        }
    }

    int masterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    struct sockaddr_in sockAddr;
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &(sockAddr.sin_addr));
    bind(masterSocket, (struct sockaddr *)(&sockAddr), sizeof(sockAddr));
    
    set_nonblock(masterSocket);

    listen(masterSocket, SOMAXCONN);

    int Epoll = epoll_create1(0);
    struct epoll_event event;
    event.data.fd = masterSocket;
    event.events = EPOLLIN;
    epoll_ctl(Epoll, EPOLL_CTL_ADD, masterSocket, &event); //регистрация события

    while(true)
    {
        struct epoll_event events[MAX_EVENTS];
        int N = epoll_wait(Epoll, events, MAX_EVENTS, -1); // -1 - вечное ожидания
        for(uint i = 0; i < N; ++i)
        {
            if(events[i].data.fd == masterSocket)
            {
                int slaveSocket = accept(masterSocket, 0, 0);
                set_nonblock(slaveSocket);
                struct epoll_event event;
                event.data.fd = slaveSocket;
                event.events = EPOLLIN;
                epoll_ctl(Epoll, EPOLL_CTL_ADD, slaveSocket, &event);
            }
            else
            {
                static char buf[1024];
                int recvRes = recv(events[i].data.fd, buf, 1024, MSG_NOSIGNAL);
                if(recvRes == 0 && errno != EAGAIN)
                {
                    shutdown(events[i].data.fd, SHUT_RDWR);
                    close(events[i].data.fd);
                }
                else if(recvRes > 0)
                {
                    //std::cout << "get smt\n";
                    std::string res = get_res(buf, directory);
                    //std::cout << res << std::endl;
                    send(events[i].data.fd, res.c_str(), res.size(), MSG_NOSIGNAL);
                }
            }
        }
    }
    return 0;
}