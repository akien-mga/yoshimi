#include <Misc/SynthEngine.h>
#include <Misc/MiscFuncs.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <string>
#include <iostream>
#include <algorithm>
#include <iterator>
#include <map>
#include <list>
#include <sstream>

using namespace std;

extern map<SynthEngine *, MusicClient *> synthInstances;

map<string, string > commands;

static bool cmdIfaceReady = false;
static int currentInstance = 0;

MiscFuncs miscFuncs;

void cmdIfaceSetup()
{
    cmdIfaceReady = true;
    commands.insert(pair<string, string >("instance [n]", "set current instance (default = 0)"));
    commands.insert(pair<string, string >("setup", "show dynamic settings"));
    commands.insert(pair<string, string >("save", "save dynamic settings"));
    commands.insert(pair<string, string >("path show", "display bank root paths"));
    commands.insert(pair<string, string >("path add [s]", "add bank root path"));
    commands.insert(pair<string, string >("path remove [n]", "remove bank root path ID"));
    commands.insert(pair<string, string >("list root (n)", "list banks in root ID or current"));
    commands.insert(pair<string, string >("list bank (n)", "list instruments in bank ID or current"));
    commands.insert(pair<string, string >("set root [n]", "set root path to ID"));
    commands.insert(pair<string, string >("set bank [n]", "set bank to ID"));
    commands.insert(pair<string, string >("set part [n1] [program,channel,destination] [n2]", "set part ID operations (instrument/MIDI channel/(dest: 1 main, 2 part, 3 both))"));
    commands.insert(pair<string, string >("set rootcc [n]", "set CC for root path changes (> 119 disables)"));
    commands.insert(pair<string, string >("set bankcc [n]", "set CC for bank changes (0, 32, other disables)"));
    commands.insert(pair<string, string >("set program [n]", "set MIDI program change (0 off, other on)"));
    commands.insert(pair<string, string >("set activate [n]", "set part activate (0 off, other on)"));
    commands.insert(pair<string, string >("set extend [n]", "set CC for extended program change (> 119 disables)"));
    commands.insert(pair<string, string >("set available [n]", "set available parts (16, 32, 64)"));
    commands.insert(pair<string, string >("set reports [n]", "set report destination (1 GUI console, other stderr)"));
    commands.insert(pair<string, string >("set volume [n]", "set master volume"));
    commands.insert(pair<string, string >("set shift [n]", "set master key shift semitones (64 no shift)"));
    commands.insert(pair<string, string >("stop", "all sound off"));
    commands.insert(pair<string, string >("exit", "clean up and close yoshimi"));
    commands.insert(pair<string, string >("help", "show all commands"));


}

bool cmdIfaceProcessCommand(string cmd, vector<string> args)
{
    //cout << "processing command " << cmd << endl;
    bool exit = false;
    map<SynthEngine *, MusicClient *>::iterator itSynth = synthInstances.begin();
    for(int i = 0; i < currentInstance; i++, ++itSynth);
    SynthEngine *synth = itSynth->first;
    Config &Runtime = synth->getRuntime();
    if(cmd == "instance")
    {
        if(args.size() == 1)
        {
            size_t iNum;
            istringstream(args [0]) >> iNum;
            if(iNum >= synthInstances.size())
            {
                cout << "instance number out of range!" << endl;
            }
            else
            {
                currentInstance = iNum;
            }

        }
    }
    else if(cmd == "stop")
    {
        synth->allStop();
    }
    else if(cmd == "exit")
    {
        exit = true;
        synth->getRuntime().runSynth = false;
    }
    else if(cmd == "setup")
    {
        synth->SetSystemValue(109, 255);
    }
    else if(cmd == "path")
    {
        if(args.size() >= 1)
        {
            if(args [0] == "show")
            {
                synth->SetSystemValue(110, 255);
            }
            else if((args [0] == "add") && (args.size() == 2))
            {
                int found = synth->getBankRef().addRootDir(args [1]);
                if (!found)
                {
                    Runtime.Log("Can't find path " + args [1]);
                }
                else
                    Runtime.Log("Added new root ID " + miscFuncs.asString(found) + " as " + args [1]);
            }
            else if((args [0] == "remove") && (args.size() == 2))
            {
                int rootID = miscFuncs.string2int(args [1]);
                string rootname = synth->getBankRef().getRootPath(rootID);
                if (rootname.empty())
                    Runtime.Log("Can't find path " + miscFuncs.asString(rootID));
                else
                {
                    synth->getBankRef().removeRoot(rootID);
                    Runtime.Log("Removed " + rootname);
                }
            }
        }

    }
    else if(cmd == "help")
    {
        map<string, string>::iterator it;
        int n = 1;
        for(it = commands.begin(); it != commands.end(); ++it, n++)
        {
            cout << n << ". " << it->first << ": " << it->second << endl;
        }
    }
    else
    {
        cout << "Unknown command: " << cmd << endl;
    }
    return exit;
}

void cmdIfaceCommandLoop()
{    
    if(!cmdIfaceReady)
    {
        cmdIfaceSetup();
    }
    char welcomeBuffer [512];
    char *cCmd = NULL;
    bool exit = false;
    while(!exit)
    {
        memset(welcomeBuffer, 0, sizeof(welcomeBuffer));
        sprintf(welcomeBuffer, "yoshimi [%d]> ", currentInstance);
        
        cCmd = readline(welcomeBuffer);
        if (cCmd) // this won't exist with a gui launch
        {
            string sCmd = cCmd;
            if (sCmd == "exit")
                exit = true;
            if(!sCmd.empty())
            {
                vector<string> vArgs;
                istringstream iss(sCmd);
                copy(istream_iterator<string>(iss), istream_iterator<string>(),     back_inserter(vArgs));
                sCmd = vArgs [0];
                vArgs.erase(vArgs.begin());
                exit = cmdIfaceProcessCommand(sCmd, vArgs);
                free(cCmd);
                cCmd = NULL;
            }
        }
    }
}
