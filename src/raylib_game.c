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
#include "world/world.h"
#include "world/universe.h"
#include "system/viewport_system.h"
#include "system/ui/rpanel_system.h"
#include "system/debug_overlay_system.h"
#include "editor/geometry_editor.h"
#include "input/drag_interaction.h"
#include "input/input_system.h"
#include <stddef.h>
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
typedef struct
{
    float alpha;
    bool active;
    bool fade_out;
    GameScreen from_screen;
    GameScreen to_screen;
} ScreenTransition;

static ScreenTransition transition_state = {
    .alpha = 0.0f,
    .active = false,
    .fade_out = false,
    .from_screen = UNKNOWN,
    .to_screen = UNKNOWN,
};

typedef struct
{
    void (*init_direct_fn)(void);
    void (*init_transition_fn)(void);
    void (*update_fn)(void);
    void (*draw_fn)(void);
    void (*unload_fn)(void);
    int (*finish_fn)(void);
} ScreenHandler;

typedef struct
{
    GameScreen source_screen;
    GameScreen target_screen;
    int expected_finish_code;
    bool match_nonzero;
} ScreenFinishTransitionRule;

static void InitGameplayDirectScreen(void)
{
    InitViewportLayout(screenWidth, screenHeight, screen_resolution_scalar);
}

static const ScreenHandler screen_handlers[GAMEPLAY + 1] = {
    [LOGO] = {
        .init_direct_fn = InitLogoScreen,
        .init_transition_fn = InitLogoScreen,
        .update_fn = UpdateLogoScreen,
        .draw_fn = DrawLogoScreen,
        .unload_fn = NULL,
        .finish_fn = FinishLogoScreen,
    },
    [TITLE] = {
        .init_direct_fn = InitTitleScreen,
        .init_transition_fn = InitTitleScreen,
        .update_fn = UpdateTitleScreen,
        .draw_fn = DrawTitleScreen,
        .unload_fn = NULL,
        .finish_fn = FinishTitleScreen,
    },
    [GAMEPLAY] = {
        .init_direct_fn = InitGameplayDirectScreen,
        .init_transition_fn = InitGameplayScreen,
        .update_fn = UpdateGameplayScreen,
        .draw_fn = DrawGameplayScreen,
        .unload_fn = UnloadGameplayScreen,
        .finish_fn = NULL,
    },
};

static const ScreenFinishTransitionRule screen_finish_transition_rules[] = {
    {LOGO, TITLE, 0, true},
    {TITLE, GAMEPLAY, 2, false},
};

static const ScreenHandler *GetScreenHandler(GameScreen screen)
{
    if (screen < LOGO || screen > GAMEPLAY)
    {
        return NULL;
    }

    return &screen_handlers[(int)screen];
}

static void TransitionToScreen(int screen);

static bool ResolveRequestedTransition(GameScreen screen, int finish_code, GameScreen *next_screen)
{
    if (!next_screen)
    {
        return false;
    }

    for (size_t i = 0; i < ARRAY_COUNT(screen_finish_transition_rules); i++)
    {
        const ScreenFinishTransitionRule rule = screen_finish_transition_rules[i];
        bool matched = false;

        if (rule.source_screen != screen)
        {
            continue;
        }

        if (rule.match_nonzero)
        {
            matched = (finish_code != 0);
        }
        else
        {
            matched = (finish_code == rule.expected_finish_code);
        }

        if (matched)
        {
            *next_screen = rule.target_screen;
            return true;
        }
    }

    return false;
}

static void InitScreenForMode(GameScreen screen, bool use_transition_init)
{
    const ScreenHandler *handler = GetScreenHandler(screen);
    if (!handler)
    {
        return;
    }

    if (use_transition_init)
    {
        if (handler->init_transition_fn)
        {
            handler->init_transition_fn();
        }
    }
    else if (handler->init_direct_fn)
    {
        handler->init_direct_fn();
    }

    currentScreen = screen;
}

