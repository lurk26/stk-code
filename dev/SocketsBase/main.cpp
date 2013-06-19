#include <iostream>
#include <enet/enet.h>

#include "client_network_manager.hpp"
#include "server_network_manager.hpp"
#include "protocol_manager.hpp"
#include "protocols/get_public_address.hpp"
#include "http_functions.hpp"
#include "protocols/connect_to_server.hpp"
#include "protocols/hide_public_address.hpp"
#include "network_interface.hpp"

#include <stdio.h>
#include <string.h>

using namespace std;

int main()
{

    HTTP::init();
    
    std::string answer;
    cout << "host or client:";
    answer = "client";
    cin >> answer;
    if (answer == "client")
    {
        ClientNetworkManager::getInstance()->run();
        
        std::string nickname;
        std::string password;
        std::string hostNickname;
        
        //NetworkManager::getInstance()->run();
        
        NetworkManager::getInstance()->setLogin(nickname, password);
        bool connected = false;
        //clt.connect(0x0100007f, 7000); // addr in little endian, real address is 7f 00 00 01 (127.0.0.1)
        std::string buffer;
        while (1)
        {
            cin >> buffer;
            if (buffer == "cmd=connect")
            {
                cout << "Host Nickname=";
                std::cin >> hostNickname;
                connected = ClientNetworkManager::getInstance()->connectToHost(hostNickname);
                continue;
            }
            if (buffer == "cmd=login")
            {
                std::cout << "Username=";
                std::cin >> nickname;
                std::cout << "Password=";
                std::cin >> password;
                NetworkManager::getInstance()->setLogin(nickname, password);
            }
            if (buffer.size() == 0) { continue; }
            char buffer2[256];
            strcpy(buffer2, buffer.c_str());
            if (connected)
                ClientNetworkManager::getInstance()->sendPacket(buffer2);
        }
        
        
        enet_deinitialize();
    }
    else if (answer == "host")
    {
        //NetworkInterface::getInstance()->initNetwork(true);
        ServerNetworkManager::getInstance()->run();
        ServerNetworkManager::getInstance()->start();
        //GetPublicAddress
        
        while(1){}
    }
    return 0;
}