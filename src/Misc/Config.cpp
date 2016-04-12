/*
    Config.cpp - Configuration file functions

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2013, Nikita Zlobin
    Copyright 2014-2016, Will Godfrey & others

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is derivative of ZynAddSubFX original code, last modified January 2015
*/

#include <iostream>
#include <fenv.h>
#include <errno.h>
#include <cmath>
#include <string>
#include <argp.h>
#include <libgen.h>
#include <limits.h>

#if defined(__SSE__)
#include <xmmintrin.h>
#endif

#if defined(JACK_SESSION)
#include <jack/session.h>
#endif

using namespace std;

#include "Synth/BodyDisposal.h"
#include "Misc/XMLwrapper.h"
#include "Misc/SynthEngine.h"
#include "Misc/Config.h"
#include "MasterUI.h"

extern void mainRegisterAudioPort(SynthEngine *s, int portnum);

static char prog_doc[] =
    "Yoshimi " YOSHIMI_VERSION ", a derivative of ZynAddSubFX - "
    "Copyright 2002-2009 Nasca Octavian Paul and others, "
    "Copyright 2009-2011 Alan Calvert, "
    "Copyright 20012-2013 Jeremy Jongepier and others, "
    "Copyright 20014-2016 Will Godfrey and others";

const char* argp_program_version = "Yoshimi " YOSHIMI_VERSION;

static struct argp_option cmd_options[] = {
    {"alsa-audio",        'A',  "<device>",   1,  "use alsa audio output" },
    {"alsa-midi",         'a',  "<device>",   1,  "use alsa midi input" },
    {"define-root",       'D',  "<path>",     0,  "define path to new bank root"},
    {"buffersize",        'b',  "<size>",     0,  "set internal buffer size" },
    {"no-gui",            'i',  NULL,         0,  "disable gui"},
    {"gui",               'I',  NULL,         0,  "enable gui"},
    {"no-cmdline",        'c',  NULL,         0,  "disable command line interface"},
    {"cmdline",           'C',  NULL,         0,  "enable command line interface"},
    {"jack-audio",        'J',  "<server>",   1,  "use jack audio output" },
    {"jack-midi",         'j',  "<device>",   1,  "use jack midi input" },
    {"autostart-jack",    'k',  NULL,         0,  "auto start jack server" },
    {"auto-connect",      'K',  NULL,         0,  "auto connect jack audio" },
    {"load",              'l',  "<file>",     0,  "load .xmz file" },
    {"load-instrument",   'L',  "<file>",     0,  "load .xiz file" },
    {"name-tag",          'N',  "<tag>",      0,  "add tag to clientname" },
    {"samplerate",        'R',  "<rate>",     0,  "set alsa audio sample rate" },
    {"oscilsize",         'o',  "<size>",     0,  "set AddSynth oscilator size" },
    {"state",             'S',  "<file>",     1,  "load saved state, defaults to '$HOME/.config/yoshimi/yoshimi.state'" },
    #if defined(JACK_SESSION)
        {"jack-session-uuid", 'U',  "<uuid>",     0,  "jack session uuid" },
        {"jack-session-file", 'u',  "<file>",     0,  "load named jack session file" },
    #endif
    { 0, }
};


Config::Config(SynthEngine *_synth, int argc, char **argv) :
    restoreState(false),
    stateChanged(false),
    restoreJackSession(false),
    Samplerate(48000),
    Buffersize(256),
    Oscilsize(512),
    runSynth(true),
    showGui(true),
    showSplash(true),
    showCLI(true),
    VirKeybLayout(1),
    audioEngine(DEFAULT_AUDIO),
    midiEngine(DEFAULT_MIDI),
    audioDevice("default"),
    midiDevice("default"),
    jackServer("default"),
    jackMidiDevice("default"),
    startJack(false),
    connectJackaudio(false),
    alsaAudioDevice("default"),
    alsaMidiDevice("default"),
    GzipCompression(3),
    Interpolation(0),
    checksynthengines(1),
    xmlType(0),
    EnableProgChange(1), // default will be inverted
    toConsole(0),
    hideErrors(0),
    logXMLheaders(0),
    configChanged(false),
    rtprio(50),
    midi_bank_root(0), // 128 is used as 'disabled'
    midi_bank_C(32),
    midi_upper_voice_C(128),
    enable_part_on_voice_load(1),
    ignoreResetCCs(false),
    monitorCCin(false),
    single_row_panel(1),
    NumAvailableParts(NUM_MIDI_CHANNELS),
    currentPart(0),
    nrpnL(127),
    nrpnH(127),
    nrpnActive(false),

    deadObjects(NULL),
    nextHistoryIndex(numeric_limits<unsigned int>::max()),
    sigIntActive(0),
    ladi1IntActive(0),
    sse_level(0),
    programcommand(string("yoshimi")),
    synth(_synth),
    bRuntimeSetupCompleted(false)
{
    if(!synth->getIsLV2Plugin())
        fesetround(FE_TOWARDZERO); // Special thanks to Lars Luthman for conquering
                               // the heffalump. We need lrintf() to round
                               // toward zero.
    //^^^^^^^^^^^^^^^ This call is not needed aymore (at least for lv2 plugin)
    //as all calls to lrintf() are replaced with (int)truncf()
    //which befaves exactly the same when flag FE_TOWARDZERO is set

    cerr.precision(4);
    deadObjects = new BodyDisposal();
    bRuntimeSetupCompleted = Setup(argc, argv);
}


