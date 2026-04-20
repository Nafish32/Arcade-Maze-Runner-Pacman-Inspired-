#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/freeglut.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

namespace {

// Board and window settings for the tile-based maze.
constexpr int MAP_COLS = 28;
constexpr int MAP_ROWS = 31;
constexpr int TILE_SIZE = 24;
constexpr int HUD_HEIGHT = 92;
constexpr int BOARD_LEFT = 40;
constexpr int BOARD_BOTTOM = 40;
constexpr int BOARD_WIDTH = MAP_COLS * TILE_SIZE;
constexpr int BOARD_HEIGHT = MAP_ROWS * TILE_SIZE;
constexpr int WINDOW_WIDTH = BOARD_WIDTH + BOARD_LEFT * 2;
constexpr int WINDOW_HEIGHT = BOARD_HEIGHT + BOARD_BOTTOM + HUD_HEIGHT;
constexpr int TUNNEL_ROW = 14;

constexpr float PLAYER_SPEED = 142.0f;
constexpr float PLAYER_FRIGHTENED_SPEED = 136.0f;
constexpr float GHOST_SPEED = 128.0f;
constexpr float GHOST_FRIGHTENED_SPEED = 96.0f;
constexpr float GHOST_EATEN_SPEED = 220.0f;
constexpr float GHOST_RESPAWN_DELAY = 0.35f;
constexpr float PLAYER_RADIUS = TILE_SIZE * 0.42f;
constexpr float GHOST_RADIUS = TILE_SIZE * 0.42f;
constexpr float CENTER_EPSILON = 1.25f;
constexpr float COLLISION_DISTANCE = TILE_SIZE * 0.72f;

enum GameState {
    MENU,
    READY,
    PLAYING,
    WIN,
    GAMEOVER
};

enum Direction {
    DIR_UP = 0,
    DIR_LEFT = 1,
    DIR_DOWN = 2,
    DIR_RIGHT = 3,
    DIR_NONE = 4
};

enum GhostKind {
    BLINKY,
    PINKY,
    INKY,
    CLYDE
};

enum GhostState {
    GHOST_ACTIVE,
    GHOST_HOUSE,
    GHOST_LEAVING,
    GHOST_EATEN
};

struct Color {
    float r;
    float g;
    float b;
};

struct TilePoint {
    int col;
    int row;
};

struct Actor {
    float x;
    float y;
    Direction dir;
};

struct Ghost {
    GhostKind kind;
    std::string name;
    Color color;
    float x;
    float y;
    Direction dir;
    GhostState state;
    int homeCol;
    int homeRow;
    int scatterCol;
    int scatterRow;
    float releaseDelay;
    float releaseClock;
};

const std::array<int, 4> DIR_COL = {0, -1, 0, 1};
const std::array<int, 4> DIR_ROW = {-1, 0, 1, 0};
const std::array<float, 4> DIR_X = {0.0f, -1.0f, 0.0f, 1.0f};
const std::array<float, 4> DIR_Y = {1.0f, 0.0f, -1.0f, 0.0f};

const std::array<std::string, MAP_ROWS> RAW_MAZE = {{
    "############################",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#o####.#####.##.#####.####o#",
    "#.####.#####.##.#####.####.#",
    "#..........................#",
    "#.####.##.########.##.####.#",
    "#.####.##.########.##.####.#",
    "#......##....##....##......#",
    "######.##### ## #####.######",
    "~~~~~#.##### ## #####.#~~~~~",
    "~~~~~#.##          ##.#~~~~~",
    "~~~~~#.## ###==### ##.#~~~~~",
    "######.## #------# ##.######",
    "      .   #------#   .      ",
    "######.## #------# ##.######",
    "~~~~~#.## ######## ##.#~~~~~",
    "~~~~~#.##          ##.#~~~~~",
    "~~~~~#.## ######## ##.#~~~~~",
    "######.## ######## ##.######",
    "#.####.#####.##.#####.####.#",
    "#.####.#####.##.#####.####.#",
    "#o..##.......  .......##..o#",
    "###.##.##.########.##.##.###",
    "###.##.##.########.##.##.###",
    "#......##....##....##......#",
    "#.##########.##.##########.#",
    "#.##########.##.##########.#",
    "#..........................#",
    "#.####.#####.##.#####.####.#",
    "############################"
}};

GameState gameState = MENU;
std::vector<std::string> maze;
Actor player = {0.0f, 0.0f, DIR_LEFT};
Direction queuedDirection = DIR_LEFT;
std::vector<Ghost> ghosts;

int score = 0;
int lives = 3;
int remainingPellets = 0;
int frightenedChain = 0;

bool frightenedMode = false;
float frightenedTimer = 0.0f;
float roundTimer = 0.0f;
float readyTimer = 0.0f;
float hudTime = 0.0f;
int lastTickMs = 0;

std::size_t modeIndex = 0;
float modePhaseTimer = 0.0f;

const std::array<float, 7> MODE_PHASES = {{7.0f, 20.0f, 7.0f, 20.0f, 5.0f, 20.0f, 5.0f}};
const std::array<bool, 7> MODE_IS_SCATTER = {{true, false, true, false, true, false, true}};

bool isScatterMode() {
    if (modeIndex >= MODE_IS_SCATTER.size()) {
        return false;
    }
    return MODE_IS_SCATTER[modeIndex];
}

Direction oppositeDirection(Direction dir) {
    switch (dir) {
        case DIR_UP: return DIR_DOWN;
        case DIR_DOWN: return DIR_UP;
        case DIR_LEFT: return DIR_RIGHT;
        case DIR_RIGHT: return DIR_LEFT;
        default: return DIR_NONE;
    }
}

float tileCenterX(int col) {
    return BOARD_LEFT + col * TILE_SIZE + TILE_SIZE * 0.5f;
}

float tileCenterY(int row) {
    return BOARD_BOTTOM + BOARD_HEIGHT - (row * TILE_SIZE + TILE_SIZE * 0.5f);
}

int clampRow(int row) {
    return std::max(0, std::min(MAP_ROWS - 1, row));
}

int clampCol(int col) {
    return std::max(0, std::min(MAP_COLS - 1, col));
}

int nearestCol(float x) {
    // Convert screen x into the nearest maze column.
    float local = (x - (BOARD_LEFT + TILE_SIZE * 0.5f)) / static_cast<float>(TILE_SIZE);
    return clampCol(static_cast<int>(std::floor(local + 0.5f)));
}

int nearestRow(float y) {
    // Convert screen y into the nearest maze row.
    float local = ((BOARD_BOTTOM + BOARD_HEIGHT - TILE_SIZE * 0.5f) - y) / static_cast<float>(TILE_SIZE);
    return clampRow(static_cast<int>(std::floor(local + 0.5f)));
}

TilePoint actorTile(float x, float y) {
    return {nearestCol(x), nearestRow(y)};
}

char tileAt(int row, int col) {
    // Treat out-of-bounds tiles as walls except for the side tunnel row.
    if (row < 0 || row >= MAP_ROWS) {
        return '#';
    }
    if (col < 0 || col >= MAP_COLS) {
        return row == TUNNEL_ROW ? ' ' : '#';
    }
    return maze[row][col];
}

bool isPlayerWalkableChar(char cell) {
    return cell != '#' && cell != '=' && cell != '-' && cell != '~';
}

bool isGhostWalkableChar(char cell, bool allowHouse, bool allowGate) {
    if (cell == '#' || cell == '~') {
        return false;
    }
    if (cell == '=') {
        return allowGate;
    }
    if (cell == '-') {
        return allowHouse;
    }
    return true;
}

bool canPlayerMoveFrom(int row, int col, Direction dir) {
    if (dir == DIR_NONE) {
        return false;
    }
    int nextRow = row + DIR_ROW[dir];
    int nextCol = col + DIR_COL[dir];
    return isPlayerWalkableChar(tileAt(nextRow, nextCol));
}

bool canGhostMoveFrom(const Ghost& ghost, int row, int col, Direction dir) {
    if (dir == DIR_NONE) {
        return false;
    }
    int nextRow = row + DIR_ROW[dir];
    int nextCol = col + DIR_COL[dir];
    // Ghosts can only cross the house area or gate in specific states.
    bool allowHouse = ghost.state == GHOST_HOUSE || ghost.state == GHOST_LEAVING || ghost.state == GHOST_EATEN;
    bool allowGate = ghost.state == GHOST_LEAVING || ghost.state == GHOST_EATEN;
    return isGhostWalkableChar(tileAt(nextRow, nextCol), allowHouse, allowGate);
}

bool snapToTileCenter(float& x, float& y) {
    // Snap actors onto exact tile centers so turns and collisions stay precise.
    TilePoint tile = actorTile(x, y);
    float cx = tileCenterX(tile.col);
    float cy = tileCenterY(tile.row);
    if (std::fabs(x - cx) <= CENTER_EPSILON && std::fabs(y - cy) <= CENTER_EPSILON) {
        x = cx;
        y = cy;
        return true;
    }
    return false;
}

void drawText(void* font, const std::string& text, float x, float y) {
    glRasterPos2f(x, y);
    for (char c : text) {
        glutBitmapCharacter(font, c);
    }
}

void drawLineBresenham(int x1, int y1, int x2, int y2, int thickness = 1) {
    // Bresenham draws a line with integer steps instead of floating-point interpolation.
    int dx = std::abs(x2 - x1);
    int dy = std::abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    glBegin(GL_POINTS);
    while (true) {
        for (int ox = 0; ox < thickness; ++ox) {
            for (int oy = 0; oy < thickness; ++oy) {
                glVertex2i(x1 + ox, y1 + oy);
            }
        }

        if (x1 == x2 && y1 == y2) {
            break;
        }

        int e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
    glEnd();
}

void drawFilledCircle(float cx, float cy, float radius, int segments = 32) {
    (void)segments;

    // Midpoint circle logic fills the circle using horizontal pixel spans.
    int xc = static_cast<int>(std::lround(cx));
    int yc = static_cast<int>(std::lround(cy));
    int r = std::max(1, static_cast<int>(std::lround(radius)));

    int x = 0;
    int y = r;
    int d = 1 - r;

    glBegin(GL_POINTS);
    auto plotSpan = [&](int left, int right, int py) {
        for (int px = left; px <= right; ++px) {
            glVertex2i(px, py);
        }
    };

    while (x <= y) {
        plotSpan(xc - x, xc + x, yc + y);
        plotSpan(xc - x, xc + x, yc - y);
        plotSpan(xc - y, xc + y, yc + x);
        plotSpan(xc - y, xc + y, yc - x);

        ++x;
        if (d < 0) {
            d += 2 * x + 1;
        } else {
            --y;
            d += 2 * (x - y) + 1;
        }
    }
    glEnd();
}

void drawRect(float x1, float y1, float x2, float y2) {
    glBegin(GL_QUADS);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
    glEnd();
}

void drawMaze() {
    // Draw each maze tile and add bright edge lines on exposed wall sides.
    glColor3f(0.02f, 0.02f, 0.06f);
    drawRect(
        static_cast<float>(BOARD_LEFT),
        static_cast<float>(BOARD_BOTTOM),
        static_cast<float>(BOARD_LEFT + BOARD_WIDTH),
        static_cast<float>(BOARD_BOTTOM + BOARD_HEIGHT)
    );

    for (int row = 0; row < MAP_ROWS; ++row) {
        for (int col = 0; col < MAP_COLS; ++col) {
            char cell = maze[row][col];
            float x1 = BOARD_LEFT + col * TILE_SIZE;
            float y1 = BOARD_BOTTOM + BOARD_HEIGHT - (row + 1) * TILE_SIZE;
            float x2 = x1 + TILE_SIZE;
            float y2 = y1 + TILE_SIZE;

            if (cell == '#') {
                glColor3f(0.0f, 0.09f, 0.35f);
                drawRect(x1, y1, x2, y2);

                glColor3f(0.1f, 0.55f, 1.0f);
                if (tileAt(row - 1, col) != '#') {
                    drawLineBresenham((int)x1, (int)y2, (int)x2, (int)y2, 2);
                }
                if (tileAt(row + 1, col) != '#') {
                    drawLineBresenham((int)x1, (int)y1, (int)x2, (int)y1, 2);
                }
                if (tileAt(row, col - 1) != '#') {
                    drawLineBresenham((int)x1, (int)y1, (int)x1, (int)y2, 2);
                }
                if (tileAt(row, col + 1) != '#') {
                    drawLineBresenham((int)x2, (int)y1, (int)x2, (int)y2, 2);
                }
            } else if (cell == '=') {
                glColor3f(1.0f, 0.75f, 0.82f);
                int gateY = static_cast<int>(y1 + TILE_SIZE * 0.5f);
                drawLineBresenham((int)x1 + 4, gateY, (int)x2 - 4, gateY, 2);
            }
        }
    }
}

void drawPellets() {
    // Power pellets pulse slightly to stand out from regular pellets.
    float pulse = 1.0f + 0.18f * std::sin(hudTime * 8.0f);
    glColor3f(1.0f, 0.9f, 0.7f);

    for (int row = 0; row < MAP_ROWS; ++row) {
        for (int col = 0; col < MAP_COLS; ++col) {
            char cell = maze[row][col];
            float cx = tileCenterX(col);
            float cy = tileCenterY(row);

            if (cell == '.') {
                drawFilledCircle(cx, cy, 3.2f, 16);
            } else if (cell == 'o') {
                drawFilledCircle(cx, cy, 6.6f * pulse, 24);
            }
        }
    }
}

void drawPacman() {
    // The mouth angle changes over time to fake the classic chomping animation.
    float mouth = 22.0f + 14.0f * (0.5f + 0.5f * std::sin(hudTime * 14.0f));
    float baseAngle = 0.0f;

    switch (player.dir) {
        case DIR_LEFT: baseAngle = 180.0f; break;
        case DIR_UP: baseAngle = 90.0f; break;
        case DIR_DOWN: baseAngle = 270.0f; break;
        case DIR_RIGHT:
        case DIR_NONE:
        default:
            baseAngle = 0.0f;
            break;
    }

    glColor3f(1.0f, 0.92f, 0.06f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(player.x, player.y);
    for (int angle = static_cast<int>(baseAngle + mouth);
         angle <= static_cast<int>(baseAngle + 360.0f - mouth);
         angle += 5) {
        float radians = angle * 3.14159265f / 180.0f;
        glVertex2f(
            player.x + std::cos(radians) * PLAYER_RADIUS,
            player.y + std::sin(radians) * PLAYER_RADIUS
        );
    }
    glEnd();

    glColor3f(0.0f, 0.0f, 0.0f);
    drawFilledCircle(player.x + 2.0f, player.y + PLAYER_RADIUS * 0.45f, 1.6f, 12);
}

void drawGhostBody(float x, float y, float radius, const Color& color) {
    glColor3f(color.r, color.g, color.b);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for (int i = 0; i <= 24; ++i) {
        float angle = 3.14159265f * static_cast<float>(i) / 24.0f;
        glVertex2f(x + std::cos(angle) * radius, y + std::sin(angle) * radius);
    }
    glEnd();

    glBegin(GL_POLYGON);
    glVertex2f(x - radius, y);
    glVertex2f(x - radius, y - radius * 0.78f);
    glVertex2f(x - radius * 0.55f, y - radius * 0.42f);
    glVertex2f(x - radius * 0.15f, y - radius * 0.78f);
    glVertex2f(x + radius * 0.2f, y - radius * 0.42f);
    glVertex2f(x + radius * 0.55f, y - radius * 0.78f);
    glVertex2f(x + radius, y - radius * 0.42f);
    glVertex2f(x + radius, y);
    glEnd();
}

Color frightenedColor(const Ghost& ghost) {
    // Frightened ghosts flash near the end so the player knows the mode is expiring.
    if (ghost.state == GHOST_EATEN) {
        return {0.0f, 0.0f, 0.0f};
    }
    if (!frightenedMode) {
        return ghost.color;
    }
    if (frightenedTimer < 2.0f && std::sin(hudTime * 18.0f) > 0.0f) {
        return {0.96f, 0.96f, 0.96f};
    }
    return {0.12f, 0.25f, 0.95f};
}

void drawGhost(const Ghost& ghost) {
    if (ghost.state != GHOST_EATEN) {
        drawGhostBody(ghost.x, ghost.y, GHOST_RADIUS, frightenedColor(ghost));
    }

    float eyeY = ghost.y + GHOST_RADIUS * 0.25f;
    float leftEyeX = ghost.x - GHOST_RADIUS * 0.35f;
    float rightEyeX = ghost.x + GHOST_RADIUS * 0.1f;

    glColor3f(1.0f, 1.0f, 1.0f);
    drawFilledCircle(leftEyeX, eyeY, 3.7f, 16);
    drawFilledCircle(rightEyeX, eyeY, 3.7f, 16);

    float pupilOffsetX = 0.0f;
    float pupilOffsetY = 0.0f;
    switch (ghost.dir) {
        case DIR_LEFT: pupilOffsetX = -1.3f; break;
        case DIR_RIGHT: pupilOffsetX = 1.3f; break;
        case DIR_UP: pupilOffsetY = 1.3f; break;
        case DIR_DOWN: pupilOffsetY = -1.3f; break;
        default: break;
    }

    glColor3f(0.05f, 0.15f, 0.9f);
    drawFilledCircle(leftEyeX + pupilOffsetX, eyeY + pupilOffsetY, 1.8f, 12);
    drawFilledCircle(rightEyeX + pupilOffsetX, eyeY + pupilOffsetY, 1.8f, 12);

    if (frightenedMode && ghost.state != GHOST_EATEN) {
        glColor3f(1.0f, 1.0f, 1.0f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glVertex2f(ghost.x - 4.0f, ghost.y - 2.0f);
        glVertex2f(ghost.x + 4.0f, ghost.y - 2.0f);
        glEnd();
    }
}

void drawLives() {
    for (int i = 0; i < lives; ++i) {
        float x = BOARD_LEFT + 24.0f + i * 26.0f;
        float y = WINDOW_HEIGHT - HUD_HEIGHT + 24.0f;
        glColor3f(1.0f, 0.92f, 0.06f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, y);
        for (int angle = 32; angle <= 328; angle += 6) {
            float radians = angle * 3.14159265f / 180.0f;
            glVertex2f(x + std::cos(radians) * 10.0f, y + std::sin(radians) * 10.0f);
        }
        glEnd();
    }
}

void drawHud() {
    glColor3f(1.0f, 1.0f, 1.0f);
    drawText(GLUT_BITMAP_HELVETICA_18, "SCORE " + std::to_string(score), BOARD_LEFT, WINDOW_HEIGHT - 28.0f);
    drawText(
        GLUT_BITMAP_HELVETICA_18,
        "PELLETS " + std::to_string(remainingPellets),
        BOARD_LEFT + 220.0f,
        WINDOW_HEIGHT - 28.0f
    );
    drawLives();

    std::string modeLabel = frightenedMode ? "MODE FRIGHTENED" : (isScatterMode() ? "MODE SCATTER" : "MODE CHASE");
    drawText(GLUT_BITMAP_HELVETICA_18, modeLabel, BOARD_LEFT + 430.0f, WINDOW_HEIGHT - 28.0f);
}

void drawCenteredMessage(const std::string& title, const std::string& subtitle) {
    glColor3f(1.0f, 1.0f, 0.15f);
    drawText(GLUT_BITMAP_TIMES_ROMAN_24, title, BOARD_LEFT + 180.0f, BOARD_BOTTOM + BOARD_HEIGHT * 0.56f);
    glColor3f(1.0f, 1.0f, 1.0f);
    drawText(GLUT_BITMAP_HELVETICA_18, subtitle, BOARD_LEFT + 140.0f, BOARD_BOTTOM + BOARD_HEIGHT * 0.48f);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    drawMaze();
    drawPellets();
    drawPacman();
    for (const Ghost& ghost : ghosts) {
        drawGhost(ghost);
    }
    drawHud();

    if (gameState == MENU) {
        drawCenteredMessage("Arcade Maze Runner", "ENTER to start, arrows to queue turns, R to reset.");
    } else if (gameState == READY) {
        drawCenteredMessage("READY!", "Turn inputs buffer at tile centers like the arcade game.");
    } else if (gameState == WIN) {
        drawCenteredMessage("LEVEL CLEARED", "You ate every pellet. Press R for another run.");
    } else if (gameState == GAMEOVER) {
        drawCenteredMessage("GAME OVER", "Press R to reload the board.");
    }

    glutSwapBuffers();
}

void rebuildPelletCount() {
    // Count all pellets left in the maze for win detection and HUD display.
    remainingPellets = 0;
    for (const std::string& line : maze) {
        for (char cell : line) {
            if (cell == '.' || cell == 'o') {
                ++remainingPellets;
            }
        }
    }
}

void validateMaze() {
    for (const std::string& row : RAW_MAZE) {
        if (static_cast<int>(row.size()) != MAP_COLS) {
            std::fprintf(stderr, "Maze row has invalid width.\n");
            std::exit(1);
        }
    }
}

void resetRoundPositions() {
    // Reset Pac-Man and ghosts to their starting positions for a fresh round.
    player.x = tileCenterX(13);
    player.y = tileCenterY(22);
    player.dir = DIR_LEFT;
    queuedDirection = DIR_LEFT;

    ghosts.clear();
    ghosts.push_back({BLINKY, "Blinky", {1.0f, 0.1f, 0.1f}, tileCenterX(13), tileCenterY(11), DIR_LEFT, GHOST_ACTIVE, 13, 13, 24, 1, 0.0f, 0.0f});
    ghosts.push_back({PINKY, "Pinky", {1.0f, 0.55f, 0.8f}, tileCenterX(13), tileCenterY(13), DIR_UP, GHOST_HOUSE, 13, 13, 3, 1, 1.5f, 0.0f});
    ghosts.push_back({INKY, "Inky", {0.1f, 0.95f, 1.0f}, tileCenterX(11), tileCenterY(13), DIR_UP, GHOST_HOUSE, 11, 13, 24, 29, 5.5f, 0.0f});
    ghosts.push_back({CLYDE, "Clyde", {1.0f, 0.65f, 0.12f}, tileCenterX(15), tileCenterY(13), DIR_UP, GHOST_HOUSE, 15, 13, 3, 29, 9.0f, 0.0f});

    frightenedMode = false;
    frightenedTimer = 0.0f;
    frightenedChain = 0;
    roundTimer = 0.0f;
    readyTimer = 1.2f;
    modeIndex = 0;
    modePhaseTimer = 0.0f;
    gameState = READY;
}

void resetGame(bool resetScore) {
    maze.assign(RAW_MAZE.begin(), RAW_MAZE.end());
    rebuildPelletCount();
    if (resetScore) {
        score = 0;
        lives = 3;
    }
    resetRoundPositions();
}

TilePoint playerLookAhead(int steps) {
    // Look ahead in Pac-Man's facing direction for ghost targeting rules.
    TilePoint tile = actorTile(player.x, player.y);
    Direction facing = player.dir == DIR_NONE ? queuedDirection : player.dir;
    if (facing == DIR_NONE) {
        return tile;
    }

    tile.col += DIR_COL[facing] * steps;
    tile.row += DIR_ROW[facing] * steps;
    tile.col = clampCol(tile.col);
    tile.row = clampRow(tile.row);
    return tile;
}

TilePoint ghostTarget(const Ghost& ghost) {
    // Each ghost chooses a different target tile to create distinct behaviour.
    if (ghost.state == GHOST_EATEN) {
        return {13, 14};
    }
    if (ghost.state == GHOST_LEAVING) {
        return {13, 11};
    }
    if (isScatterMode()) {
        return {ghost.scatterCol, ghost.scatterRow};
    }

    TilePoint pac = actorTile(player.x, player.y);
    TilePoint aheadFour = playerLookAhead(4);
    TilePoint aheadTwo = playerLookAhead(2);

    switch (ghost.kind) {
        case BLINKY:
            return pac;
        case PINKY:
            return aheadFour;
        case INKY: {
            const Ghost& blinky = ghosts[0];
            TilePoint blinkyTile = actorTile(blinky.x, blinky.y);
            int vx = aheadTwo.col - blinkyTile.col;
            int vy = aheadTwo.row - blinkyTile.row;
            return {
                clampCol(aheadTwo.col + vx),
                clampRow(aheadTwo.row + vy)
            };
        }
        case CLYDE: {
            TilePoint clydeTile = actorTile(ghost.x, ghost.y);
            int dx = pac.col - clydeTile.col;
            int dy = pac.row - clydeTile.row;
            if (dx * dx + dy * dy > 64) {
                return pac;
            }
            return {ghost.scatterCol, ghost.scatterRow};
        }
        default:
            return pac;
    }
}

Direction chooseGhostDirection(const Ghost& ghost) {
    // Choose the next legal direction that best moves the ghost toward its target.
    TilePoint tile = actorTile(ghost.x, ghost.y);
    std::vector<Direction> options;

    for (int dir = 0; dir < 4; ++dir) {
        Direction direction = static_cast<Direction>(dir);
        if (!canGhostMoveFrom(ghost, tile.row, tile.col, direction)) {
            continue;
        }
        if (direction == oppositeDirection(ghost.dir) && ghost.state != GHOST_EATEN) {
            continue;
        }
        options.push_back(direction);
    }

    if (options.empty()) {
        Direction fallback = oppositeDirection(ghost.dir);
        if (canGhostMoveFrom(ghost, tile.row, tile.col, fallback)) {
            return fallback;
        }
        return DIR_NONE;
    }

    if (frightenedMode && ghost.state == GHOST_ACTIVE) {
        return options[std::rand() % options.size()];
    }

    TilePoint target = ghostTarget(ghost);
    float bestDistance = 1e9f;
    Direction bestDirection = options.front();

    for (Direction direction : options) {
        int nextCol = tile.col + DIR_COL[direction];
        int nextRow = tile.row + DIR_ROW[direction];
        float dx = static_cast<float>(target.col - nextCol);
        float dy = static_cast<float>(target.row - nextRow);
        float distanceSquared = dx * dx + dy * dy;

        if (distanceSquared < bestDistance) {
            bestDistance = distanceSquared;
            bestDirection = direction;
        }
    }

    return bestDirection;
}

void wrapActor(float& x, float y) {
    // Move actors from one side tunnel exit to the other.
    int row = nearestRow(y);
    if (row != TUNNEL_ROW) {
        return;
    }

    float leftLimit = BOARD_LEFT - TILE_SIZE * 0.5f;
    float rightLimit = BOARD_LEFT + BOARD_WIDTH + TILE_SIZE * 0.5f;
    if (x < leftLimit) {
        x = rightLimit;
    } else if (x > rightLimit) {
        x = leftLimit;
    }
}

void moveActor(Actor& actor, float speed, float dt) {
    // Pac-Man can only change direction cleanly when he reaches tile centers.
    TilePoint tile = actorTile(actor.x, actor.y);
    bool centered = snapToTileCenter(actor.x, actor.y);

    if (centered) {
        tile = actorTile(actor.x, actor.y);
        if (queuedDirection != DIR_NONE && canPlayerMoveFrom(tile.row, tile.col, queuedDirection)) {
            actor.dir = queuedDirection;
        } else if (!canPlayerMoveFrom(tile.row, tile.col, actor.dir)) {
            actor.dir = DIR_NONE;
        }
    }

    if (actor.dir == DIR_NONE) {
        return;
    }

    actor.x += DIR_X[actor.dir] * speed * dt;
    actor.y += DIR_Y[actor.dir] * speed * dt;
    wrapActor(actor.x, actor.y);
}

void moveGhost(Ghost& ghost, float dt) {
    // Ghosts switch between house, leaving, active, and eaten movement states.
    bool centered = snapToTileCenter(ghost.x, ghost.y);
    TilePoint tile = actorTile(ghost.x, ghost.y);

    if (ghost.state == GHOST_HOUSE) {
        ghost.releaseClock += dt;
        if (ghost.releaseClock >= ghost.releaseDelay) {
            ghost.state = GHOST_LEAVING;
            ghost.dir = DIR_UP;
        }
        return;
    }

    if (centered) {
        tile = actorTile(ghost.x, ghost.y);

        if (ghost.state == GHOST_EATEN && tile.col == 13 && tile.row == 14) {
            ghost.state = GHOST_HOUSE;
            ghost.x = tileCenterX(ghost.homeCol);
            ghost.y = tileCenterY(ghost.homeRow);
            ghost.releaseClock = std::max(ghost.releaseDelay - GHOST_RESPAWN_DELAY, 0.0f);
            ghost.dir = DIR_UP;
            return;
        }

        if (ghost.state == GHOST_LEAVING && tile.row <= 11) {
            ghost.state = GHOST_ACTIVE;
        }

        Direction next = chooseGhostDirection(ghost);
        if (next != DIR_NONE) {
            ghost.dir = next;
        }
    }

    float speed = GHOST_SPEED;
    if (ghost.state == GHOST_EATEN) {
        speed = GHOST_EATEN_SPEED;
    } else if (frightenedMode && ghost.state == GHOST_ACTIVE) {
        speed = GHOST_FRIGHTENED_SPEED;
    }

    ghost.x += DIR_X[ghost.dir] * speed * dt;
    ghost.y += DIR_Y[ghost.dir] * speed * dt;
    wrapActor(ghost.x, ghost.y);
}

void collectPelletIfNeeded() {
    // Eating a power pellet flips all active ghosts into frightened mode.
    TilePoint tile = actorTile(player.x, player.y);
    char& cell = maze[tile.row][tile.col];

    if (cell == '.') {
        cell = ' ';
        score += 10;
        --remainingPellets;
    } else if (cell == 'o') {
        cell = ' ';
        score += 50;
        --remainingPellets;
        frightenedMode = true;
        frightenedTimer = 6.0f;
        frightenedChain = 0;
        for (Ghost& ghost : ghosts) {
            if (ghost.state == GHOST_ACTIVE) {
                ghost.dir = oppositeDirection(ghost.dir);
            }
        }
    }
}

void handlePlayerHit() {
    // Losing a life restarts the round unless all lives are gone.
    --lives;
    if (lives <= 0) {
        gameState = GAMEOVER;
        return;
    }
    resetRoundPositions();
}

void handleGhostCollisions() {
    // Collision outcome depends on whether frightened mode is active.
    for (Ghost& ghost : ghosts) {
        if (ghost.state == GHOST_HOUSE) {
            continue;
        }

        float dx = player.x - ghost.x;
        float dy = player.y - ghost.y;
        float distanceSquared = dx * dx + dy * dy;
        if (distanceSquared > COLLISION_DISTANCE * COLLISION_DISTANCE) {
            continue;
        }

        if (frightenedMode && ghost.state == GHOST_ACTIVE) {
            ghost.state = GHOST_EATEN;
            ghost.dir = oppositeDirection(ghost.dir);
            int ghostScore = 200 << frightenedChain;
            score += ghostScore;
            frightenedChain = std::min(frightenedChain + 1, 3);
        } else if (ghost.state != GHOST_EATEN) {
            handlePlayerHit();
            return;
        }
    }
}

void updateModes(float dt) {
    // This controls the global scatter/chase timer when frightened mode is off.
    if (frightenedMode) {
        frightenedTimer -= dt;
        if (frightenedTimer <= 0.0f) {
            frightenedMode = false;
            frightenedTimer = 0.0f;
            frightenedChain = 0;
        }
        return;
    }

    if (modeIndex >= MODE_PHASES.size()) {
        return;
    }

    modePhaseTimer += dt;
    if (modePhaseTimer >= MODE_PHASES[modeIndex]) {
        modePhaseTimer = 0.0f;
        ++modeIndex;
        for (Ghost& ghost : ghosts) {
            if (ghost.state == GHOST_ACTIVE) {
                ghost.dir = oppositeDirection(ghost.dir);
            }
        }
    }
}

void updateGame(float dt) {
    // One frame update: timers, movement, pickups, collisions, and win state.
    hudTime += dt;
    if (gameState == MENU || gameState == WIN || gameState == GAMEOVER) {
        return;
    }

    if (gameState == READY) {
        readyTimer -= dt;
        if (readyTimer <= 0.0f) {
            gameState = PLAYING;
        }
        return;
    }

    roundTimer += dt;
    updateModes(dt);

    float playerSpeed = frightenedMode ? PLAYER_FRIGHTENED_SPEED : PLAYER_SPEED;
    moveActor(player, playerSpeed, dt);
    collectPelletIfNeeded();

    for (Ghost& ghost : ghosts) {
        moveGhost(ghost, dt);
    }

    handleGhostCollisions();

    if (remainingPellets <= 0) {
        gameState = WIN;
    }
}

void timer(int) {
    // The GLUT timer keeps the game updating at roughly 60 FPS.
    int now = glutGet(GLUT_ELAPSED_TIME);
    if (lastTickMs == 0) {
        lastTickMs = now;
    }

    float dt = (now - lastTickMs) / 1000.0f;
    lastTickMs = now;
    dt = std::max(0.0f, std::min(dt, 0.035f));

    updateGame(dt);
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

void specialKeys(int key, int, int) {
    // Arrow keys only change the queued direction, not the position directly.
    switch (key) {
        case GLUT_KEY_UP: queuedDirection = DIR_UP; break;
        case GLUT_KEY_DOWN: queuedDirection = DIR_DOWN; break;
        case GLUT_KEY_LEFT: queuedDirection = DIR_LEFT; break;
        case GLUT_KEY_RIGHT: queuedDirection = DIR_RIGHT; break;
        default: break;
    }
}

void keyboard(unsigned char key, int, int) {
    if (key == 27) {
        std::exit(0);
    }

    if (key == 13) {
        if (gameState == MENU) {
            resetGame(true);
        }
        return;
    }

    if (key == 'r' || key == 'R') {
        resetGame(true);
    }
}

void initGL() {
    // Set up a simple 2D orthographic view for the maze scene.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, WINDOW_WIDTH, 0.0, WINDOW_HEIGHT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glLineWidth(2.0f);
}

} // namespace

int main(int argc, char** argv) {
    validateMaze();
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("Arcade Maze Runner - freeglut");

    initGL();
    resetGame(true);
    gameState = MENU;

    glutDisplayFunc(display);
    glutSpecialFunc(specialKeys);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(16, timer, 0);

    glutMainLoop();
    return 0;
}