static void UpdateCurrentScreenAndTransition(void)
{
    const ScreenHandler *handler = GetScreenHandler(currentScreen);
    if (!handler)
    {
        return;
    }

    if (handler->update_fn)
    {
        handler->update_fn();
    }

    if (!handler->finish_fn)
    {
        return;
    }

    GameScreen next_screen = UNKNOWN;
    int finish_code = handler->finish_fn();
    if (ResolveRequestedTransition(currentScreen, finish_code, &next_screen))
    {
        TransitionToScreen(next_screen);
    }
}

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void TransitionToScreen(int screen); // Request transition to next screen
static void UpdateTransition(void);         // Update transition effect
static void DrawTransition(void);           // Draw transition effect (full-screen rectangle)
static void UpdateDrawFrame(void);          // Update and draw one frame

static void UnloadScreenData(GameScreen screen)
{
    const ScreenHandler *handler = GetScreenHandler(screen);
    if (handler && handler->unload_fn)
    {
        handler->unload_fn();
    }
}

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
    UnloadScreenData(currentScreen);

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
// Request transition to next screen
static void TransitionToScreen(int screen)
{
    transition_state.active = true;
    transition_state.fade_out = false;
    transition_state.from_screen = currentScreen;
    transition_state.to_screen = (GameScreen)screen;
    transition_state.alpha = 0.0f;
}

// Update transition effect (fade-in, fade-out)
static void UpdateTransition(void)
{
    if (!transition_state.fade_out)
    {
        transition_state.alpha += 0.05f;

        // NOTE: Due to float internal representation, condition jumps on 1.0f instead of 1.05f
        // For that reason we compare against 1.01f, to avoid last frame loading stop
        if (transition_state.alpha > 1.01f)
        {
            transition_state.alpha = 1.0f;

            // Unload current screen
            UnloadScreenData(transition_state.from_screen);

            // Load next screen
            InitScreenForMode(transition_state.to_screen, true);

            // Activate fade out effect to next loaded screen
            transition_state.fade_out = true;
        }
    }
    else // Transition fade out logic
    {
        transition_state.alpha -= 0.02f;

        if (transition_state.alpha < -0.01f)
        {
            transition_state.alpha = 0.0f;
            transition_state.fade_out = false;
            transition_state.active = false;
            transition_state.from_screen = UNKNOWN;
            transition_state.to_screen = UNKNOWN;
        }
    }
}

// Draw transition effect (full-screen rectangle)
static void DrawTransition(void)
{
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, transition_state.alpha));
}

// Update and draw game frame
static void UpdateDrawFrame(void)
{
    UpdateFrameCounter(&frame_counter);
    memory_allocated = GetCurrentMemoryAllocated();
    // Update
    //----------------------------------------------------------------------------------
    // UpdateMusicStream(music);       // NOTE: Music keeps playing between screens

    if (!transition_state.active)
    {
        UpdateCurrentScreenAndTransition();
    }
    else
        UpdateTransition(); // Update transition (fade-in, fade-out)
    //----------------------------------------------------------------------------------

    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(RAYWHITE);

    const ScreenHandler *draw_handler = GetScreenHandler(currentScreen);
    if (draw_handler && draw_handler->draw_fn)
    {
        draw_handler->draw_fn();
    }

    // Draw full screen rectangle in front of everything
    if (transition_state.active)
        DrawTransition();

    // DrawFPS(10, 10);

    EndDrawing();
    //----------------------------------------------------------------------------------
}

void InitGameplayScreen(void)
{
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
     InputFrame input = {
        .pointer_position = {(float)mouse_x, (float)mouse_y},
        .left_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
        .left_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
        .left_released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT),
        .right_pressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT),
        .right_down = IsMouseButtonDown(MOUSE_BUTTON_RIGHT),
        .right_released = IsMouseButtonReleased(MOUSE_BUTTON_RIGHT),
        .wheel_delta = GetMouseWheelMove()};

    UpdateInputSystem(&input);
}