bool Config::Setup(int argc, char **argv)
{
    clearPresetsDirlist();
    AntiDenormals(true);

    if (!loadConfig())
        return false;

    if(synth->getIsLV2Plugin()) //skip further setup for lv2 plugin instance.
    {
        /*
         * These are needed here now, as for stand-alone they have
         * been moved to main to give the users the impression of
         * a faster startup, and reduce the likelyhood of thinking
         * they failed and trying to start again.
         */
        synth->installBanks(synth->getUniqueId());
        synth->loadHistory(synth->getUniqueId());
        return true;
    }
    switch (audioEngine)
    {
        case alsa_audio:
            audioDevice = string(alsaAudioDevice);
            break;

        case jack_audio:
            audioDevice = string(jackServer);
            break;
        case no_audio:
        default:
            audioDevice.clear();
            break;
    }
    if (!audioDevice.size())
        audioDevice = "default";
    switch (midiEngine)
    {
        case jack_midi:
            midiDevice = string(jackMidiDevice);
            break;

        case alsa_midi:
            midiDevice = string(alsaMidiDevice);
            break;

        case no_midi:
        default:
            midiDevice.clear();
            break;
    }
    if (!midiDevice.size())
        midiDevice = "";
    loadCmdArgs(argc, argv);
    Oscilsize = nearestPowerOf2(Oscilsize, MAX_AD_HARMONICS * 2, 16384);
    Buffersize = nearestPowerOf2(Buffersize, 16, 1024);
    //Log(asString(Oscilsize));
    //Log(asString(Buffersize));
    if (restoreState)
    {
        char * fp;
        if (! StateFile.size()) goto no_state0;
        else fp = new char [PATH_MAX];

        if (! realpath (StateFile.c_str(), fp)) goto no_state1;
        StateFile = fp;
        delete (fp);

        if (! isRegFile(StateFile))
        {
            no_state1: delete (fp);
            no_state0: Log("Invalid state file specified for restore " + StateFile);
            return false;
        }
        Log(StateFile);
        restoreSessionData(StateFile, true);
        /* This needs improving!
         * There is a single state file that contains both startup config
         * data that must be set early, and runtime data that must be set
         * after synth has been initialised.
         *
         * Currently we open it here and fetch just the startup data, then
         * reopen it in synth and fetch all the data (including the startup
         * again).
         *
         * This is further complicated because the same functions are
         * being used by jack session.
         */
    }
    return true;
}


Config::~Config()
{
    AntiDenormals(false);
}


void Config::flushLog(void)
{
    if (LogList.size())
    {
        while (LogList.size())
        {
            cerr << LogList.front() << endl;
            LogList.pop_front();
        }
    }
}


string Config::addParamHistory(string file, string extension, deque<HistoryListItem> &ParamsHistory)
{
    if (!file.empty())
    {
        unsigned int name_start = file.rfind("/");
        unsigned int name_end = file.rfind(extension);
        if (name_start != string::npos && name_end != string::npos
            && (name_start - 1) < name_end)
        {
            HistoryListItem item;
            item.name = file.substr(name_start + 1, name_end - name_start - 1);
            item.file = file;
            item.index = nextHistoryIndex--;
            itx = ParamsHistory.begin();
            for (unsigned int i = 0; i < ParamsHistory.size(); ++i, ++itx)
                if (ParamsHistory.at(i).sameFile(file))
                    ParamsHistory.erase(itx);
            ParamsHistory.insert(ParamsHistory.begin(), item);
            if (ParamsHistory.size() > MAX_HISTORY)
            {
                itx = ParamsHistory.end();
                ParamsHistory.erase(--itx);
            }
            return (CurrentXMZ = item.name);
        }
        else
            Log("Invalid param file proffered to history:" + file);
    }
    return string();
}


bool Config::showQuestionOrCmdWarning(string guiQuestion, string cmdLineWarning, bool bForceCmdLinePositive)
{
    bool bRet = false;
    if(showGui)
    {
        bRet = fl_choice("%s, ok?", "No", "Yes", "Cancel", guiQuestion.c_str());
    }
    else
    {
        bRet = bForceCmdLinePositive;//force positive answer if gui is not used (default behavior)
        cerr << endl << "----- WARNING! -----" << cmdLineWarning << endl << "----- ^^^^^^^^ -----" << endl;
    }
    return bRet;
}


