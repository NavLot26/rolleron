# Rolleron 
This is a physics themed space game I developed using C and SDL2 durring the start of my 10th grade year. It has many different game components, a full level system, and a custom level editor. This is the largest game I have created to this date, and the biggest challenge was designing the entire architecture of a complex codebase with minimal external libraries. 

I will probably upload it to itch or some other website at some point with a possible web version, but I am still in the proccess of designing levels and adding more features.

## Demos
[Game play Demo](https://drive.google.com/file/d/1VXs0bzT8swTN7RwBkQo1q76bbU6oC_HO/view?usp=sharing)

[Menu and Editor Demo](mhttps://drive.google.com/file/d/10NAiwbbfudi8Ji8Lx61_hL0jfKZ1M3-c/view?usp=sharing)


## Tech stack 
- C 
- SDL2

One of the most important aspects of this project was that I created a medium sized game with no game engine and designed everything basically from scratch. I chose C since I like its simplicity and low level control, and SDL2 for its ease of use and minimalism. 

## Architecture
The app manages states, which each are responsible for a different aspect of the game: Official Select, Custom Select, Game, Editor, and Overlay.

Each state is in a file with a struct that owns its data and exposes functions for initialization, cleanup, entry, exit, event handling, rendering, and (when needed) per-frame update.

The app then dispatches these function calls based on the current state and other shared data. 

State transitions are done through request: a state sets *next_state, and the app performs the transitions centrally. This keeps lifetime and ownerships rules explicit and responsibilities localized. 

## Old versions
I did not use git when creating this project, but old versions of the code are stored in the archive folder. 


## Build and Run
Just use the makefile: $ make run

(Note that for simplicity I used a unity build (everything is just included into the main file,) so linking is minimal)