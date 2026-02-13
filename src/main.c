// Central application entry point 

#include <stdlib.h> 
#include <stdio.h> 
#include <SDL.h> 
#include <SDL_image.h> 

#include "lib.c"
#include "official.c"
#include "custom.c"
#include "game.c"
#include "editor.c"
#include "overlay.c"

struct App {
    // basic sdl2
    SDL_Window *window; 
    SDL_Renderer *renderer; 
    int running; 
    float delta_time;

    // manages state and state transitions
    enum AppState state; 
    enum AppState next_state; 

    int came_from_editor; // this will just keep track of if we came from the editor so I know to go back to that 

    // tiles for the menu and the font
    SDL_Texture *tiles; 
    TTF_Font *font; 
    unsigned mouse_row, mouse_col; // keeps track of mouse for menu drawing

    // progress data 
    unsigned num_completed; 
    unsigned num_custom; 

    // level data
    enum LevelType last_type; 
    unsigned last_id; // stores the id of the last level 

    // state data
    struct OfficialSelect official; 
    struct CustomSelect custom; 
    struct Game game; 
    struct Editor editor; 
    struct GameOverlay overlay; 
}; 

void enter_app_state(struct App *app) {
    // prepare a new state for entry 
    if (app->next_state == InOfficial) {
        enter_official_select(&app->official, app->renderer, app->font, app->num_completed); 
    }
    else if (app->next_state == InCustom) {
        enter_custom_select(&app->custom, app->renderer, app->font, app->num_custom); 
    }
    else if (app->next_state == InGame) {
        

        // if we came from the editor, then mark that, but if we came from the official or custom unmark (if we came from overlay, just maintain the value)
        if (app->state == InEditor) app->came_from_editor = 1; 
        else if (app->state == InOfficial || app->state == InCustom) app->came_from_editor = 0; 


        // restart it if there is not the one case where the game is coming from a pause overlay that did not flag to restart the game 
        if (!(app->state == InOverlay && app->overlay.type == PausePage && !app->overlay.pause_restart_game)) {
            // my logic for why the game takes just the path and not the level type + id, is that unlike the menus, the game should not be aware of any level progression or level system. 
            // instead it should simply take in a path for the level to play, play it, then exit. 
            // this may change though with more complex features added later, so I am not totally stuck on this since either way the game only needs the level info on enter and exit
            char path[32];
            sprintf(path, "levels/%s/%d.lvl", app->last_type == OfficialLevel ? "official" : "custom", app->last_id);
            enter_game(&app->game, path, app->font, app->renderer); 
        }
       
    }
    else if (app->next_state == InEditor) {
        char path[32];
        sprintf(path, "levels/%s/%d.lvl", app->last_type == OfficialLevel ? "official" : "custom", app->last_id);
        enter_editor_state(&app->editor, app->renderer, app->font, path); 
    }
    else if (app->next_state == InOverlay) {
        enum OverlayType type = app->game.player.state == Winning? WinPage: app->game.player.state == Exploding? LosePage: PausePage; 

        char path[32];
        sprintf(path, "levels/%s/%d.lvl", app->last_type == OfficialLevel ? "official" : "custom", app->last_id);
        FILE *file = fopen(path, "rb"); 
        char name[32]; 
        fread(name, sizeof(name), 1, file); 
        fclose(file); 
        enter_overlay_state(&app->overlay, type, app->game.timer, app->game.record, name, app->renderer, app->font, app->last_type, app->last_id); 
    }
}

void exit_app_state(struct App *app) {
    // cleanup the old state when entering a new state
    if (app->state == InOfficial) {
        cleanup_previews(app->official.previews); 
    }
    else if (app->state == InCustom) {
        cleanup_previews(app->custom.previews); 
    }
    else if (app->state == InGame) {
        // do not delete if entering an overlay state
        if (app->next_state != InOverlay) {
            char path[32];
            sprintf(path, "levels/%s/%d.lvl", app->last_type == OfficialLevel ? "official" : "custom", app->last_id);
            exit_game(&app->game, path, app->last_type, app->last_id, &app->num_completed); 
        }
    }
    else if (app->state == InEditor) {
        char path[32];
        sprintf(path, "levels/%s/%d.lvl", app->last_type == OfficialLevel ? "official" : "custom", app->last_id);
        exit_editor_state(&app->editor, path); 
    }
    else if (app->state == InOverlay) {
        // exit the game bellow the overlay when the overlay is exititng if it is win or lose or pause with a restart flag 
        if (app->overlay.type == WinPage || app->overlay.type == LosePage || (app->overlay.type == PausePage && app->overlay.pause_restart_game)) {
            char path[32];
            sprintf(path, "levels/%s/%d.lvl", app->last_type == OfficialLevel ? "official" : "custom", app->last_id);
            exit_game(&app->game, path, app->last_type, app->last_id, &app->num_completed);    
        }
        

        // then exit overlay itself 
        exit_overlay_state(&app->overlay); 
    }
}
     

