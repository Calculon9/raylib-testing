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
const int screenWidth = 1920;
const int screenHeight = 1080;

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
//----------------------------------------------------------------------------------
// static const int screenWidth = 1920;
// static const int screenHeight = 1080;
static Vector2d resolution = {0};
int screen_resolution_scalar = 100; // used to divide up the pixel resolution to get a local coordinate resolution for the entire screen

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
static void CalculateSpaceProperties(void); // Caclulates the world and UI regions' resolution and origins

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

    // Setup and init first screen
    // currentScreen = LOGO;
    // InitLogoScreen();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 60, 1);
#else
    SetTargetFPS(60); // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    // // 1. Logic & Physics
    // UpdatePhysics(world);

    // // 2. UI Interaction (The Dispatcher)
    // // This lives in ui_input.c
    // ProcessUIInput(GetMouseX(), GetMouseY(), CheckMouseInPanel());

    // // 3. UI Resolution (The Math)
    // // This lives in ui_core.c
    // ResolveUITree(lpanel_root, screen_box, current_basis);

    // // 4. Rendering
    // BeginDrawing();
    //     DrawWorld(world);
    //     // This lives in ui_renderer.c
    //     DrawUITree(lpanel_root);
    // EndDrawing();
    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        // DEBUGGING - we will update game loop if button is pressed
        bool keyDown = IsKeyDown(KEY_LEFT_CONTROL);
        if (true)
        {
            UpdateDrawFrame();
        }
        else
        {
            BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("Hold Left Control to update!", 10, 10, 20, DARKGRAY);
            EndDrawing();
        }
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
        CalculateSpaceProperties();
        InitGameWorld();
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
    CalculateSpaceProperties();
    InitGameWorld();
    InitUI();
}

void DrawGameplayScreen(void)
{
    DrawGameWorld();
    DrawUI();
}

void UpdateGameplayScreen(void)
{
    int mouse_x = GetMouseX();
    int mouse_y = GetMouseY();

    UpdateUISystem(mouse_x, mouse_y);
    UpdateWorldSystem(mouse_x, mouse_y);
}

// Draw transition effect (full-screen rectangle)
static void CalculateSpaceProperties(void)
{
    // 0 CALCULATE LOGICAL/LOCAL resolution from screen's pixel resolution
    resolution = VectorScale_2d((Vector2d){screenWidth, screenHeight}, 1.0f / screen_resolution_scalar);
    resolution.x = floorf(resolution.x);
    resolution.y = floorf(resolution.y);
    float total_space_measure = resolution.x * resolution.y;

    // 1. DEFINE & CALCULATE LOGICAL screen origin and end points for each region (panel, world)
    // 1.1 Give the panel ~1/4 of the x-dimension, and always 100% y-dimension
    lpanel_origin = ZERO_VECTOR_2D;
    lpanel_end.x = floorf(lpanel_origin.x + ((1.0f / 4.0f) * resolution.x));
    lpanel_end.y = resolution.y;
    lpanel_resolution = VectorSum_2d(VectorScale_2d(lpanel_origin, -1), lpanel_end);
    float lpanel_space_measure = VectorBox_2d(lpanel_resolution);

    // 1.2 The game screen simply takes up the rest of the screen
    world_resolution = (Vector2d){resolution.x - lpanel_resolution.x, resolution.y};
    world_origin = (Vector2d){lpanel_end.x, 0};
    world_end = (Vector2d){world_origin.x + world_resolution.x, world_origin.y + world_resolution.y};
    float world_space_measure = VectorBox_2d(world_resolution);

    // 2. BACK-CALCULATE SCREEN PIXEL SPACE Basis for panel and game world
    // 2.1 Influence of resolution scaling
    world_pixel_u = VectorScale_2d(world_u, screen_resolution_scalar);
    world_pixel_v = VectorScale_2d(world_v, screen_resolution_scalar);
    lpanel_pixel_u = VectorScale_2d(lpanel_u, screen_resolution_scalar);
    lpanel_pixel_v = VectorScale_2d(lpanel_v, screen_resolution_scalar);

    // 2.2 Save the basis scaling factors for later use in coordinate conversions
    Basis2d lpanel_basis = (Basis2d){lpanel_u, lpanel_v};  
    Basis2d lpanel_pixel_basis = (Basis2d){lpanel_pixel_u, lpanel_pixel_v};  
    Basis2d world_basis = (Basis2d){world_u, world_v};  
    Basis2d world_pixel_basis = (Basis2d){world_pixel_u, world_pixel_v};  
    local_to_lpanel_scale = BasisTransform_2d_Scale(lpanel_basis, lpanel_pixel_basis); // useful for later
    lpanel_to_local_scale = BasisTransform_2d_Scale(lpanel_pixel_basis, lpanel_basis); // useful for later
    local_to_world_scale = BasisTransform_2d_Scale(world_basis, world_pixel_basis); // useful for later
    world_to_local_scale = BasisTransform_2d_Scale(world_pixel_basis, world_basis); // useful for later

    // 3. CALCULATE SCREEN PIXEL SPACE origins for each region (panel, world)
    lpanel_pixel_origin.x = (lpanel_pixel_u.x + lpanel_pixel_v.x) * lpanel_origin.x;
    lpanel_pixel_origin.y = (lpanel_pixel_u.y + lpanel_pixel_v.y) * lpanel_origin.y;
    world_pixel_origin.x = (world_pixel_u.x + world_pixel_v.x) * world_origin.x;
    world_pixel_origin.y = (world_pixel_u.y + world_pixel_v.y) * world_origin.y;

    // DEBUG - the sum of panel and world resolutions should equal the overall resolution from Step 0
    float resolution_recalc_x = lpanel_resolution.x + world_resolution.x;
    float resolution_recalc_y = lpanel_resolution.y + world_resolution.y;
    float res_recalc_measure = resolution_recalc_x * resolution_recalc_y; // world_space_measure + lpanel_space_measure;

    printf("LOCAL RESOLUTIONS --> TOTAL_LOCAL(%0.1f)(%0.1f,%0.1f); PANEL_LOCAL(%0.1f)(%0.1f,%0.1f); WORLD_LOCAL(%0.1f)(%0.1f,%0.1f); TOTAL_LOCAL_RECALC_MEASURE(%0.1f);\n", total_space_measure, resolution.x, resolution.y, lpanel_space_measure, lpanel_resolution.x, lpanel_resolution.y, world_space_measure, world_resolution.x, world_resolution.y, res_recalc_measure);
    printf("PIXEL ORIGINS --> PANEL(%0.1f,%0.1f); GAME_WORLD (%0.1f,%0.1f);\n", lpanel_pixel_origin.x, lpanel_pixel_origin.y, world_pixel_origin.x, world_pixel_origin.y);
}