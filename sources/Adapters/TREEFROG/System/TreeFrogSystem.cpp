#include "TreeFrogSystem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Adapters/Unix/Process/UnixProcess.h"
#include "Adapters/Dummy/Midi/DummyMidi.h"
#include "Adapters/TREEFROG/Audio/TreeFrogAudio.h"
#include "Adapters/TREEFROG/GUI/TreeFrogGUIFactory.h"
#include "Adapters/TREEFROG/GUI/TreeFrogEventManager.h"
#include "Adapters/TREEFROG/Timer/TreeFrogTimer.h"
#include "Application/Application.h"
#include "Application/Model/Config.h"
#include "Services/Audio/Audio.h"
#include "Services/Midi/MidiService.h"
#include "System/FileSystem/FileSystem.h"
#include "System/Process/Process.h"
#include "System/Timer/Timer.h"
#include "System/Console/Trace.h"
#include "System/Console/Logger.h"

#ifndef LGPT_TREEFROG_ROOT
#define LGPT_TREEFROG_ROOT "/mnt/sdcard/lgpt"
#endif

static TreeFrogSystem *g_system = 0;
static TreeFrogTimerService *g_timer = 0;

static unsigned long now_msec_abs() {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (unsigned long)(tv.tv_sec * 1000UL + tv.tv_usec / 1000UL);
}

static void mkdir_if_missing(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) mkdir(path, 0755);
}

TreeFrogSystem::TreeFrogSystem() : clockBase_(now_msec_abs()) {}
TreeFrogSystem::~TreeFrogSystem() {}

bool TreeFrogSystem::Boot(const char *contentPath) {
    if (g_system) return true;

    mkdir_if_missing(LGPT_TREEFROG_ROOT);
    mkdir_if_missing(LGPT_TREEFROG_ROOT "/projects");
    mkdir_if_missing(LGPT_TREEFROG_ROOT "/samples");
    mkdir_if_missing(LGPT_TREEFROG_ROOT "/instruments");
    mkdir_if_missing(LGPT_TREEFROG_ROOT "/exports");

    g_system = new TreeFrogSystem();
    System::Install(g_system);
    FileSystem::Install(new UnixFileSystem());
    SysProcessFactory::Install(new UnixProcessFactory());

    Path::SetAlias("bin", LGPT_TREEFROG_ROOT);
    Path::SetAlias("root", LGPT_TREEFROG_ROOT);

    Path logPath("bin:lgpt.log");
    FileLogger *fileLogger = new FileLogger(logPath);
    if (fileLogger->Init().Succeeded()) Trace::GetInstance()->SetLogger(*fileLogger);

    I_GUIWindowFactory::Install(new TreeFrogGUIFactory());

    g_timer = new TreeFrogTimerService();
    TimerService::Install(g_timer);

    AudioSettings hint;
    hint.audioAPI_ = "libretro";
    hint.audioDevice_ = "picoarch";
    hint.bufferSize_ = 1024;
    hint.preBufferCount_ = 4;
    Audio::Install(new TreeFrogAudio(hint));

    MidiService::Install(new DummyMidi());

    TreeFrogEventManager::GetInstance()->Init();

    Config::GetInstance()->ProcessArguments(0, 0);

    return true;
}

void TreeFrogSystem::Shutdown() {
    if (Audio::GetInstance()) delete Audio::GetInstance();
    g_system = 0;
}

unsigned long TreeFrogSystem::GetClock() {
    return now_msec_abs() - clockBase_;
}

int TreeFrogSystem::GetBatteryLevel() { return -1; }
void *TreeFrogSystem::Malloc(unsigned size) { return malloc(size); }
void TreeFrogSystem::Free(void *ptr) { free(ptr); }
void TreeFrogSystem::Memset(void *addr, char value, int size) { memset(addr, value, size); }
void *TreeFrogSystem::Memcpy(void *s1, const void *s2, int n) { return memcpy(s1, s2, n); }
void TreeFrogSystem::PostQuitMessage() { TreeFrogSetQuitRequested(true); }
unsigned int TreeFrogSystem::GetMemoryUsage() { return 0; }
