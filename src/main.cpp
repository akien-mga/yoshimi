/*
    main.cpp

    Copyright 2009-2011, Alan Calvert

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 2 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <termios.h>

using namespace std;

#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/MusicClient.h"
#include "MasterUI.h"
#include "Synth/BodyDisposal.h"
#include <map>
#include <list>
#include <pthread.h>
#include <semaphore.h>
#include <cstdio>
#include <unistd.h>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Shared_Image.H>
#include <FL/Fl_PNG_Image.H>
#include "yoshimi-logo.h"

#include <readline/readline.h>
#include <readline/history.h>
#include <Misc/CmdInterface.h>

#include "Misc/NSM.H"
#include "Misc/NSM/Client.H"

// NSM instance
char *instance_name;
NSM_Client *nsm;

// cleanup on signaled exit
void sigterm_exit( int /* sig */ )
{
    // NSM_TODO: fix this
    //Runtime.runSynth = false;
    exit(0);
}

CmdInterface commandInt;

void mainRegisterAudioPort(SynthEngine *s, int portnum);

map<SynthEngine *, MusicClient *> synthInstances;
list<string> splashMessages;

static SynthEngine *firstSynth = NULL;
static Config *firstRuntime = NULL;
static int globalArgc = 0;
static char **globalArgv = NULL;
bool bShowGui = true;
bool bShowCmdLine = true;


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

void splashTimeout(void *splashWin)
{
    (static_cast<Fl_Window *>(splashWin))->hide();
}

static void *mainGuiThread(void *arg)
{
    Fl::lock();

    sem_post((sem_t *)arg);

    map<SynthEngine *, MusicClient *>::iterator it;
    fl_register_images();
#if (FL_MAJOR_VERSION == 1 && FL_MINOR_VERSION < 3)
    char *fname = tmpnam(NULL);
    if (fname)
    {
        FILE *f = fopen(fname, "wb");
        if (f)
        {
            fwrite(yoshimi_logo_png, sizeof(yoshimi_logo_png), 1, f);
            fclose(f);
        }
    }
    Fl_PNG_Image pix(fname);
    if (fname)
    unlink(fname);
#else
    Fl_PNG_Image pix("yoshimi_logo_png", yoshimi_logo_png, sizeof(yoshimi_logo_png));
#endif

    const int splashWidth = 411;
    const int splashHeight = 311;
    const int textHeight = 20;
    const int textBorder = 15;

    Fl_Window winSplash(splashWidth, splashHeight, "yoshimi splash screen");
    Fl_Box box(0, 0, splashWidth,splashHeight);
    //Fl_Pixmap pix(yoshimi_logo);

    box.image(pix);
    Fl_Box boxLb(textBorder, splashHeight - textHeight * 2, splashWidth - textBorder * 2, textHeight);
    boxLb.box(FL_NO_BOX);
    boxLb.align(FL_ALIGN_CENTER);
    boxLb.labelsize(textHeight);
    boxLb.labeltype(FL_NORMAL_LABEL);
    boxLb.labelfont(FL_HELVETICA | FL_ITALIC);
    string startup = YOSHIMI_VERSION;
    startup = "Yoshimi " + startup + " is starting";
    boxLb.label(startup.c_str());

    winSplash.set_modal();
    winSplash.clear_border();
    winSplash.border(false);

    if (bShowGui && firstRuntime->showSplash)
    {
        winSplash.position((Fl::w() - winSplash.w()) / 2, (Fl::h() - winSplash.h()) / 2);
        winSplash.show();
        Fl::add_timeout(2, splashTimeout, &winSplash);
    }

    do
    {
        if (bShowGui)
        {
            Fl::wait(0.033333);
            while (!splashMessages.empty())
            {
                boxLb.copy_label(splashMessages.front().c_str());
                splashMessages.pop_front();
            }
        }
        else
            usleep(33333);
    }
    while (firstSynth == NULL); // just wait

    GuiThreadMsg::sendMessage(firstSynth, GuiThreadMsg::NewSynthEngine, 0);

    while (firstSynth->getRuntime().runSynth)
    {
        if (firstSynth->getUniqueId() == 0)
        {
            firstSynth->getRuntime().signalCheck();
        }

        for (it = synthInstances.begin(); it != synthInstances.end(); ++it)
        {
            SynthEngine *_synth = it->first;
            MusicClient *_client = it->second;
            _synth->getRuntime().deadObjects->disposeBodies();
            if (!_synth->getRuntime().runSynth && _synth->getUniqueId() > 0)
            {
                int tmpID =  _synth->getUniqueId();
                if (_client)
                {
                    _client->Close();
                    delete _client;
                }

                if (_synth)
                {
                    _synth->saveHistory(tmpID);
                    _synth->saveBanks(tmpID);
                    _synth->getRuntime().deadObjects->disposeBodies();
                    _synth->getRuntime().flushLog();
                    delete _synth;
                }

                synthInstances.erase(it);
                cout << "\nStopped " << tmpID << "\n";
                break;
            }
            if (bShowGui)
            {
                for (int i = 0; !_synth->getRuntime().LogList.empty() && i < 5; ++i)
                {
                    MasterUI *guiMaster = _synth->getGuiMaster(false);
                    if (guiMaster)
                    {
                        guiMaster->Log(_synth->getRuntime().LogList.front());
                        _synth->getRuntime().LogList.pop_front();
                    }
                }
            }
        }

        // where all the action is ...
        if (bShowGui)
        {
            Fl::wait(0.033333);
            while (!splashMessages.empty())
            {
                boxLb.copy_label(splashMessages.front().c_str());
                splashMessages.pop_front();
            }
            GuiThreadMsg::processGuiMessages();
        }
        else
            usleep(33333);
    }
    firstSynth->saveHistory(0);
    firstSynth->saveBanks(0);
    return NULL;
}

