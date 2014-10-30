/*
    main.cpp

    Copyright 2009-2011, Alan Calvert

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>

using namespace std;

#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/MusicClient.h"
#include "MasterUI.h"
#include "Synth/BodyDisposal.h"
#include <map>

std::map<SynthEngine *, MusicClient *> synthInstances;

static Config *firstRuntime = NULL;
static int globalArgc = 0;
static char **globalArgv = NULL;

//Andrew Deryabin: signal handling moved to main from Config Runtime
//It's only suitable for single instance app support
static struct sigaction yoshimiSigAction;

void yoshimiSigHandler(int sig)
{
    switch (sig)
    {
        case SIGINT:
        case SIGHUP:
        case SIGTERM:
        case SIGQUIT:
            firstRuntime->setInterruptActive();
            break;

        case SIGUSR1:
            firstRuntime->setLadi1Active();
            sigaction(SIGUSR1, &yoshimiSigAction, NULL);
            break;

        default:
            break;
    }
}

bool mainCreateNewInstance()
{
    MasterUI *guiMaster = NULL;
    MusicClient *musicClient = NULL;
    SynthEngine *synth = new SynthEngine(globalArgc, globalArgv);
    if (!synth->getRuntime().isRuntimeSetupCompleted())
        goto bail_out;

    if (!synth)
    {
        std::cerr << "Failed to allocate SynthEngine" << std::endl;
        goto bail_out;
    }

    if (!(musicClient = MusicClient::newMusicClient(synth)))
    {
        synth->getRuntime().Log("Failed to instantiate MusicClient");
        goto bail_out;
    }

    if (!(musicClient->Open()))
    {
        synth->getRuntime().Log("Failed to open MusicClient");
        goto bail_out;
    }

    if (!synth->Init(musicClient->getSamplerate(), musicClient->getBuffersize()))
    {
        synth->getRuntime().Log("SynthEngine init failed");
        goto bail_out;
    }

    if (!musicClient->Start())
    {
        synth->getRuntime().Log("Failed to start MusicIO");
        goto bail_out;
    }

    if (synth->getRuntime().showGui)
    {
        guiMaster = synth->getGuiMaster();
        if (guiMaster == NULL)
        {
            synth->getRuntime().Log("Failed to instantiate gui");
            goto bail_out;
        }
        guiMaster->Init(musicClient->midiClientName().c_str());
    }

    synth->getRuntime().StartupReport(musicClient);
    synth->Unmute();
    cout << "Yay! We're up and running :-)\n";

    synthInstances.insert(std::make_pair<SynthEngine *, MusicClient *>(synth, musicClient));
    return true;

bail_out:
    synth->getRuntime().runSynth = false;
    synth->getRuntime().Log("Yoshimi stages a strategic retreat :-(");
    if (musicClient)
    {
        musicClient->Close();
        delete musicClient;
    }
    if (synth)
    {
        synth->getRuntime().flushLog();
        delete synth;
    }

    return false;
}


int main(int argc, char *argv[])
{
    globalArgc = argc;
    globalArgv = argv;
    bool bExitSuccess = false;
    MasterUI *guiMaster = NULL;
    //MusicClient *musicClient = NULL;
    SynthEngine *synth = NULL;
    std::map<SynthEngine *, MusicClient *>::iterator it;

    if (!mainCreateNewInstance())
    {
        goto bail_out;
    }
    it = synthInstances.begin();
    synth = it->first;
    //musicClient = it->second;
    guiMaster = synth->getGuiMaster();

    firstRuntime = &synth->getRuntime();

    memset(&yoshimiSigAction, 0, sizeof(yoshimiSigAction));
    yoshimiSigAction.sa_handler = yoshimiSigHandler;
    if (sigaction(SIGUSR1, &yoshimiSigAction, NULL))
        synth->getRuntime().Log("Setting SIGUSR1 handler failed");
    if (sigaction(SIGINT, &yoshimiSigAction, NULL))
        synth->getRuntime().Log("Setting SIGINT handler failed");
    if (sigaction(SIGHUP, &yoshimiSigAction, NULL))
        synth->getRuntime().Log("Setting SIGHUP handler failed");
    if (sigaction(SIGTERM, &yoshimiSigAction, NULL))
        synth->getRuntime().Log("Setting SIGTERM handler failed");
    if (sigaction(SIGQUIT, &yoshimiSigAction, NULL))
        synth->getRuntime().Log("Setting SIGQUIT handler failed");

    while (synth->getRuntime().runSynth)
    {
        synth->getRuntime().signalCheck();
        synth->getRuntime().deadObjects->disposeBodies();
        if (synth->getRuntime().showGui)
        {
            for (int i = 0; !synth->getRuntime().LogList.empty() && i < 5; ++i)
            {
                guiMaster->Log(synth->getRuntime().LogList.front());
                synth->getRuntime().LogList.pop_front();
            }
            if (synth->getRuntime().runSynth)
                Fl::wait(0.033333);
        }
        else if (synth->getRuntime().runSynth)
            usleep(33333); // where all the action is ...
    }

    cout << "Goodbye - Play again soon?\n";
    bExitSuccess = true;

bail_out:    
    for(it = synthInstances.begin(); it != synthInstances.end(); ++it)
    {
        SynthEngine *_synth = it->first;
        MusicClient *_client = it->second;
        _synth->getRuntime().runSynth = false;
        if(!bExitSuccess)
        {
            _synth->getRuntime().Log("Yoshimi stages a strategic retreat :-(");
        }

        if (_client)
        {
            _client->Close();
            delete _client;
        }

        if (synth)
        {
            _synth->getRuntime().deadObjects->disposeBodies();
            _synth->getRuntime().flushLog();
            delete _synth;
        }
    }
    if(bExitSuccess)
        exit(EXIT_SUCCESS);
    else
        exit(EXIT_FAILURE);
}
