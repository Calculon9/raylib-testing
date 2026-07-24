/*******************************************************************************************
 *
 *   raylib game template
 *
 *   <Game title>
 *   <Game description>
 *
 *   This game has been created using raylib (www.raylib.com)
 *   raylib is licensed under an unmodified zlib/libpng license (View raylib.h for details)
 *
 *   Copyright (c) 2021 Ramon Santamaria (@raysan5)
 *
 ********************************************************************************************/

#include "raylib.h"
#include "screens.h"       // NOTE: Declares global (extern) variables and screens functions
#include "system/screen.h" // NOTE: Declares global (extern) variables and screens functions
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/world_system.h"
#include "system/universe_system.h"
#include "system/viewport_system.h"
#include "system/rpanel_system.h"
#include "system/debug_overlay_system.h"
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

//----------------------------------------------------------------------------------
// Shared Variables Definition (global)
// NOTE: Those variables are shared between modules through screens.h
//----------------------------------------------------------------------------------
GameScreen currentScreen = LOGO;
FrameCounter frame_counter = {0};
size_t memory_allocated = 0.0f;
Font font = {0};
Music music = {0};
Sound fxCoin = {0};
const int screenWidth = 1600;
const int screenHeight = 900;

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
//----------------------------------------------------------------------------------
// static const int screenWidth = 1920;
// static const int screenHeight = 1080;
int screen_resolution_scalar = 0; // <= 0 uses dynamic world scaling from target logical height
static float viewport_target_game_logical_height = 18.0f;
static int viewport_ui_pixels_per_unit_override = 0;

// Required variables to manage screen transitions (fade-in, fade-out)
static float transAlpha = 0.0f;
static bool onTransition = false;
static bool transFadeOut = false;
static int transFromScreen = -1;
static GameScreen transToScreen = UNKNOWN;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void ChangeToScreen(int screen);     // Change to screen, no transition effect
static void TransitionToScreen(int screen); // Request transition to next screen
static void UpdateTransition(void);         // Update transition effect
static void DrawTransition(void);           // Draw transition effect (full-screen rectangle)
static void UpdateDrawFrame(void);          // Update and draw one frame

//----------------------------------------------------------------------------------
// Program main entry point
//----------------------------------------------------------------------------------
int main(void)
{
    // Initialization
    //---------------------------------------------------------
    InitWindow(screenWidth, screenHeight, "raylib game template");

    InitAudioDevice(); // Initialize audio device

    // Load global data (assets that must be available in all screens, i.e. font)
    font = LoadFont("resources/mecha.png");
    // music = LoadMusicStream("resources/ambient.ogg"); // TODO: Load music
    fxCoin = LoadSound("resources/coin.wav");

    SetMusicVolume(music, 1.0f);
    PlayMusicStream(music);

    // Setup and init first screen
    currentScreen = GAMEPLAY;
    InitGameplayScreen();

    frame_counter = InitFrameCounter();
    
    // Seed the random number generator using the current time
    srand(time(NULL));

    // Setup and init first screen
    // currentScreen = LOGO;
    // InitLogoScreen();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 60, 1);
#else
    SetTargetFPS(60); // Set our game to run at 60 frames-per-second
    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        UpdateDrawFrame();
    }
#endif

    // De-Initialization
    //--------------------------------------------------------------------------------------
    // Unload current screen data before closing
    switch (currentScreen)
    {
    case LOGO:
        UnloadLogoScreen();
        break;
    case TITLE:
        UnloadTitleScreen();
        break;
    case OPTIONS:
        UnloadOptionsScreen();
        break;
    case GAMEPLAY:
        UnloadGameplayScreen();
        break;
    case ENDING:
        UnloadEndingScreen();
        break;
    default:
        break;
    }

    // Unload global data loaded
    UnloadFont(font);
    UnloadMusicStream(music);
    UnloadSound(fxCoin);

    CloseAudioDevice(); // Close audio context

    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
// Change to next screen, no transition
static void ChangeToScreen(int screen)
{
    // Unload current screen
    switch (currentScreen)
    {
    case LOGO:
        UnloadLogoScreen();
        break;
    case TITLE:
        UnloadTitleScreen();
        break;
    // case OPTIONS: UnloadOptionsScreen(); break;
    case GAMEPLAY:
        UnloadGameplayScreen();
        break;
    // case ENDING: UnloadEndingScreen(); break;
    default:
        break;
    }

    // Init next screen
    switch (screen)
    {
    case LOGO:
        InitLogoScreen();
        break;
    case TITLE:
        InitTitleScreen();
        break;
    // case OPTIONS: InitOptionsScreen(); break;
    case GAMEPLAY:
        InitViewportLayout(screenWidth, screenHeight, screen_resolution_scalar);
        // InitGameWorld();
        break;
    // case ENDING: InitEndingScreen(); break;
    default:
        break;
    }

    currentScreen = screen;
}