bool mainCreateNewInstance(unsigned int forceId)
{
    MusicClient *musicClient = NULL;
    SynthEngine *synth = new SynthEngine(globalArgc, globalArgv, false, forceId);
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


    /* this is done in newMusicClient() now! ^^^^^
    if (!(musicClient->Open()))
    {
        synth->getRuntime().Log("Failed to open MusicClient");
        goto bail_out;
    }
    */

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
        synth->setWindowTitle(musicClient->midiClientName());
        if(firstSynth != NULL) //FLTK is not ready yet - send this message later for first synth
        {
            GuiThreadMsg::sendMessage(synth, GuiThreadMsg::NewSynthEngine, 0);
        }
    }

    synth->getRuntime().StartupReport(musicClient);

    if( nsm && nsm->is_active() )
    {
        if( nsm->project_filename )
        {
           struct stat st;
           if( stat( nsm->project_filename, &st ) == 0 )
           {
                    MasterUI *guiMaster = synth->getGuiMaster(false);
                    guiMaster->do_load_master_unconditional( true, nsm->project_filename );
           }
           // NSM_TODO: disable save here, NSM sessions can only
           // be saved from the NSM SM app
        }
    }

    synth->Unmute();
    if (synth->getUniqueId() == 0)
        cout << "\nYay! We're up and running :-)\n";
    else
    {
        cout << "\nStarted "<< synth->getUniqueId() << "\n";
        // following copied here for other instances
        synth->loadHistory(synth->getUniqueId());
        synth->installBanks(synth->getUniqueId());
    }
    synthInstances.insert(std::make_pair(synth, musicClient));
    //register jack ports for enabled parts
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        if(synth->part [npart]->Penabled)
        {
            mainRegisterAudioPort(synth, npart);
        }
    }
    return true;

bail_out:
    synth->getRuntime().runSynth = false;
    synth->getRuntime().Log("Bail: Yoshimi stages a strategic retreat :-(");
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

void *commandThread(void *arg)
{
    commandInt.cmdIfaceCommandLoop();
    return 0;
}