string Config::testCCvalue(int cc)
{
    string result = "";
    switch (cc)
    {
        case 1:
            result = "mod wheel";
            break;

        case 10:
            result = "panning";
            break;

        case 11:
            result = "expression";
            break;

        case 38:
            result = "data lsb";
            break;

        case 71:
            result = "filter Q";
            break;

        case 74:
            result = "filter cutoff";
            break;

        case 75:
            result = "bandwidth";
            break;

        case 76:
            result = "FM amplitude";
            break;

        case 77:
            result = "resonance centre";
            break;

        case 78:
            result = "resonance bandwidth";
            break;

        default:
            result = masterCCtest(cc);
    }
    return result;
}


string Config::masterCCtest(int cc)
{
    string result = "";
    switch (cc)
    {
         case 6:
            result = "data msb";
            break;

        case 7:
            result = "volume";
            break;

        case 38:
            result = "data lsb";
            break;

        case 64:
            result = "sustain pedal";
            break;

        case 65:
            result = "portamento";
            break;

        case 96:
            result = "data increment";
            break;

        case 97:
            result = "data decrement";
            break;

        case 98:
            result = "NRPN lsb";
            break;

        case 99:
            result = "NRPN msb";
            break;

        case 120:
            result = "all sounds off";
            break;

        case 121:
            result = "reset all controllers";
            break;

        case 123:
            result = "all notes off";
            break;

        default:
        {
            if (cc < 128) // don't compare with 'disabled' state
            {
                if (cc == midi_bank_C)
                    result = "bank change";
                else if (cc == midi_bank_root)
                    result = "bank root change";
                else if (cc == midi_upper_voice_C)
                    result = "extended program change";
            }
        }
    }
    return result;
}


void Config::clearPresetsDirlist(void)
{
    for (int i = 0; i < MAX_PRESET_DIRS; ++i)
        presetsDirlist[i].clear();
}


bool Config::loadConfig(void)
{
    string cmd;
    int chk;
    string homedir = string(getenv("HOME"));
    if (homedir.empty() || !isDirectory(homedir))
        homedir = string("/tmp");
    ConfigDir = homedir + string("/.config/") + YOSHIMI;
    if (!isDirectory(ConfigDir))
    {
        cmd = string("mkdir -p ") + ConfigDir;
        if ((chk = system(cmd.c_str())) < 0)
        {
            Log("Create config directory " + ConfigDir + " failed, status " + asString(chk));
            return false;
        }
    }
    string yoshimi = "/"; // for some reason it doesn't
    yoshimi += YOSHIMI; // like these as one line here

    if (synth->getUniqueId() > 0)
        yoshimi += ("-" + asString(synth->getUniqueId()));
    string presetDir = ConfigDir + "/presets";
    if (!isDirectory(presetDir))
    {
        cmd = string("mkdir -p ") + presetDir;
        if ((chk = system(cmd.c_str())) < 0)
            Log("Create preset directory " + presetDir + " failed, status " + asString(chk));
    }
    ConfigFile = ConfigDir + yoshimi + string(".config");
    StateFile = ConfigDir + yoshimi + string(".state");
    string resConfigFile = ConfigFile;

    bool isok = true;
    if (!isRegFile(resConfigFile) && !isRegFile(ConfigFile))
    {
        Log("ConfigFile " + resConfigFile + " not found, will use default settings");
        defaultPresets();
        configChanged = true; // give the user the choice
    }
    else
    {
        XMLwrapper *xml = new XMLwrapper(synth);
        if (!xml)
            Log("loadConfig failed XMLwrapper allocation");
        else
        {
            if (!xml->loadXMLfile(resConfigFile))
            {
                if((synth->getUniqueId() > 0) && (!xml->loadXMLfile(ConfigFile)))
                {
                    Log("loadConfig loadXMLfile failed");
                    return false;
                }
            }
            isok = extractConfigData(xml);
            if (isok)
                Oscilsize = (int)truncf(powf(2.0f, ceil(log (Oscilsize - 1.0f) / logf(2.0))));
            delete xml;
        }
    }
    return isok;
}


void Config::defaultPresets(void)
{
    string presetdirs[]  = {
        "/usr/share/yoshimi/presets",
        "/usr/local/share/yoshimi/presets",
        "/usr/share/zynaddsubfx/presets",
        "/usr/local/share/zynaddsubfx/presets",
        string(getenv("HOME")) + "/.config/yoshimi/presets",
        localPath("/presets"),
        "end"
    };
    int i = 0;
    while (presetdirs[i] != "end")
    {
        if (isDirectory(presetdirs[i]))
        {
            Log(presetdirs[i], 2);
            presetsDirlist[i] = presetdirs[i];
        }
        ++ i;
    }
}


