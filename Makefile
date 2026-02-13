bin/main: src/main.c src/lib.c src/official.c src/custom.c src/game.c src/editor.c src/overlay.c
	cc src/main.c -o bin/main \
	-std=c99 -Wall -Wextra -Wpedantic $(shell sdl2-config --cflags) \
	-lm $(shell sdl2-config --libs) -lSDL2_image -lSDL2_ttf -lSDL2_mixer

run: bin/main
	./bin/main
