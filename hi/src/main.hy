include {
    std.io,
    vendors.raylib,
}

void main() {
    InitWindow(320, 240, "Zora + Hylian");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(Color { r: 30, g: 30, b: 40, a: 255 });
        EndDrawing();
    }

    Color bleh = {r: 1, g: 2, b: 3, a: 4};

    CloseWindow();
}
