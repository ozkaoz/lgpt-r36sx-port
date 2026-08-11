#ifndef _RANDOM_NAMES_H_
#define _RANDOM_NAMES_H_

#include <string>
#include <cstdlib>
#include "time.h"

#ifndef MAX_NAME_LENGTH
#define MAX_NAME_LENGTH 25
#endif

/*
 * RANDOM_NAME_V1:
 * Load-time safe. Upstream kept global std::string/vector objects here; with
 * the port's custom CPP_MEMORY allocator those constructors run during
 * dlopen (before retro_init) and crash the core with a null deref
 * (SIG=11 f0). All static data is plain pointers (no constructors) and the
 * name is built lazily inside getRandomName().
 */
static const char *const adj[] = {
        "Red", "Swift", "Spoopy", "Gentle", "Fierce",
        "Sparkling", "Magic", "Curious", "Fast", "Hyped",
        "Rizzy", "Radiant", "Soothing", "Weird", "Haunted",
        "Buzzy", "Wild", "Joyful", "Serene", "Wobbly",
        "Lively", "Dopey", "Dynamic", "Graceful", "Cool",
        "Playful", "Dorky", "Singing", "Clever", "Quirky",
        "Dull", "Fine", "Gold", "Gray", "Huge",
        "Light", "Chocolate", "Ripe", "Sour", "Tart",
        "Tough", "Brisk", "Fresh", "Grand", "Lean",
        "Lush", "Mild", "Pale", "Rich", "Ripe"
    };
static const char *const vrb[] =
    {
        "Jump", "Explore", "Dance", "Whisper", "Roar",
        "Run", "Climb", "Song", "Sleep", "Laugh",
        "Banana", "Fly", "Reader", "Build", "Create",
        "Hiker", "Cook", "Brows", "Cod", "Dope",
        "Glow", "Gyatt", "Dream", "Play", "Wire",
        "Holla", "Question", "Rizz", "Plant", "Craft",
        "Pecker", "Roar", "Purr", "Surfer", "Drum",
        "Kick", "Flip", "Snap", "Clap", "Snap",
        "Bite", "Chew", "Hunt", "Singer", "Draw",
        "Sleeper", "Skier", "Smile", "Yell", "Zoomer"
    };

// Generate a random name made in the format of: "adjective+verb"
static std::string getRandomName() {
    static bool noSeed = true;
    if (noSeed) {
        srand((unsigned int)time(NULL));
        noSeed = false;
    }
    const unsigned int adjCount = sizeof(adj) / sizeof(adj[0]);
    const unsigned int vrbCount = sizeof(vrb) / sizeof(vrb[0]);
    for (int attempt = 0; attempt < 64; attempt++) {
        const char *a = adj[rand() % adjCount];
        const char *v = vrb[rand() % vrbCount];
        std::string name(a);
        name += v;
        if ((int)name.length() <= MAX_NAME_LENGTH) return name;
    }
    return "CoolCat";
}

#endif //_RANDOM_NAMES_H_