bool Config::extractConfigData(XMLwrapper *xml)
{
    if (!xml)
    {
        Log("extractConfigData on NULL");
        return false;
    }
    if (!xml->enterbranch("CONFIGURATION"))
    {
        Log("extractConfigData, no CONFIGURATION branch");
        return false;
    }
    GzipCompression = xml->getpar("gzip_compression", GzipCompression, 0, 9);
    showGui = xml->getpar("enable_gui", showGui, 0, 1);
    showSplash = xml->getpar("enable_splash", showSplash, 0, 1);
    showCLI = xml->getpar("enable_CLI", showCLI, 0, 1);
    single_row_panel = xml->getpar("single_row_panel", single_row_panel, 0, 1);
    toConsole = xml->getpar("reports_destination", toConsole, 0, 1);
    hideErrors = xml->getpar("hide_system_errors", hideErrors, 0, 1);
    logXMLheaders = xml->getpar("report_XMLheaders", logXMLheaders, 0, 1);
    VirKeybLayout = xml->getpar("virtual_keyboard_layout", VirKeybLayout, 0, 10);

    Samplerate = xml->getpar("sample_rate", Samplerate, 44100, 192000);
    Buffersize = xml->getpar("sound_buffer_size", Buffersize, 16, 1024);
    Oscilsize = xml->getpar("oscil_size", Oscilsize, MAX_AD_HARMONICS * 2, 16384);

    // get preset dirs
    int count = 0;
    bool found = false;
    for (int i = 0; i < MAX_PRESET_DIRS; ++i)
    {
        if (xml->enterbranch("PRESETSROOT", i))
        {
            string dir = xml->getparstr("presets_root");
            if (isDirectory(dir))
            {
                presetsDirlist[count++] = dir;
                found = true;
            }
            xml->exitbranch();
        }
    }
    if (!found)
    {
        defaultPresets();
        configChanged = true; // give the user the choice
    }

    Interpolation = xml->getpar("interpolation", Interpolation, 0, 1);

    // engines
    audioEngine = (audio_drivers)xml->getpar("audio_engine", audioEngine, no_audio, alsa_audio);
    midiEngine = (midi_drivers)xml->getpar("midi_engine", midiEngine, no_midi, alsa_midi);

    // alsa settings
    alsaAudioDevice = xml->getparstr("linux_alsa_audio_dev");
    alsaMidiDevice = xml->getparstr("linux_alsa_midi_dev");

    // jack settings
    jackServer = xml->getparstr("linux_jack_server");
    jackMidiDevice = xml->getparstr("linux_jack_midi_dev");

    // midi options
    midi_bank_root = xml->getpar("midi_bank_root", midi_bank_root, 0, 128);
    midi_bank_C = xml->getpar("midi_bank_C", midi_bank_C, 0, 128);
    midi_upper_voice_C = xml->getpar("midi_upper_voice_C", midi_upper_voice_C, 0, 128);
    EnableProgChange = 1 - xml->getpar("ignore_program_change", EnableProgChange, 0, 1); // inverted for Zyn compatibility
    enable_part_on_voice_load = xml->getpar("enable_part_on_voice_load", enable_part_on_voice_load, 0, 1);
    ignoreResetCCs = xml->getpar("ignore_reset_all_CCs",ignoreResetCCs,0, 1);

    //misc
    checksynthengines = xml->getpar("check_pad_synth", checksynthengines, 0, 1);

    xml->exitbranch(); // CONFIGURATION
    return true;
}


void Config::saveConfig(void)
{
    xmlType = XML_CONFIG;
    XMLwrapper *xmltree = new XMLwrapper(synth);
    if (!xmltree)
    {
        Log("saveConfig failed xmltree allocation");
        return;
    }
    addConfigXML(xmltree);
    unsigned int tmp = GzipCompression;
    GzipCompression = 0;

    string resConfigFile = ConfigFile;

    if (xmltree->saveXMLfile(resConfigFile))
        configChanged = false;
    else
        Log("Failed to save config to " + resConfigFile);
    GzipCompression = tmp;

    delete xmltree;
}


