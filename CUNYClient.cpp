#include <BWAPI.h>
#include <BWAPI/Client.h>
#include "Source/CUNYAIModule.h"
#include "bwem.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>

using namespace BWAPI;

void reconnect()
{
    while (!BWAPIClient.connect())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{ 1000 });
    }
}

int main(int argc, const char* argv[])
{
    std::cout << "Connecting..." << std::endl;
    reconnect();
    while (true)
    {
        std::unique_ptr<CUNYAIModule> myBot;
        std::cout << "waiting to enter match" << std::endl;
        while (!Broodwar->isInGame()){
                 BWAPI::BWAPIClient.update();
            if (!BWAPI::BWAPIClient.isConnected())
            {
                std::cout << "Reconnecting..." << std::endl;;
                reconnect();
            }
        }
        // Enable some cheat flags
        Broodwar->enableFlag(Flag::UserInput);
        // Uncomment to enable complete map information
        //Broodwar->enableFlag(Flag::CompleteMapInformation);

        //if (Broodwar->isReplay())
        //{
        //    Broodwar << "The following players are in this replay:" << std::endl;;
        //    Playerset players = Broodwar->getPlayers();
        //    for (auto p : players)
        //    {
        //        if (!p->getUnits().empty() && !p->isNeutral())
        //            Broodwar << p->getName() << ", playing as " << p->getRace() << std::endl;
        //    }
        //}
        //else
        //{
        //}
        while (Broodwar->isInGame())
        {
            for (auto &e : Broodwar->getEvents())
            {
                switch (e.getType())
                {
                case EventType::MatchStart:
                    myBot = std::make_unique<CUNYAIModule>();
                    myBot->onStart();
                    break;
                case EventType::MatchEnd:
                    myBot->onEnd(e.isWinner());
                    ///exit; // prevents auto-restart issues.
                    break;
                case EventType::SendText:
                    myBot->onSendText(e.getText());
                    break;
                case EventType::ReceiveText:
                    myBot->onReceiveText(e.getPlayer(), e.getText());
                    break;
                case EventType::PlayerLeft:
                    myBot->onPlayerLeft(e.getPlayer());
                    break;
                case EventType::NukeDetect:
                    myBot->onNukeDetect(e.getPosition());
                    break;
                case EventType::UnitDiscover:
                    myBot->onUnitDiscover(e.getUnit());
                    break;
                case EventType::UnitEvade:
                    myBot->onUnitEvade(e.getUnit());
                    break;
                case EventType::UnitShow:
                    myBot->onUnitShow(e.getUnit());
                    break;
                case EventType::UnitHide:
                    myBot->onUnitHide(e.getUnit());
                    break;
                case EventType::UnitCreate:
                    myBot->onUnitCreate(e.getUnit());
                    break;
                case EventType::UnitDestroy:
                    myBot->onUnitDestroy(e.getUnit());
                    break;
                case EventType::UnitMorph:
                    myBot->onUnitMorph(e.getUnit());
                    break;
                case EventType::UnitRenegade:
                    myBot->onUnitRenegade(e.getUnit());
                    break;
                case EventType::SaveGame:
                    myBot->onSaveGame(e.getText().c_str());
                    break;
                case EventType::UnitComplete:
                    myBot->onUnitComplete(e.getUnit());
                    break;
                default:
                    myBot->onFrame(); // has catch in it for when it needs to run.
                    break;
                }
            }
            //if (BWAPI::Broodwar->getFrameCount() % BWAPI::Broodwar->getLatencyFrames() / 2 == BWAPI::Broodwar->getLatencyFrames() / 2 - 1)
            
            BWAPI::BWAPIClient.update();
            if (!BWAPI::BWAPIClient.isConnected())
            {
                std::cout << "Reconnecting..." << std::endl;
                reconnect();
            }
        }
        std::cout << "Game ended" << std::endl;
    }
    std::cout << "Press ENTER to continue..." << std::endl;
    std::cin.ignore();
    return 0;
}