void init_app(struct App *app) {
    // initialize permanent objects (run at program startup); 
    // sdl2 init
    SDL_Init(SDL_INIT_VIDEO); 
    IMG_Init(IMG_INIT_PNG); 
    TTF_Init(); 
    Mix_Init(MIX_INIT_MP3); 
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048); 
    app->window = SDL_CreateWindow("Rolleron", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_RESIZABLE);
    SDL_MaximizeWindow(app->window);

    app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC); 
    app->running = 1; 

    // app icon
    SDL_Surface* icon = IMG_Load("assets/app_icon.png"); // load PNG
    SDL_SetWindowIcon(app->window, icon);
    SDL_FreeSurface(icon);

    // start in the offical level menu 
    app->state = InOfficial; 
    app->next_state = InOfficial;

    app->tiles = IMG_LoadTexture(app->renderer, "assets/low_res_tiles.png"); 
    app->font = TTF_OpenFont("assets/conthrax.otf", 64);

    FILE *file = fopen("levels/progress.dat", "rb"); 
    fread(&app->num_completed, sizeof(unsigned), 1, file); 
    fread(&app->num_custom, sizeof(unsigned), 1, file); 
    fclose(file); 

    // intialize each of the app states and enter the current state
    init_official_select(&app->official, app->renderer, app->font, app->num_completed);
    init_custom_select(&app->custom, app->renderer, app->font, app->num_custom); 
    init_game(&app->game, app->renderer); 
    init_editor(&app->editor, app->renderer, app->font);
    init_overlay(&app->overlay, app->renderer, app->font); 
    enter_app_state(app); 

}

void cleanup_app(struct App *app) {
    // exit the state
    exit_app_state(app); 

    // cleanup all states
    cleanup_overlay(&app->overlay); 
    cleanup_editor(&app->editor); 
    cleanup_game(&app->game); 
    cleanup_custom_select(&app->custom); 
    cleanup_official_select(&app->official); 

    TTF_CloseFont(app->font);
    SDL_DestroyTexture(app->tiles); 
    SDL_DestroyWindow(app->window); 
    SDL_DestroyRenderer(app->renderer); 
    TTF_Quit(); 
    IMG_Quit(); 
    SDL_Quit(); 
}


void handle_events(struct App *app) {
    for (SDL_Event event; SDL_PollEvent(&event); ) {
        // window exit 
        if (event.type == SDL_QUIT) app->running = 0;   

        // window resize
        else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
            int window_w = event.window.data1, window_h = event.window.data2; 
            int display_w = (float)window_w/window_h > 16/9.0 ? window_h * 16/9.0: window_w; 
            int display_h = (float)window_w/window_h > 16/9.0 ? window_h: window_w / (16/9.0); 
            SDL_RenderSetViewport(app->renderer, &(SDL_Rect){(window_w - display_w)/2, (window_h - display_h)/2, display_w, display_h});
        }

        // mouse position tracking for menus 
        else if (event.type == SDL_MOUSEMOTION) {
            get_mouse_coords(event.motion.x, event.motion.y, app->renderer, &app->mouse_row, &app->mouse_col); 
        }

        // state specific event dispatch 
        if (app->state == InOfficial) {
            handle_official_event(&app->official, &event, app->renderer, app->font, app->num_completed, &app->next_state, &app->last_type, &app->last_id); 
        }
        else if (app->state == InCustom) {
            handle_custom_event(&app->custom, &event, app->renderer, app->font, &app->num_custom, &app->next_state, &app->last_type, &app->last_id); 
        }
        else if (app->state == InGame) {
            handle_game_event(&app->game, &event, &app->next_state); 
        }
        else if (app->state == InEditor) {
            handle_editor_event(&app->editor, &event, app->renderer, app->font, &app->next_state); 
        }
        else if (app->state == InOverlay) {
            handle_overlay_event(&app->overlay, &event, app->renderer, app->last_type, &app->next_state, &app->last_id, app->came_from_editor); 
        }
    }
}

void update_and_render(struct App *app) {
    // update if needed and render the current state
    if (app->state == InOfficial) {
        render_official_select(&app->official, app->renderer, app->tiles, app->mouse_row, app->mouse_col, app->num_completed); 
    }
    else if (app->state == InCustom) {
        render_custom_select(&app->custom, app->renderer, app->tiles, app->mouse_row, app->mouse_col, app->num_custom); 
    }
    else if (app->state == InGame) {
        update_game(&app->game, app->delta_time, app->font, app->renderer, &app->next_state); 
        render_game(&app->game, app->renderer); 
    }
    else if (app->state == InEditor) {
        update_editor(&app->editor); 
        render_editor(&app->editor, app->renderer, app->tiles, app->mouse_row, app->mouse_col); 
    }
    else if (app->state == InOverlay) {
        render_game(&app->game, app->renderer); 
        render_overlay(&app->overlay, app->renderer, app->tiles, app->mouse_row, app->mouse_col); 
    }
}

void run_app(struct App *app) {
    // Application lifetime manager 

    // for delta time tracking 
    unsigned long last_time = SDL_GetPerformanceCounter(); 
    unsigned long frequency = SDL_GetPerformanceFrequency();
    
    while (app->running) {
        unsigned long current_time = SDL_GetPerformanceCounter(); 
        app->delta_time = (current_time - last_time)/(float)frequency; 
        
        handle_events(app); 
        update_and_render(app); 

        // manages the state transition 
        if (app->state != app->next_state) {
            exit_app_state(app); // exit current state 
            enter_app_state(app); // enter next state 
            app->state = app->next_state; 

        }

        SDL_RenderPresent(app->renderer); 

        last_time = current_time;   
    }
}

int main() {
    // simply create the app, initialize it, run it, and then clean it up
    struct App app; 
    init_app(&app);    
    run_app(&app); 
    cleanup_app(&app); 

    return EXIT_SUCCESS; 
}