void Config::addConfigXML(XMLwrapper *xmltree)
{
    xmltree->beginbranch("CONFIGURATION");
    xmltree->addpar("gzip_compression", GzipCompression);
    xmltree->addpar("enable_gui", synth->getRuntime().showGui);
    xmltree->addpar("enable_splash", synth->getRuntime().showSplash);
    xmltree->addpar("enable_CLI", synth->getRuntime().showCLI);
    xmltree->addpar("single_row_panel", single_row_panel);
    xmltree->addpar("reports_destination", toConsole);
    xmltree->addpar("hide_system_errors", hideErrors);
    xmltree->addpar("report_XMLheaders", logXMLheaders);
    xmltree->addpar("virtual_keyboard_layout", VirKeybLayout);

    xmltree->addpar("sample_rate", Samplerate);
    xmltree->addpar("sound_buffer_size", Buffersize);
    xmltree->addpar("oscil_size", Oscilsize);

    for (int i = 0; i < MAX_PRESET_DIRS; ++i)
        if (presetsDirlist[i].size())
        {
            xmltree->beginbranch("PRESETSROOT",i);
            xmltree->addparstr("presets_root", presetsDirlist[i]);
            xmltree->endbranch();
        }

    xmltree->addpar("interpolation", Interpolation);

    xmltree->addpar("audio_engine", synth->getRuntime().audioEngine);
    xmltree->addpar("midi_engine", synth->getRuntime().midiEngine);

    xmltree->addparstr("linux_alsa_audio_dev", alsaAudioDevice);
    xmltree->addparstr("linux_alsa_midi_dev", alsaMidiDevice);

    xmltree->addparstr("linux_jack_server", jackServer);
    xmltree->addparstr("linux_jack_midi_dev", jackMidiDevice);

    xmltree->addpar("midi_bank_root", midi_bank_root);
    xmltree->addpar("midi_bank_C", midi_bank_C);
    xmltree->addpar("midi_upper_voice_C", midi_upper_voice_C);
    xmltree->addpar("ignore_program_change", (1 - EnableProgChange));
    xmltree->addpar("enable_part_on_voice_load", enable_part_on_voice_load);
    xmltree->addpar("ignore_reset_all_CCs",ignoreResetCCs);
    xmltree->addpar("check_pad_synth", checksynthengines);
    xmltree->endbranch(); // CONFIGURATION
}


void Config::saveSessionData(string savefile)
{
    string ext = ".state";
    if (savefile.rfind(ext) != (savefile.length() - 6))
        savefile += ext;
    synth->getRuntime().xmlType = XML_STATE;
    XMLwrapper *xmltree = new XMLwrapper(synth);
    if (!xmltree)
    {
        Log("saveSessionData failed xmltree allocation", 1);
        return;
    }
    addConfigXML(xmltree);
    addRuntimeXML(xmltree);
    synth->add2XML(xmltree);
    if (xmltree->saveXMLfile(savefile))
        Log("Session data saved to " + savefile);
    else
        Log("Failed to save session data to " + savefile, 1);
}


bool Config::restoreSessionData(string sessionfile, bool startup)
{
    XMLwrapper *xml = NULL;
    bool ok = false;
    if (sessionfile.size() && !isRegFile(sessionfile))
        sessionfile += ".state";
    if (!sessionfile.size() || !isRegFile(sessionfile))
    {
        Log("Session file " + sessionfile + " not available", 1);
        goto end_game;
    }
    if (!(xml = new XMLwrapper(synth)))
    {
        Log("Failed to init xmltree for restoreState", 1);
        goto end_game;
    }

    if (xml->loadXMLfile(sessionfile) < 0)
    {
        Log("Failed to load xml file " + sessionfile);
        goto end_game;
    }
    ok = extractConfigData(xml); // this needs improving
    if (!startup && ok)
        ok = extractRuntimeData(xml) && synth->getfromXML(xml);

end_game:
    if (xml)
        delete xml;
    return ok;
}


bool Config::extractRuntimeData(XMLwrapper *xml)
{
    if (!xml->enterbranch("RUNTIME"))
    {
        Log("Config extractRuntimeData, no RUNTIME branch", 1);
        return false;
    }
// need to put current root and bank here
    nameTag = xml->getparstr("name_tag");
    CurrentXMZ = xml->getparstr("current_xmz");
    xml->exitbranch();
    return true;
}


void Config::addRuntimeXML(XMLwrapper *xml)
{
    xml->beginbranch("RUNTIME");
// need to put current root and bank here
    xml->addparstr("name_tag", nameTag);
    xml->addparstr("current_xmz", CurrentXMZ);
    xml->endbranch();
}


void Config::Log(string msg, char tostderr)
{
    if ((tostderr & 2) && hideErrors)
        return;
    if (showGui && !(tostderr & 1) && toConsole)
        LogList.push_back(msg);
    else
        cerr << msg << endl;
}


#ifndef YOSHIMI_LV2_PLUGIN
void Config::StartupReport(MusicClient *musicClient)
{
    Log(string(argp_program_version));
    Log("Clientname: " + musicClient->midiClientName());
    string report = "Audio: ";
    switch (audioEngine)
    {
        case jack_audio:
            report += "jack";
            break;

        case alsa_audio:
            report += "alsa";
            break;

        default:
            report += "nada";
    }
    report += (" -> '" + audioDevice + "'");
    Log(report, 2);
    report = "Midi: ";
    switch (midiEngine)
    {
        case jack_midi:
            report += "jack";
            break;

        case alsa_midi:
            report += "alsa";
            break;

        default:
            report += "nada";
    }
    if (!midiDevice.size())
        midiDevice = "default";
    report += (" -> '" + midiDevice + "'");
    Log(report, 2);
    Log("Oscilsize: " + asString(synth->oscilsize), 2);
    Log("Samplerate: " + asString(synth->samplerate), 2);
    Log("Period size: " + asString(synth->buffersize), 2);
}
#endif


