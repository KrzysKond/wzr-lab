#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <map>
#include <chrono>

#include "net.h"
#include "vector3D.h"
#include "quaternion.h"

struct ObjectState
{
    Vector3 vPos;             // polozenie obiektu (œrodka geometrycznego obiektu) 
    quaternion qOrient;       // orientacja (polozenie katowe)
    Vector3 vV, vA;            // predkosc, przyspiesznie liniowe
    Vector3 vV_ang, vA_ang;   // predkosc i przyspieszenie liniowe
    float steering_angle;               // kat skretu kol w radianach (w lewo - dodatni)
};

struct Frame
{
    int iID;
    int type;
    ObjectState state;
    long sending_time;
    int iID_receiver;
};

// client info
struct ClientData
{
    unsigned long ip;
    int iID;
    std::chrono::steady_clock::time_point last_seen;
};

//std::map<unsigned long, ClientData> clients{};
std::map<int, ClientData> clients{};

constexpr std::chrono::milliseconds CLIENT_TIMEOUT{ 5000 };

int main()
{
    std::cout << "Starting server...\n";

    unicast_net* recv_net = new unicast_net(7573);
    unicast_net* send_net = new unicast_net(7574);

    while (true)
    {
        Frame frame;
        unsigned long sender_ip = 0;

        const auto now = std::chrono::steady_clock::now();

        // Remove inactive clients
        for (auto it = clients.begin(); it != clients.end();)
        {
            if (now - it->second.last_seen > std::chrono::milliseconds(CLIENT_TIMEOUT))
            {
                std::cout << "Removing inactive client: iID:" << it->second.iID << std::endl;
                it = clients.erase(it);
            }
            else
            {
                ++it;
            }
        }

        const int size = recv_net->reciv((char*)&frame, &sender_ip, sizeof(Frame));

        if (size > 0)
        {
            const int iID = frame.iID;
			std::cout << "Received frame from iID: " << iID << std::endl;

            // Register / update client
            //bool is_known_client = clients.find(sender_ip) != clients.end();
            bool is_known_client = clients.find(iID) != clients.end();
            if (is_known_client)
            {
                clients[iID].last_seen = now;
            }
            else
            {
                std::cout << "New client: IP: " << sender_ip << ", iID: " << iID << std::endl;

                ClientData ci{};
                ci.ip = sender_ip;
				ci.iID = iID;
                ci.last_seen = now;

                clients[iID] = ci;
            }

            // Forward frame to all clients
            for (auto& c : clients)
            {
                send_net->send((char*)&frame, c.second.ip, sizeof(Frame));
            }
        }
    }

    return 0;
}