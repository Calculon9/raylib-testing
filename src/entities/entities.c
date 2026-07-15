// /**********************************************************************************************

//  **********************************************************************************************/

// #include "raylib.h"
// #include "screens.h"
// #include <stdio.h>
// #include "utility/utility.h"

// //----------------------------------------------------------------------------------
// // Module Variables Definition (local)
// //----------------------------------------------------------------------------------
// static int framesCounter = 0;
// static int finishScreen = 0;
// static Font fontSidePanel = {0};
// static Font fontStage = {0};

// static 

// //----------------------------------------------------------------------------------
// // Gameplay Screen Functions Definition
// //----------------------------------------------------------------------------------

// // Gameplay Screen Initialization logic
// void InitGameplayScreen(void)
// {
//     // Initialise utilities (FPS tracking, etc.)
//     InitUtilities();

//     // TODO: Initialize GAMEPLAY screen variables here!
//     framesCounter = 0;
//     finishScreen = 0;
// }

// // Gameplay Screen Update logic
// void UpdateGameplayScreen(void)
// {
//     // TODO: Update GAMEPLAY screen variables here!
//     UpdateGameplayScreenPanel();
    
//     UpdateGameplayScreenStage();
    
//     // Press enter or tap to change to ENDING screen
//     // if (IsKeyPressed(KEY_ENTER) || IsGestureDetected(GESTURE_TAP))
//     // {
//     //     finishScreen = 1;
//     //     PlaySound(fxCoin);
//     // }


//     // Press enter or tap to change to ENDING screen
//     // if (IsKeyPressed(KEY_ENTER) || IsGestureDetected(GESTURE_TAP))
//     // {
//     //     finishScreen = 1;
//     //     PlaySound(fxCoin);
//     // }
// }

// // Gameplay Screen Stage Update logic
// void UpdateGameplayScreenPanel(void) {

//     UpdateUtilities();
// }

// // Gameplay Screen Stage Update logic
// void UpdateGameplayScreenStage(void) {

//     //Draw a circle where the mouse clicks and add it to the state
//     if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
//     {
//         int x = GetMouseX();
//         int y = GetMouseY();
//         int radius = 32;
//         Color color = DARKBROWN;
//         DrawCircle(x, y, radius, color);
//         //finishScreen = 1;
//         //PlaySound(fxCoin);
//     }

//     //
// }

// // Gameplay Screen Draw logic
// void DrawGameplayScreen(void)
// {
//     int panel_width = 250;
//     int panel_height = GetScreenHeight();
//     int stage_width = GetScreenWidth() - panel_width;
//     int stage_height = GetScreenHeight();

//     // Draw the side panel
//     DrawGameplayScreenPanel(0, 0, panel_width, panel_height, BROWN);

//     // Draw the stage
//     DrawGameplayScreenStage(panel_width, 0, stage_width, stage_height, DARKGREEN);
//     // Vector2 pos = { 20, 100 };
//     // DrawTextEx(font, "GAMEPLAY SCREEN", pos, font.baseSize*3.0f, 4, MAROON);

//     // DrawText("PRESS ENTER or TAP to JUMP to ENDING SCREEN", 130, 220, 20, MAROON);
// }

// // Gameplay Screen - Stats panel Draw
// void DrawGameplayScreenPanel(int startX, int startY, int width, int height, Color color)
// {
//     // TODO: Draw GAMEPLAY screen here!
//     DrawRectangle(startX, startY, width, height, color);
//     Vector2 pos = {20, 100};
//     Vector2 lineSpacing = {0, 40};

//     char fpsText[32];                                              // Buffer to hold the FPS text
//     snprintf(fpsText, sizeof(fpsText), "FPS: %.2f", GetFps().fps); // Format the FPS value into the buffer
//     // printf("%c", fpsText);
//     DrawTextEx(font, "Circloids: ", pos, font.baseSize * 2.0f, 2, BEIGE);

//     DrawTextEx(font, fpsText, (Vector2){pos.x + lineSpacing.x, pos.y + lineSpacing.y}, font.baseSize * 2.0f, 2, BEIGE);

//     // Update FPS
//     // DrawTextEx(font, fpsText, (Vector2){pos.x + lineSpacing.x, pos.y + lineSpacing.y}, font.baseSize*2.0f, 2, GREEN);
// }

// // Gameplay Screen - Main stage Draw
// void DrawGameplayScreenStage(int startX, int startY, int width, int height, Color color)
// {
//     // TODO: Draw GAMEPLAY screen here!
//     DrawRectangle(startX, startY, width, height, color);
// }

// // Gameplay Screen Unload logic
// void UnloadGameplayScreen(void)
// {
//     // TODO: Unload GAMEPLAY screen variables here!
// }

// // Gameplay Screen should finish?
// int FinishGameplayScreen(void)
// {
//     return finishScreen;
// }

// // // Gameplay Screen Draw logic
// // void DrawGameplayScreen(void)
// // {
// //     // TODO: Draw GAMEPLAY screen here!
// //     DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), PURPLE);
// //     Vector2 pos = { 20, 100 };
// //     DrawTextEx(font, "GAMEPLAY SCREEN", pos, font.baseSize*3.0f, 4, MAROON);
// //     DrawText("PRESS ENTER or TAP to JUMP to ENDING SCREEN", 130, 220, 20, MAROON);
// // }