void Config::setRtprio(int prio)
{
    if (prio < rtprio)
        rtprio = prio;
}


// general thread start service
bool Config::startThread(pthread_t *pth, void *(*thread_fn)(void*), void *arg,
                         bool schedfifo, char priodec, bool create_detached)
{
    pthread_attr_t attr;
    int chk;
    bool outcome = false;
    bool retry = true;
    while (retry)
    {
        if (!(chk = pthread_attr_init(&attr)))
        {
            if(create_detached)
            {
               chk = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            }
            if(!chk)
            {
                if (schedfifo)
                {
                    if ((chk = pthread_attr_setschedpolicy(&attr, SCHED_FIFO)))
                    {
                        Log("Failed to set SCHED_FIFO policy in thread attribute "
                                    + string(strerror(errno))
                                    + " (" + asString(chk) + ")", 1);
                        schedfifo = false;
                        continue;
                    }
                    if ((chk = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)))
                    {
                        Log("Failed to set inherit scheduler thread attribute "
                                    + string(strerror(errno)) + " ("
                                    + asString(chk) + ")", 1);
                        schedfifo = false;
                        continue;
                    }
                    sched_param prio_params;
                    int prio = rtprio;
                    if (priodec)
                        prio -= priodec;
                    prio_params.sched_priority = (prio > 0) ? prio : 0;
                    if ((chk = pthread_attr_setschedparam(&attr, &prio_params)))
                    {
                        Log("Failed to set thread priority attribute ("
                                    + asString(chk) + ")  "
                                    + string(strerror(errno)), 1);
                        schedfifo = false;
                        continue;
                    }
                }
                if (!(chk = pthread_create(pth, &attr, thread_fn, arg)))
                {
                    outcome = true;
                    break;
                }
                else if (schedfifo)
                {
                    schedfifo = false;
                    continue;
                }
                outcome = false;
                break;
            }
            else
                Log("Failed to set thread detach state " + asString(chk), 1);
            pthread_attr_destroy(&attr);
        }
        else
            Log("Failed to initialise thread attributes " + asString(chk), 1);

        if (schedfifo)
        {
            Log("Failed to start thread (sched_fifo) " + asString(chk)
                + "  " + string(strerror(errno)), 1);
            schedfifo = false;
            continue;
        }
        Log("Failed to start thread (sched_other) " + asString(chk)
            + "  " + string(strerror(errno)), 1);
        outcome = false;
        break;
    }
    return outcome;
}


void Config::signalCheck(void)
{
    #if defined(JACK_SESSION)
        int jsev = __sync_fetch_and_add(&jsessionSave, 0);
        if (jsev != 0)
        {
            __sync_and_and_fetch(&jsessionSave, 0);
            switch (jsev)
            {
                case JackSessionSave:
                    saveJackSession();
                    break;

                case JackSessionSaveAndQuit:
                    saveJackSession();
                    runSynth = false;
                    break;

                case JackSessionSaveTemplate: // not implemented
                    break;

                default:
                    break;
            }
        }
    #endif

    if (ladi1IntActive)
    {
        __sync_and_and_fetch(&ladi1IntActive, 0);
        saveSessionData(StateFile);
    }

    if (sigIntActive)
        runSynth = false;
}


void Config::setInterruptActive(void)
{
    Log("Interrupt received", 1);
    __sync_or_and_fetch(&sigIntActive, 0xFF);
}


void Config::setLadi1Active(void)
{
    __sync_or_and_fetch(&ladi1IntActive, 0xFF);
}


bool Config::restoreJsession(void)
{
    #if defined(JACK_SESSION)
        return restoreSessionData(jackSessionFile, false);
    #else
        return false;
    #endif
}


void Config::setJackSessionSave(int event_type, string session_file)
{
    jackSessionFile = session_file;
    __sync_and_and_fetch(&jsessionSave, 0);
    __sync_or_and_fetch(&jsessionSave, event_type);
}


void Config::saveJackSession(void)
{
    saveSessionData(jackSessionFile);
    jackSessionFile.clear();
}


int Config::SSEcapability(void)
{
    #if !defined(__SSE__)
        return 0;
    #else
        #if defined(__x86_64__)
            int64_t edx;
            __asm__ __volatile__ (
                "mov %%rbx,%%rdi\n\t" // save PIC register
                "movl $1,%%eax\n\t"
                "cpuid\n\t"
                "mov %%rdi,%%rbx\n\t" // restore PIC register
                : "=d" (edx)
                : : "%rax", "%rcx", "%rdi"
            );
        #else
            int32_t edx;
            __asm__ __volatile__ (
                "movl %%ebx,%%edi\n\t" // save PIC register
                "movl $1,%%eax\n\t"
                "cpuid\n\t"
                "movl %%edi,%%ebx\n\t" // restore PIC register
                : "=d" (edx)
                : : "%eax", "%ecx", "%edi"
            );
        #endif
        return ((edx & 0x02000000 /*SSE*/) | (edx & 0x04000000 /*SSE2*/)) >> 25;
    #endif
}


