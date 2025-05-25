#ifndef MULTICAST_ANNOUNCER_H
#define MULTICAST_ANNOUNCER_H

#include <string>
#include <thread>
#include <atomic>

class MulticastAnnouncer {
public:
    MulticastAnnouncer(int port, const std::string& multicast_addr = "239.255.0.1", int multicast_port = 50000, int interval_sec = 5);
    ~MulticastAnnouncer();
    void start();
    void stop();
private:
    void run();
    std::string getLocalIp();
    int port_;
    std::string multicast_addr_;
    int multicast_port_;
    int interval_sec_;
    std::thread thread_;
    std::atomic<bool> running_;
};

#endif // MULTICAST_ANNOUNCER_H 