// Request transition to next screen
static void TransitionToScreen(int screen)
{
    onTransition = true;
    transFadeOut = false;
    transFromScreen = currentScreen;
    transToScreen = screen;
    transAlpha = 0.0f;
}

// Update transition effect (fade-in, fade-out)
static void UpdateTransition(void)
{
    if (!transFadeOut)
    {
        transAlpha += 0.05f;

        // NOTE: Due to float internal representation, condition jumps on 1.0f instead of 1.05f
        // For that reason we compare against 1.01f, to avoid last frame loading stop
        if (transAlpha > 1.01f)
        {
            transAlpha = 1.0f;

            // Unload current screen
            switch (transFromScreen)
            {
            case LOGO:
                UnloadLogoScreen();
                break;
            case TITLE:
                UnloadTitleScreen();
                break;
            // case OPTIONS: UnloadOptionsScreen(); break;
            case GAMEPLAY:
                UnloadGameplayScreen();
                break;
            // case ENDING: UnloadEndingScreen(); break;
            default:
                break;
            }

            // Load next screen
            switch (transToScreen)
            {
            case LOGO:
                InitLogoScreen();
                break;
            case TITLE:
                InitTitleScreen();
                break;
            // case OPTIONS: InitOptionsScreen(); break;
            case GAMEPLAY:
                InitGameplayScreen();
                break;
            // case ENDING: InitEndingScreen(); break;
            default:
                break;
            }

            currentScreen = transToScreen;

            // Activate fade out effect to next loaded screen
            transFadeOut = true;
        }
    }
    else // Transition fade out logic
    {
        transAlpha -= 0.02f;

        if (transAlpha < -0.01f)
        {
            transAlpha = 0.0f;
            transFadeOut = false;
            onTransition = false;
            transFromScreen = -1;
            transToScreen = UNKNOWN;
        }
    }
}

// Draw transition effect (full-screen rectangle)
static void DrawTransition(void)
{
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, transAlpha));
}

// Update and draw game frame
static void UpdateDrawFrame(void)
{
    UpdateFrameCounter(&frame_counter);
    memory_allocated = GetCurrentMemoryAllocated();
    // Update
    //----------------------------------------------------------------------------------
    // UpdateMusicStream(music);       // NOTE: Music keeps playing between screens

    if (!onTransition)
    {
        switch (currentScreen)
        {
        case LOGO:
        {
            UpdateLogoScreen();

            if (FinishLogoScreen())
                TransitionToScreen(TITLE);
        }
        break;
        case TITLE:
        {
            UpdateTitleScreen();

            if (FinishTitleScreen() == 2)
                TransitionToScreen(GAMEPLAY);
        }
        break;
        case GAMEPLAY:
        {
            UpdateGameplayScreen();

            // if (FinishGameplayScreen() == 1) TransitionToScreen(ENDING);
            // else if (FinishGameplayScreen() == 2) TransitionToScreen(TITLE);
        }
        break;
        default:
            break;
        }
    }
    else
        UpdateTransition(); // Update transition (fade-in, fade-out)
    //----------------------------------------------------------------------------------

    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(RAYWHITE);

    switch (currentScreen)
    {
    case LOGO:
        DrawLogoScreen();
        break;
    case TITLE:
        DrawTitleScreen();
        break;
    // case OPTIONS: DrawOptionsScreen(); break;
    case GAMEPLAY:
        DrawGameplayScreen();
        break;
    // case ENDING: DrawEndingScreen(); break;
    default:
        break;
    }

    // Draw full screen rectangle in front of everything
    if (onTransition)
        DrawTransition();

    // DrawFPS(10, 10);

    EndDrawing();
    //----------------------------------------------------------------------------------
}

void InitGameplayScreen(void)
{
    SetViewportPanelRatios(0.20f, 0.20f);
    SetViewportTargetLogicalHeight(viewport_target_game_logical_height);
    SetViewportUIScaleScalar(viewport_ui_pixels_per_unit_override);
    InitViewportLayout(screenWidth, screenHeight, screen_resolution_scalar);
    InitUniverseSystem(); // Initialise the universe system with independent universe coordinates
    InitWorldSystem();
    InitUI();
}

void DrawGameplayScreen(void)
{
    DrawUniverse();
    DrawRPanel();
    DrawUI();
    DrawGlobalDebugOverlays();
}

void UpdateGameplayScreen(void)
{
    UpdateDebugOverlayHotkeys(screenWidth, screenHeight, screen_resolution_scalar,
                              &viewport_target_game_logical_height,
                              &viewport_ui_pixels_per_unit_override);

    int mouse_x = GetMouseX();
    int mouse_y = GetMouseY();

    UpdateRPanel(mouse_x, mouse_y);
    UpdateUISystem(mouse_x, mouse_y);
    UpdateUniverseSystem(mouse_x, mouse_y);
    UpdateWorldSystem(mouse_x, mouse_y);
}

