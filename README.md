# Pac-Man Arcade Level with `g++` + `freeglut`

This project is now a much closer arcade-style Pac-Man level instead of a loose maze demo.

## What Changed

- 28x31 tile-based maze inspired by the original arcade board
- Buffered turning at tile centers for cleaner, more precise movement
- Side tunnel wrap-around
- Four ghosts with scatter/chase/frightened behavior
- Ghost house and return-to-home logic when ghosts are eaten
- Score, lives, power pellets, and win/game-over states
- Animated Pac-Man and more recognizable ghost rendering

## Build

### Windows (MinGW + freeglut)

```bash
g++ project.cpp -std=c++17 -o pacman.exe -lfreeglut -lopengl32 -lglu32
```

### Linux

```bash
g++ project.cpp -std=c++17 -o pacman -lfreeglut -lGL -lGLU
```

### macOS

```bash
g++ project.cpp -std=c++17 -o pacman -framework GLUT -framework OpenGL
```

## Run

```bash
pacman.exe
```

## Controls

- `Enter`: start from the menu
- `Arrow keys`: move and queue turns
- `R`: reset the level
- `Esc`: quit