int main(int argc, char *argv[])
{
    char *nsm_url;

    struct termios  oldTerm;
    tcgetattr(0, &oldTerm);

    cout << "Yoshimi " << YOSHIMI_VERSION << " is starting" << endl; // guaranteed start message
    globalArgc = argc;
    globalArgv = argv;
    bool bExitSuccess = false;
    map<SynthEngine *, MusicClient *>::iterator it;
    bool guiStarted = false;
    pthread_t thr;
    pthread_attr_t attr;
    sem_t semGui;

    if (!mainCreateNewInstance(0))
    {
        goto bail_out;
    }

    it = synthInstances.begin();
    firstRuntime = &it->first->getRuntime();
    firstSynth = it->first;
    bShowGui = firstRuntime->showGui;
    bShowCmdLine = firstRuntime->showCLI;
    if (!(bShowGui | bShowCmdLine))
    {
        cout << "Can't disable both gui and command line!\nSet for command line.\n";
        firstRuntime->showCLI = true;
        bShowCmdLine = true;
        firstRuntime->configChanged = true;
    }

    if(sem_init(&semGui, 0, 0) == 0)
    {
        if (pthread_attr_init(&attr) == 0)
        {
            if (pthread_create(&thr, &attr, mainGuiThread, (void *)&semGui) == 0)
            {
                guiStarted = true;
            }
            pthread_attr_destroy(&attr);
        }
    }

    if (!guiStarted)
    {
        cout << "Yoshimi can't start main gui loop!" << endl;
        goto bail_out;
    }
    sem_wait(&semGui);
    sem_destroy(&semGui);


    // NSM_TODO : Is this signal handling OK given the sigaction() below?
    signal( SIGINT , sigterm_exit);
    signal( SIGTERM, sigterm_exit);

    nsm_url = getenv( "NSM_URL" );

    if( nsm_url )
    {
        nsm = new NSM_Client( firstSynth, firstSynth->getGuiMaster() );
        if( !nsm->init( nsm_url ) )
        {
                nsm->announce( "Yoshimi", "", argv[0] );
                /* Yoshimi really isn't structure to work without
                 * being connected to JACK and having synth object
                 * defined, so sleep for a bit and give up if not done */
                nsm->check( 500 );
                if( !nsm->is_active() )
                {
                        delete nsm;
                        nsm = 0;
                }
        }
        else
        {
                delete nsm;
                nsm = 0;
        }
    }


    memset(&yoshimiSigAction, 0, sizeof(yoshimiSigAction));
    yoshimiSigAction.sa_handler = yoshimiSigHandler;
    if (sigaction(SIGUSR1, &yoshimiSigAction, NULL))
        firstSynth->getRuntime().Log("Setting SIGUSR1 handler failed");
    if (sigaction(SIGINT, &yoshimiSigAction, NULL))
        firstSynth->getRuntime().Log("Setting SIGINT handler failed");
    if (sigaction(SIGHUP, &yoshimiSigAction, NULL))
        firstSynth->getRuntime().Log("Setting SIGHUP handler failed");
    if (sigaction(SIGTERM, &yoshimiSigAction, NULL))
        firstSynth->getRuntime().Log("Setting SIGTERM handler failed");
    if (sigaction(SIGQUIT, &yoshimiSigAction, NULL))
        firstSynth->getRuntime().Log("Setting SIGQUIT handler failed");
    // following moved here for faster first synth startup
    firstSynth->loadHistory(0);
    firstSynth->installBanks(0);
    GuiThreadMsg::sendMessage(firstSynth, GuiThreadMsg::RefreshCurBank, 1);

    //splashMessages.push_back("Startup complete!");

    //create command line processing thread
    pthread_t cmdThr;
    if(bShowCmdLine)
    {
        if (pthread_attr_init(&attr) == 0)
        {
            if (pthread_create(&cmdThr, &attr, commandThread, (void *)firstSynth) == 0)
            {

            }
            pthread_attr_destroy(&attr);
        }
    }

    void *ret;
    pthread_join(thr, &ret);    
    if(ret == (void *)1)
    {
        goto bail_out;
    }
    cout << "\nGoodbye - Play again soon?\n";
    bExitSuccess = true;

bail_out:
    if (bShowGui && !bExitSuccess) // this could be done better!
        sleep(2);
    for (it = synthInstances.begin(); it != synthInstances.end(); ++it)
    {
        SynthEngine *_synth = it->first;
        MusicClient *_client = it->second;
        _synth->getRuntime().runSynth = false;
        if(!bExitSuccess)
        {
            _synth->getRuntime().Log("Bail: Yoshimi stages a strategic retreat :-(");
        }

        if (_client)
        {
            _client->Close();
            delete _client;
        }

        if (_synth)
        {
            _synth->getRuntime().deadObjects->disposeBodies();
            _synth->getRuntime().flushLog();
            delete _synth;
        }
    }
    if(bShowCmdLine)
        tcsetattr(0, TCSANOW, &oldTerm);
    if (bExitSuccess)
        exit(EXIT_SUCCESS);
    else
        exit(EXIT_FAILURE);
}

void mainRegisterAudioPort(SynthEngine *s, int portnum)
{
    if (s && (portnum < NUM_MIDI_PARTS) && (portnum >= 0))
    {
        map<SynthEngine *, MusicClient *>::iterator it = synthInstances.find(s);
        if (it != synthInstances.end())
        {
            it->second->registerAudioPort(portnum);
        }
    }
}