void Config::AntiDenormals(bool set_daz_ftz)
{
    if(synth->getIsLV2Plugin())
    {
        return;// no need to set floating point rules for lv2 - host should control it.
    }
    #if defined(__SSE__)
        if (set_daz_ftz)
        {
            sse_level = SSEcapability();
            if (sse_level & 0x01)
                // SSE, turn on flush to zero (FTZ) and round towards zero (RZ)
                _mm_setcsr(_mm_getcsr() | 0x8000|0x6000);
            if (sse_level & 0x02)
                // SSE2, turn on denormals are zero (DAZ)
               _mm_setcsr(_mm_getcsr() | 0x0040);
        }
        else if (sse_level)
        {
            // Clear underflow and precision flags,
            // turn DAZ, FTZ off, restore round to nearest (RN)
            _mm_setcsr(_mm_getcsr() & ~(0x0030|0x8000|0x0040|0x6000));
        }
    #endif
}


/**
SSEcapability() and AntiDenormals() draw gratefully on the work of others,
including:

Jens M Andreasen, LAD, <http://lists.linuxaudio.org/pipermail/linux-audio-dev/2009-August/024707.html>).

LinuxSampler src/common/Features.cpp, licensed thus -

 *   LinuxSampler - modular, streaming capable sampler                     *
 *                                                                         *
 *   Copyright (C) 2003, 2004 by Benno Senoner and Christian Schoenebeck   *
 *   Copyright (C) 2005 - 2008 Christian Schoenebeck                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the Free Software           *
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston,                 *
 *   MA  02111-1307  USA                                                   *
**/


static error_t parse_cmds (int key, char *arg, struct argp_state *state)
{
    Config *settings = (Config*)state->input;
    if (arg && arg[0] == 0x3d)
        ++ arg;
    int num;

    switch (key)
    {
        case 'N': settings->nameTag = string(arg); break;

        case 'l': settings->paramsLoad = string(arg); break;

        case 'L': settings->instrumentLoad = string(arg); break;

        case 'A':
            settings->configChanged = true;
            settings->audioEngine = alsa_audio;
            if (arg)
                settings->audioDevice = string(arg);
            else
                settings->audioDevice = settings->alsaAudioDevice;
            break;

        case 'a':
            settings->configChanged = true;
            settings->midiEngine = alsa_midi;
            if (arg)
                settings->midiDevice = string(arg);
            else
                settings->midiDevice = string(settings->alsaMidiDevice);
            break;

        case 'b':
            settings->configChanged = true;
            settings->Buffersize = Config::string2int(string(arg));
            break;

        case 'D':
            if (arg)
                settings->rootDefine = string(arg);
            break;

        case 'c':
            settings->configChanged = true;
            settings->showCLI = false;
            break;

        case 'C':
            settings->configChanged = true;
            settings->showCLI = true;
            break;

        case 'i':
            settings->configChanged = true;
            settings->showGui = false;
            break;

        case 'I':
            settings->configChanged = true;
            settings->showGui = true;
            break;

        case 'J':
            settings->configChanged = true;
            settings->audioEngine = jack_audio;
            if (arg)
                settings->audioDevice = string(arg);
            break;

        case 'j':
            settings->configChanged = true;
            settings->midiEngine = jack_midi;
            if (arg)
                settings->midiDevice = string(arg);
            else
                settings->midiDevice = string(settings->jackMidiDevice);
            break;

        case 'k': settings->startJack = true; break;

        case 'K': settings->connectJackaudio = true; break;

        case 'o':
            settings->configChanged = true;
            settings->Oscilsize = Config::string2int(string(arg));
            break;

        case 'R':
            settings->configChanged = true;
            num = (Config::string2int(string(arg)) / 48 ) * 48;
            if (num < 48000 || num > 192000)
                num = 44100; // play safe
            settings->Samplerate = num;
            break;

        case 'S':
            settings->restoreState = true;
            if (arg)
                settings->StateFile = string(arg);
            break;

#if defined(JACK_SESSION)
        case 'u':
            if (arg)
                settings->jackSessionFile = string(arg);
            break;

        case 'U':
                if (arg)
                    settings->jackSessionUuid = string(arg);
        break;
#endif

        case ARGP_KEY_ARG:
        case ARGP_KEY_END:
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}


static struct argp cmd_argp = { cmd_options, parse_cmds, prog_doc };


void Config::loadCmdArgs(int argc, char **argv)
{
    argp_parse(&cmd_argp, argc, argv, 0, 0, this);
    if (jackSessionUuid.size() && jackSessionFile.size())
        restoreJackSession = true;
}


void GuiThreadMsg::processGuiMessages()
{
    GuiThreadMsg *msg = (GuiThreadMsg *)Fl::thread_message();
    if(msg)
    {
        switch(msg->type)
        {
        case GuiThreadMsg::NewSynthEngine:
        {
            SynthEngine *synth = ((SynthEngine *)msg->data);
            MasterUI *guiMaster = synth->getGuiMaster();
            if(!guiMaster)
                cerr << "Error starting Main UI!" << endl;
            else
                guiMaster->Init(guiMaster->getSynth()->getWindowTitle().c_str());
            break;
        }

        case GuiThreadMsg::UpdateMaster:
        {
            SynthEngine *synth = ((SynthEngine *)msg->data);
            MasterUI *guiMaster = synth->getGuiMaster(false);
            if(guiMaster)
                guiMaster->refresh_master_ui();
            break;
        }

        case GuiThreadMsg::UpdateConfig:
        {
            SynthEngine *synth = ((SynthEngine *)msg->data);
            MasterUI *guiMaster = synth->getGuiMaster(false);
            if(guiMaster)
                guiMaster->configui->update_config(msg->index);
            break;
        }

        case GuiThreadMsg::UpdatePaths:
        {
            SynthEngine *synth = ((SynthEngine *)msg->data);
            MasterUI *guiMaster = synth->getGuiMaster(false);
            if(guiMaster)
                guiMaster->updatepaths(msg->index);
            break;
        }

        case GuiThreadMsg::UpdatePanel:
        {
            SynthEngine *synth = ((SynthEngine *)msg->data);
            MasterUI *guiMaster = synth->getGuiMaster(false);
            if(guiMaster)
                guiMaster->updatepanel();
            break;
        }

        case GuiThreadMsg::UpdatePart:
        {
            SynthEngine *synth = ((SynthEngine *)msg->data);
            MasterUI *guiMaster = synth->getGuiMaster(false);
            if(guiMaster)
            {
                guiMaster->updatepart();
                guiMaster->updatepanel();
            }
            break;
        }

        case GuiThreadMsg::UpdatePanelItem:
            if(msg->index < NUM_MIDI_PARTS && msg->data)
            {
                SynthEngine *synth = ((SynthEngine *)msg->data);
                MasterUI *guiMaster = synth->getGuiMaster(false);
                if(guiMaster)
                {
                    guiMaster->updatelistitem(msg->index);
                    guiMaster->updatepart();
                }
            }
            break;

        case GuiThreadMsg::UpdatePartProgram:
            if(msg->index < NUM_MIDI_PARTS && msg->data)
            {
                SynthEngine *synth = ((SynthEngine *)msg->data);
                MasterUI *guiMaster = synth->getGuiMaster(false);
                if(guiMaster)
                {
                    guiMaster->updatelistitem(msg->index);
                    guiMaster->updatepartprogram(msg->index);
                }
            }
            break;

        case GuiThreadMsg::UpdateEffects:
            if(msg->data)
            {
                SynthEngine *synth = ((SynthEngine *)msg->data);
                MasterUI *guiMaster = synth->getGuiMaster(false);
                if(guiMaster)
                    guiMaster->updateeffects(msg->index);
            }
            break;

        case GuiThreadMsg::RegisterAudioPort:
            if(msg->data)
            {
                SynthEngine *synth = ((SynthEngine *)msg->data);
                mainRegisterAudioPort(synth, msg->index);
            }
            break;

        case GuiThreadMsg::UpdateBankRootDirs:
            if(msg->data)
            {
                SynthEngine *synth = ((SynthEngine *)msg->data);
                MasterUI *guiMaster = synth->getGuiMaster(false);
                if(guiMaster)
                    guiMaster->updateBankRootDirs();
            }
            break;

        case GuiThreadMsg::RescanForBanks:
            if(msg->data)
            {
                SynthEngine *synth = ((SynthEngine *)msg->data);
                MasterUI *guiMaster = synth->getGuiMaster(false);
                if(guiMaster && guiMaster->bankui)
                {
                    guiMaster->bankui->rescan_for_banks(false);
                }
            }
            break;

        case GuiThreadMsg::RefreshCurBank:
            if(msg->data)
            {
                SynthEngine *synth = ((SynthEngine *)msg->data);
                MasterUI *guiMaster = synth->getGuiMaster(false);
                if(guiMaster && guiMaster->bankui)
                {
                    if (msg->index == 1)
                    {
                        // special case for first synth statup
                        guiMaster->bankui->readbankcfg();
                        guiMaster->bankui->rescan_for_banks(false);
                    }
                    guiMaster->bankui->set_bank_slot();
                    guiMaster->bankui->refreshmainwindow();
                }
            }
            break;

        default:
            break;
        }
        delete msg;
    }
}
