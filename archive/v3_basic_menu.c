#include <stdlib.h> 
#include <stdio.h> 
#include <SDL.h> 
#include <SDL_image.h> 
#include <SDL_ttf.h> 

#define MAP_W 48
#define MAP_H 32
struct App {
    SDL_Window *window; 
    SDL_Renderer *renderer; 
    int running; 
    float delta_time; 
    enum AppState {InMainMenu, InCustomMenu, InWinMenu, InLoseMenu, InGame, InEditor, InPauseMenu} state; 
    enum AppState next_state; 

    unsigned last_played_level_num; 
    enum LevelType {Normal, Custom} last_played_level_type; 
    unsigned num_unlocked_levels; 
    unsigned num_custom_levels; 
    unsigned last_edited_level_num; 

    struct MainMenu {
        TTF_Font *font; 
        SDL_Texture *title; 
        SDL_Texture *custom_button_text; 
        unsigned level_page; 
        struct LevelPreview {
            SDL_Texture *description; 
            SDL_Texture *best_time; 
        } level_previews[8];  // temp 
    } main_menu; 

    struct CustomMenu {
        SDL_Texture *title; 
        SDL_Texture *main_button_text; 
        SDL_Texture *new_level_button_text; 
        unsigned level_page; 
        struct LevelPreview level_previews[8]; // temp
        SDL_Texture *edit_symbol; 
        
    } custom_menu; 

    struct Game { // all temp except marked perm
        float render_scale; 
        enum Tile {None, Solid, Win, Gravity, AntiGravity, Friction, AntiFriction, ThrustersOn, ThrustersOff, StrongerThrusters, WeakerThrusters, DownForce, UpForce, LeftForce, RightForce, CounterClockwiseTorque, ClockwiseTorque} map[MAP_H][MAP_W]; 
        SDL_Texture *tiles_texture;  // perm

        struct Player {
            enum PlayerState {Playing, Exploding, Winning} state; 
            float x, y;
            float vel_x, vel_y; 
            float rot, rot_vel;  
            int right_thruster, left_thruster; 
            unsigned current_frame; 
            float animation_timer; 
            SDL_Texture *base_texture; // perm
            SDL_Texture *thrusters_texture; // perm
            SDL_Texture *sparks_texture; // perm
            SDL_Texture *exploding_texture; // perm
        } player; 

        float timer; 
        float best_time; 
        float timer_animation_timer; 
        SDL_Texture *timer_texture; 
        SDL_Texture *level_description; 
    } game; 

    struct WinMenu {
        SDL_Texture *completed; 
        SDL_Texture *level_description; // temp
        SDL_Texture *new_record; 
        SDL_Texture *timer_label; 
        SDL_Texture *best_time_label; 
        SDL_Texture *timer; // temp
        SDL_Texture *best_time; // temp
        SDL_Texture *menu_button_text; 
        SDL_Texture *next_button_text; 
    } win_menu; 

    struct LoseMenu {
        SDL_Texture *destroyed; 
        SDL_Texture *level_description; // temp 
        SDL_Texture *timer_label; 
        SDL_Texture *best_time_label;
        SDL_Texture *timer;  // temp
        SDL_Texture *best_time; // temp
        SDL_Texture *menu_button_text; 
        SDL_Texture *retry_button_text; 
    } lose_menu; 

    struct Editor {
        char description[32]; 
        float spawn_x, spawn_y; 
        float spawn_rot; 
        enum Tile map[MAP_H][MAP_W]; 

        float camera_x, camera_y; 
        float render_scale; 

        enum EditorTool {DrawTiles, SetSpawn, RotateSpawn, SetDescription, Leave} tool; 
        enum Tile selected_draw_tile; 
        int drawing_tiles; 
        SDL_Texture *description_texture; 

        SDL_Texture *leave_button_text; // perm 
        SDL_Texture *rename_button_text; // perm 
    } editor; 

    struct PauseMenu {
        SDL_Texture *paused; 
        SDL_Texture *level_description; // temp 
        SDL_Texture *timer_label;
        SDL_Texture *best_time_label; 
        SDL_Texture *timer; // temp 
        SDL_Texture *best_time; // temp 
        SDL_Texture *menu_button_text; 
        SDL_Texture *resume_button_text; 
    } pause_menu; 
}; 

void init_text(SDL_Texture **texture, char *string, TTF_Font *font, SDL_Color color, SDL_Renderer *renderer) {
    SDL_Surface *temp_surface = TTF_RenderText_Blended(font, string, color); 
    *texture = SDL_CreateTextureFromSurface(renderer, temp_surface); 
    SDL_FreeSurface(temp_surface); 
}
enum TextAnchor {MiddleLeft, Middle, MiddleRight}; 
void render_text(SDL_Texture *texture, int x, int y, enum TextAnchor anchor, float scale, SDL_Renderer *renderer) {
    int texture_w, texture_h; SDL_QueryTexture(texture, NULL, NULL, &texture_w, &texture_h); 
    int display_w = texture_w * scale, display_h = texture_h * scale; 
    SDL_RenderCopy(renderer, texture, NULL, &(SDL_Rect){anchor == MiddleLeft? x : anchor == Middle ? x - display_w/2: x - display_w, y - display_h/2, display_w, display_h}); 
}
   
void init_app_state(struct App *app) {
    if (app->state == InMainMenu) { // initialize temporary main menu components (just level buttons)
        // anything that does not exist (best time is inf or non existant level) is set to NULL, on destruction, this is checked for
        for (int i = 0; i < 8; ++i) {
            if (app->main_menu.level_page * 8 + i < 10) {
                char path[32]; sprintf(path, "assets/level%d.lvl", app->main_menu.level_page * 8 + i);
                FILE *level_file = fopen(path, "rb"); 
                char description[32]; fread(description, 32, 1, level_file); 
                float best_time; fread(&best_time, sizeof(float), 1, level_file); 
                fclose(level_file); 

                init_text(&app->main_menu.level_previews[i].description, description, app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 

                if (best_time < INFINITY) {
                    char best_time_string[8]; sprintf(best_time_string, "%.1f", best_time); 
                    init_text(&app->main_menu.level_previews[i].best_time, best_time_string, app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 
                } 
                else app->main_menu.level_previews[i].best_time = NULL; 
            }
            else {
                app->main_menu.level_previews[i].description = NULL; 
                app->main_menu.level_previews[i].best_time = NULL; 
            }
        }
    }   
    else if (app->state == InCustomMenu) { // initialize temporary custom menu compnents (just level buttons)
        for (int i = 0; i < 8; ++i) {
            if (app->custom_menu.level_page * 8 + i < app->num_custom_levels) {
                char path[32]; sprintf(path, "assets/custom_level%d.lvl", app->custom_menu.level_page * 8 + i);
                FILE *level_file = fopen(path, "rb"); 
                char description[32]; fread(description, 32, 1, level_file); 
                float best_time; fread(&best_time, sizeof(float), 1, level_file); 
                fclose(level_file); 

                init_text(&app->custom_menu.level_previews[i].description, description, app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 

                if (best_time < INFINITY) {
                    char best_time_string[8]; sprintf(best_time_string, "%.1f", best_time); 
                    init_text(&app->custom_menu.level_previews[i].best_time, best_time_string, app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 
                }
                else app->custom_menu.level_previews[i].best_time = NULL; 
            }
            else {
                app->custom_menu.level_previews[i].description = NULL; 
                app->custom_menu.level_previews[i].best_time = NULL; 
            }
        }
    }
    else if (app->state == InGame) { // initialize temporary game components (the game state -- everything not marked perm in data structure)
        char path[32]; 
        if (app->last_played_level_type == Normal) sprintf(path, "assets/level%d.lvl", app->last_played_level_num); 
        else if (app->last_played_level_type == Custom) sprintf(path, "assets/custom_level%d.lvl", app->last_played_level_num); 

        FILE *level_file = fopen(path, "rb"); 
        char description[32]; fread(description, 32, 1, level_file); 
        fread(&app->game.best_time, sizeof(float), 1, level_file); 
        fread(&app->game.player.x, sizeof(float), 1, level_file); 
        fread(&app->game.player.y, sizeof(float), 1, level_file); 
        fread(&app->game.player.rot, sizeof(float), 1, level_file); 
        fread(&app->game.map, sizeof(enum Tile), MAP_W * MAP_H, level_file); 
        fclose(level_file); 

        app->game.render_scale = 80; 
        app->game.player.state = Playing; 
        app->game.player.vel_x = 0; app->game.player.vel_y = 0;
        app->game.player.rot_vel = 0; 
        app->game.player.right_thruster = 0; app->game.player.left_thruster = 0; 
        app->game.player.current_frame = 0; 
        app->game.player.animation_timer = 0; 
        app->game.timer = 0; 
        app->game.timer_animation_timer = 0; 

        init_text(&app->game.timer_texture, "0.0", app->main_menu.font, (SDL_Color){0, 100, 0, 255}, app->renderer); 
        init_text(&app->game.level_description, description, app->main_menu.font, (SDL_Color){0, 180, 180, 255},app->renderer); 
    }
    else if (app->state == InWinMenu) { // initialize tmeporary win menu compnents (level name, timer, and best time)
        char path[32]; 
        if (app->last_played_level_type == Normal) sprintf(path, "assets/level%d.lvl", app->last_played_level_num); 
        else if (app->last_played_level_type == Custom) sprintf(path, "assets/custom_level%d.lvl", app->last_played_level_num); 
        FILE *level_file = fopen(path, "rb"); 
        char description[32]; fread(description, 32, 1, level_file); 
        fclose(level_file); 
        init_text(&app->win_menu.level_description, description, app->main_menu.font, (SDL_Color){46, 16, 107, 255}, app->renderer); 
        
        char timer_string[8]; sprintf(timer_string, "%.1f", app->game.timer); 
        init_text(&app->win_menu.timer, timer_string, app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 

        // dont have to read it from the file since the game had to keep track of it 
        if (app->game.best_time < INFINITY) {
            char best_time_string[8]; sprintf(best_time_string, "%.1f", app->game.best_time); 
            init_text(&app->win_menu.best_time, best_time_string, app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 
        }
        else app->win_menu.best_time = NULL; 
    }
    else if (app->state == InLoseMenu) { // initialize tmeporary lose menu compnents (level name, timer, and best time)
        char path[32]; 
        if (app->last_played_level_type == Normal) sprintf(path, "assets/level%d.lvl", app->last_played_level_num); 
        else if (app->last_played_level_type == Custom) sprintf(path, "assets/custom_level%d.lvl", app->last_played_level_num); 
        FILE *level_file = fopen(path, "rb"); 
        char description[32]; fread(description, 32, 1, level_file); 
        fclose(level_file); 
        init_text(&app->lose_menu.level_description, description, app->main_menu.font, (SDL_Color){46, 16, 107, 255}, app->renderer); 
        
        char timer_string[8]; sprintf(timer_string, "%.1f", app->game.timer); 
        init_text(&app->lose_menu.timer, timer_string, app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 

        // dont have to read it from the file since the game had to keep track of it anyway
        if (app->game.best_time < INFINITY) {
            char best_time_string[8]; sprintf(best_time_string, "%.1f", app->game.best_time); 
            init_text(&app->lose_menu.best_time, best_time_string, app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 
        }
        else app->lose_menu.best_time = NULL; 
    }
    else if (app->state == InEditor) {
        char path[32]; 
        sprintf(path, "assets/custom_level%d.lvl", app->last_edited_level_num); 
        if (app->last_edited_level_num < app->num_custom_levels) { // existing level 
            FILE *level_file = fopen(path, "rb"); 
            fread(app->editor.description, sizeof(app->editor.description), 1, level_file); 
            fseek(level_file, sizeof(float), SEEK_CUR); 
            fread(&app->editor.spawn_x, sizeof(float), 1, level_file); 
            fread(&app->editor.spawn_y, sizeof(float), 1, level_file); 
            fread(&app->editor.spawn_rot, sizeof(float), 1, level_file); 
            fread(&app->editor.map, sizeof(enum Tile), MAP_W * MAP_H, level_file); 
            fclose(level_file);
        }
        else { // new level 
            sprintf(app->editor.description, "Unnamed"); 
            app->editor.spawn_x = 0.5; 
            app->editor.spawn_y = 0.5; 
            app->editor.spawn_rot = M_PI/2; 
            for (unsigned row = 0; row < MAP_H; ++row) for (unsigned col = 0; col < MAP_W; ++col ) app->editor.map[row][col] = None; 
        }

        app->editor.camera_x = app->editor.spawn_x; 
        app->editor.camera_y = app->editor.spawn_y; 
        app->editor.render_scale = 32; 
        app->editor.tool = DrawTiles; 
        app->editor.drawing_tiles = 0; 
        app->editor.selected_draw_tile = Solid; 
        init_text(&app->editor.description_texture, app->editor.description, app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 
    }
    else if (app->state == InPauseMenu) {
        char path[32]; 
        if (app->last_played_level_type == Normal) sprintf(path, "assets/level%d.lvl", app->last_played_level_num); 
        else if (app->last_played_level_type == Custom) sprintf(path, "assets/custom_level%d.lvl", app->last_played_level_num); 
        FILE *level_file = fopen(path, "rb"); 
        char description[32]; fread(description, 32, 1, level_file); 
        fclose(level_file); 
        init_text(&app->pause_menu.level_description, description, app->main_menu.font, (SDL_Color){46, 16, 107, 255}, app->renderer); 
        
        char timer_string[8]; sprintf(timer_string, "%.1f", app->game.timer); 
        init_text(&app->pause_menu.timer, timer_string, app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 

        // dont have to read it from the file since the game had to keep track of it anyway
        if (app->game.best_time < INFINITY) {
            char best_time_string[8]; sprintf(best_time_string, "%.1f", app->game.best_time); 
            init_text(&app->pause_menu.best_time, best_time_string, app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 
        }
        else app->pause_menu.best_time = NULL; 
    }
}
void cleanup_app_state(struct App *app) {
    if (app->state == InMainMenu) {
        for (int i = 0; i < 8; ++i) {
            if (app->main_menu.level_previews[i].description != NULL) SDL_DestroyTexture(app->main_menu.level_previews[i].description);
            if (app->main_menu.level_previews[i].best_time != NULL) SDL_DestroyTexture(app->main_menu.level_previews[i].best_time);
        }
    }
    else if (app->state == InCustomMenu) {
        for (int i = 0; i < 8; ++i) {
            if (app->custom_menu.level_previews[i].description != NULL) SDL_DestroyTexture(app->custom_menu.level_previews[i].description);
            if (app->custom_menu.level_previews[i].best_time != NULL) SDL_DestroyTexture(app->custom_menu.level_previews[i].best_time);
        }
    }
    else if (app->state == InGame) { // game state parts timer texture and level description need to be destroyed, also best time and num unlocked may need to be updated
        SDL_DestroyTexture(app->game.timer_texture); 
        SDL_DestroyTexture(app->game.level_description); 

        if (app->game.player.state == Winning && app->game.timer < app->game.best_time) {
            char path[32]; 
            if (app->last_played_level_type == Normal) sprintf(path, "assets/level%d.lvl", app->last_played_level_num); 
            else if (app->last_played_level_type == Custom) sprintf(path, "assets/custom_level%d.lvl", app->last_played_level_num); 

            FILE *level_file = fopen(path, "r+b"); 
            fseek(level_file, 32, SEEK_SET); 
            fwrite(&app->game.timer, sizeof(app->game.timer), 1, level_file); 
            fclose(level_file); 
        }
        
        if (app->game.player.state == Winning && app->last_played_level_type == Normal && app->last_played_level_num == app->num_unlocked_levels - 1) ++app->num_unlocked_levels; 
    }
    else if (app->state == InWinMenu) {
        SDL_DestroyTexture(app->win_menu.level_description); 
        SDL_DestroyTexture(app->win_menu.timer); 
        if (app->win_menu.best_time != NULL) SDL_DestroyTexture(app->win_menu.best_time); 
    }
    else if (app->state == InLoseMenu) {
        SDL_DestroyTexture(app->lose_menu.level_description); 
        SDL_DestroyTexture(app->lose_menu.timer); 
        if (app->lose_menu.best_time != NULL) SDL_DestroyTexture(app->lose_menu.best_time); 
    }
    else if (app->state == InEditor) {
        char path[32]; 
        sprintf(path, "assets/custom_level%d.lvl", app->last_edited_level_num); 
        FILE *level_file = fopen(path, "wb");
        fwrite(app->editor.description, sizeof(app->editor.description), 1, level_file); 
        float new_best_time = INFINITY; fwrite(&new_best_time, sizeof(new_best_time), 1, level_file); 
        fwrite(&app->editor.spawn_x, sizeof(float), 1, level_file); 
        fwrite(&app->editor.spawn_y, sizeof(float), 1, level_file); 
        fwrite(&app->editor.spawn_rot, sizeof(float), 1, level_file); 
        fwrite(&app->editor.map, sizeof(enum Tile), MAP_W * MAP_H, level_file); 
        fclose(level_file);

        if (app->last_edited_level_num >= app->num_custom_levels) ++app->num_custom_levels; 

        SDL_DestroyTexture(app->editor.description_texture); 
    }
    else if (app->state == InPauseMenu) {
        SDL_DestroyTexture(app->pause_menu.level_description); 
        SDL_DestroyTexture(app->pause_menu.timer); 
        if (app->pause_menu.best_time != NULL) SDL_DestroyTexture(app->pause_menu.best_time); 
    }
}

void init_app(struct App *app) {
    // app componenets 
    SDL_Init(SDL_INIT_VIDEO); 
    IMG_Init(IMG_INIT_PNG); 
    TTF_Init(); 
    app->window = SDL_CreateWindow("Rolleron", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN); 
    app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC); 
    app->running = 1; 
    app->state = InMainMenu; 
    app->next_state = InMainMenu; 

    FILE *num_unlocked_file = fopen("assets/num_unlocked_levels.dat", "rb"); 
    fread(&app->num_unlocked_levels, sizeof(app->num_unlocked_levels), 1, num_unlocked_file); 
    fclose(num_unlocked_file); 
    FILE *num_custom_file = fopen("assets/num_custom_levels.dat", "rb"); 
    fread(&app->num_custom_levels, sizeof(app->num_custom_levels), 1, num_custom_file); 
    fclose(num_custom_file); 

    // permanent main menu components
    app->main_menu.font = TTF_OpenFont("assets/font.ttf", 32); 
    init_text(&app->main_menu.title, "Rolleron", app->main_menu.font, (SDL_Color){46, 16, 107, 255}, app->renderer);
    init_text(&app->main_menu.custom_button_text, "Custom Menu", app->main_menu.font, (SDL_Color){0, 180, 180, 0}, app->renderer); 
    app->main_menu.level_page = (app->num_unlocked_levels - 1) / 8; 
    init_app_state(app); // temporary main menu components (since it is the first state)

    // permanent custom menu componenets
    init_text(&app->custom_menu.title, "Custom Levels", app->main_menu.font, (SDL_Color){46, 16, 107, 255}, app->renderer);
    init_text(&app->custom_menu.main_button_text, "Main Menu", app->main_menu.font, (SDL_Color){0, 180, 180, 0}, app->renderer); 
    init_text(&app->custom_menu.new_level_button_text, "New Level", app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 
    app->custom_menu.level_page = 0; 
    init_text(&app->custom_menu.edit_symbol, "...", app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 
    
    // permanent game components 
    app->game.tiles_texture = IMG_LoadTexture(app->renderer, "assets/tiles.png"); 
    app->game.player.base_texture = IMG_LoadTexture(app->renderer, "assets/space_ship.png"); 
    app->game.player.thrusters_texture = IMG_LoadTexture(app->renderer, "assets/thrusters.png"); 
    app->game.player.sparks_texture = IMG_LoadTexture(app->renderer, "assets/sparks.png"); 
    app->game.player.exploding_texture = IMG_LoadTexture(app->renderer, "assets/explosion.png");

    // permanent win menu components 
    init_text(&app->win_menu.completed, "Completed!!!", app->main_menu.font, (SDL_Color){46, 16, 107, 255}, app->renderer); 
    init_text(&app->win_menu.new_record, "New Record!", app->main_menu.font, (SDL_Color){46, 16, 107, 255}, app->renderer); 
    init_text(&app->win_menu.timer_label, "Timer:", app->main_menu.font, (SDL_Color){46, 16, 107, 255}, app->renderer); 
    init_text(&app->win_menu.best_time_label, "Best Time:", app->main_menu.font, (SDL_Color){46, 16, 107, 255}, app->renderer); 
    init_text(&app->win_menu.menu_button_text, "Back To Menu", app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 
    init_text(&app->win_menu.next_button_text, "Next Level", app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 

    // permanent lose menu components
    init_text(&app->lose_menu.destroyed, "Destroyed!!!", app->main_menu.font, (SDL_Color){46, 16, 107, 255}, app->renderer); 
    init_text(&app->lose_menu.timer_label, "Lose Time:", app->main_menu.font, (SDL_Color){46, 16, 107, 255}, app->renderer); 
    init_text(&app->lose_menu.best_time_label, "Best Win Time:", app->main_menu.font, (SDL_Color){46, 16, 107, 255}, app->renderer); 
    init_text(&app->lose_menu.menu_button_text, "Back To Menu", app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 
    init_text(&app->lose_menu.retry_button_text, "Retry Level", app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 

    // permanent editor components 
    init_text(&app->editor.leave_button_text, "X", app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 
    init_text(&app->editor.rename_button_text, "Rnm", app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 

    // permanent pause menu components
    init_text(&app->pause_menu.paused, "Paused!!!", app->main_menu.font, (SDL_Color){46, 16, 107, 255}, app->renderer); 
    init_text(&app->pause_menu.timer_label, "Pause Time:", app->main_menu.font, (SDL_Color){46, 16, 107, 255}, app->renderer); 
    init_text(&app->pause_menu.best_time_label, "Best Completed Time:", app->main_menu.font, (SDL_Color){46, 16, 107, 255}, app->renderer); 
    init_text(&app->pause_menu.menu_button_text, "Back To Menu", app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 
    init_text(&app->pause_menu.resume_button_text, "Resume Level", app->main_menu.font, (SDL_Color){0, 180, 180, 255}, app->renderer); 
}
void cleanup_app(struct App *app) {
    // permanent pause menu components 
    SDL_DestroyTexture(app->pause_menu.resume_button_text); 
    SDL_DestroyTexture(app->pause_menu.menu_button_text); 
    SDL_DestroyTexture(app->pause_menu.best_time_label); 
    SDL_DestroyTexture(app->pause_menu.timer_label); 
    SDL_DestroyTexture(app->pause_menu.paused); 

    // permanent editor components 
    SDL_DestroyTexture(app->editor.rename_button_text); 
    SDL_DestroyTexture(app->editor.leave_button_text); 

    // permanent lose menu components
    SDL_DestroyTexture(app->lose_menu.retry_button_text); 
    SDL_DestroyTexture(app->lose_menu.menu_button_text); 
    SDL_DestroyTexture(app->lose_menu.best_time_label); 
    SDL_DestroyTexture(app->lose_menu.timer_label); 
    SDL_DestroyTexture(app->lose_menu.destroyed); 

    // permanent win menu components 
    SDL_DestroyTexture(app->win_menu.next_button_text); 
    SDL_DestroyTexture(app->win_menu.menu_button_text); 
    SDL_DestroyTexture(app->win_menu.best_time_label); 
    SDL_DestroyTexture(app->win_menu.timer_label); 
    SDL_DestroyTexture(app->win_menu.new_record); 
    SDL_DestroyTexture(app->win_menu.completed); 

    // permanet game components 
    SDL_DestroyTexture(app->game.player.exploding_texture); 
    SDL_DestroyTexture(app->game.player.sparks_texture); 
    SDL_DestroyTexture(app->game.player.thrusters_texture); 
    SDL_DestroyTexture(app->game.player.base_texture); 
    SDL_DestroyTexture(app->game.tiles_texture); 

    // permanent custom menu components
    SDL_DestroyTexture(app->custom_menu.edit_symbol); 
    SDL_DestroyTexture(app->custom_menu.new_level_button_text); 
    SDL_DestroyTexture(app->custom_menu.main_button_text); 
    SDL_DestroyTexture(app->custom_menu.title); 

    // permanent main menu components
    SDL_DestroyTexture(app->main_menu.custom_button_text); 
    SDL_DestroyTexture(app->main_menu.title); 
    TTF_CloseFont(app->main_menu.font);

    // app componenets
    cleanup_app_state(app); // clean up temporary components from last state, whatever that may be

    FILE *num_unlocked_file = fopen("assets/num_unlocked_levels.dat", "wb"); 
    fwrite(&app->num_unlocked_levels, sizeof(app->num_unlocked_levels), 1, num_unlocked_file); 
    fclose(num_unlocked_file); 

    FILE *num_custom_file = fopen("assets/num_custom_levels.dat", "wb"); 
    fwrite(&app->num_custom_levels, sizeof(app->num_custom_levels), 1, num_custom_file); 
    fclose(num_custom_file); 
    
    SDL_DestroyWindow(app->window); 
    SDL_DestroyRenderer(app->renderer); 
    TTF_Quit(); 
    IMG_Quit(); 
    SDL_Quit(); 
}

void handle_main_menu_event(struct MainMenu *main_menu, SDL_Event event, enum AppState *next_app_state, SDL_Renderer *renderer, unsigned num_unlocked_levels, unsigned *last_played_level_num, enum LevelType *last_played_level_type) {
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        int row = event.button.y/64, col = event.button.x/64; 

        if (row == 1 && col >= 1 && col <= 5) *next_app_state = InCustomMenu; 

        else if ((row == 6 && col == 0 && main_menu->level_page > 0) || (row == 6 && col == 19 && (main_menu->level_page + 1) * 8 < 10)) {
            main_menu->level_page = col == 0 ? main_menu->level_page - 1 : main_menu->level_page + 1; 
            for (int i = 0; i < 8; ++i) {
                // destroy existing textures and then create the new ones (destruction + init are stuck right onto each other since this is all in the same state, while in the actuall designated functions, they are designed to destroy one and create another )
                if (main_menu->level_previews[i].description != NULL) SDL_DestroyTexture(main_menu->level_previews[i].description);
                if (main_menu->level_previews[i].best_time != NULL) SDL_DestroyTexture(main_menu->level_previews[i].best_time);

                if (main_menu->level_page * 8 + i < 10) {
                    char path[32]; sprintf(path, "assets/level%d.lvl", main_menu->level_page * 8 + i);
                    FILE *level_file = fopen(path, "rb"); 
                    char description[32]; fread(description, 32, 1, level_file); 
                    float best_time; fread(&best_time, sizeof(float), 1, level_file); 
                    fclose(level_file); 

                    init_text(&main_menu->level_previews[i].description, description, main_menu->font, (SDL_Color){0, 180, 180, 255}, renderer); 

                    if (best_time < INFINITY) {
                        char best_time_string[8]; sprintf(best_time_string, "%.1f", best_time); 
                        init_text(&main_menu->level_previews[i].best_time, best_time_string, main_menu->font, (SDL_Color){0, 180, 180, 255}, renderer); 
                    } 
                    else main_menu->level_previews[i].best_time = NULL; 
                }
                else {
                    main_menu->level_previews[i].description = NULL; 
                    main_menu->level_previews[i].best_time = NULL; 
                }
            }
        } 

        else if ((row == 3 || row == 5 || row == 7 || row == 9) && ((col >= 1 && col <= 8) || (col >= 11 && col <= 18)) && (main_menu->level_page * 8 + (row - 3) / 2 * 2 + col / 10 < num_unlocked_levels)) {
            *last_played_level_num = main_menu->level_page * 8 + (row - 3) / 2 * 2 + col / 10; 
            *last_played_level_type = Normal; 
            *next_app_state = InGame; 
        }
    }
}
void render_main_menu(struct MainMenu *main_menu, SDL_Renderer *renderer, unsigned window_w, unsigned window_h, SDL_Texture *tiles, unsigned num_unlocked_levels) {
    for (unsigned row = 0; row < window_h/64 + 1; ++row) {
        for (unsigned col = 0; col < window_w/64; ++col) {
            enum Tile tile_at_pos = (row == 1 && col >= 1 && col <= 5) ? Solid : (row == 6 && col == 0 && main_menu->level_page > 0)? LeftForce : (row == 6 && col == 19 && (main_menu->level_page + 1) * 8 < 10) ? RightForce : ((row == 3 || row == 5 || row == 7 || row == 9) && ((col >= 1 && col <= 8) || (col >= 11 && col <= 18)) && (main_menu->level_page * 8 + (row - 3) / 2 * 2 + col / 10 < 10)) ? ((main_menu->level_page * 8 + (row - 3) / 2 * 2 + col / 10 < num_unlocked_levels) ? Solid : Friction) : None;
            SDL_RenderCopy(renderer, tiles, &(SDL_Rect){tile_at_pos * 16, 0, 16, 16}, &(SDL_Rect){col * 64, row * 64, 64, 64}); 
        }
    }

    render_text(main_menu->title, 64 * 10, 32, Middle, 2.5, renderer); 
    render_text(main_menu->custom_button_text, 64 * 3 + 32, 96, Middle, 1.25, renderer); 

    for (int row = 3; row <= 9; row += 2) {
        for (int col = 1; col <= 11; col += 10) {
            if (main_menu->level_previews[(row - 3) / 2 * 2 + col / 10].description != NULL) render_text(main_menu->level_previews[(row - 3) / 2 * 2 + col / 10].description, col * 64, row * 64 + 32, MiddleLeft, 1, renderer); 
            if (main_menu->level_previews[(row - 3) / 2 * 2 + col / 10].best_time != NULL) render_text(main_menu->level_previews[(row - 3) / 2 * 2 + col / 10].best_time, (col + 8) * 64, row * 64 + 32, MiddleRight, 1.25, renderer); 
        }
    }
}

void handle_custom_menu_event(struct CustomMenu *custom_menu, SDL_Event event, enum AppState *next_app_state, SDL_Renderer *renderer, TTF_Font *main_menu_font, unsigned num_custom_levels, unsigned *last_played_level_num, enum LevelType *last_played_level_type, unsigned *last_edited_level_num) {
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        int row = event.button.y/64, col = event.button.x/64; 
        
        if (row == 1 && col >= 1 && col <= 5) *next_app_state = InMainMenu; 

        else if ((row == 6 && col == 0 && custom_menu->level_page > 0) || (row == 6 && col == 19 && (custom_menu->level_page + 1) * 8 < num_custom_levels)) {
            custom_menu->level_page = col == 0 ? custom_menu->level_page - 1 : custom_menu->level_page + 1;
            for (int i = 0; i < 8; ++i) {
                // destroy existing textures and then create the new ones (destruction + init are stuck right onto each other since this is all in the same state, while in the actuall designated functions, they are designed to destroy one and create another )
                if (custom_menu->level_previews[i].description != NULL) SDL_DestroyTexture(custom_menu->level_previews[i].description);
                if (custom_menu->level_previews[i].best_time != NULL) SDL_DestroyTexture(custom_menu->level_previews[i].best_time);

                if (custom_menu->level_page * 8 + i < num_custom_levels) {
                    char path[32]; sprintf(path, "assets/custom_level%d.lvl", custom_menu->level_page * 8 + i);
                    FILE *level_file = fopen(path, "rb"); 
                    char description[32]; fread(description, 32, 1, level_file); 
                    float best_time; fread(&best_time, sizeof(float), 1, level_file); 
                    fclose(level_file); 

                    init_text(&custom_menu->level_previews[i].description, description, main_menu_font, (SDL_Color){0, 180, 180, 255}, renderer); 

                    if (best_time < INFINITY) {
                        char best_time_string[8]; sprintf(best_time_string, "%.1f", best_time); 
                        init_text(&custom_menu->level_previews[i].best_time, best_time_string, main_menu_font, (SDL_Color){0, 180, 180, 255}, renderer); 
                    } 
                    else custom_menu->level_previews[i].best_time = NULL; 
                }
                else {
                    custom_menu->level_previews[i].description = NULL; 
                    custom_menu->level_previews[i].best_time = NULL; 
                }
            }
        }

        else if ((row == 3 || row == 5 || row == 7 || row == 9) && ((col >= 1 && col <= 8) || (col >= 11 && col <= 18)) && (custom_menu->level_page * 8 + (row - 3) / 2 * 2 + col / 10 < num_custom_levels)) {
            *last_played_level_num = custom_menu->level_page * 8 + (row - 3) / 2 * 2 + col / 10; 
            *last_played_level_type = Custom; 
            *next_app_state = InGame; 
        } 

        else if ((row == 3 || row == 5 || row == 7 || row == 9) && (col == 0 || col == 10) && (custom_menu->level_page * 8 + (row - 3) / 2 * 2 + col / 10 < num_custom_levels)) {
            *last_edited_level_num = custom_menu->level_page * 8 + (row - 3) / 2 * 2 + col / 10; 
            *next_app_state = InEditor; 
        }

        else if (row == 1 && 14 <= col && col <= 18) {
            *last_edited_level_num = num_custom_levels;
            *next_app_state = InEditor; 
        }
    }
}
void render_custom_menu(struct CustomMenu *custom_menu, SDL_Renderer *renderer, unsigned window_w, unsigned window_h, SDL_Texture *tiles, unsigned num_custom_levels) {
    for (unsigned row = 0; row < window_h/64 + 1; ++row) {
        for (unsigned col = 0; col < window_w/64; ++col) {
            enum Tile tile_at_pos = (row == 1 && col >= 1 && col <= 5) ? Solid: (row == 6 && col == 0 && custom_menu->level_page > 0)? LeftForce : (row == 6 && col == 19 && (custom_menu->level_page + 1) * 8 < num_custom_levels) ? RightForce : (row == 1 && 14 <= col && col <= 18)? Solid : ((row == 3 || row == 5 || row == 7 || row == 9) && custom_menu->level_page * 8 + (row - 3) / 2 * 2 + col / 10 < num_custom_levels) ? (col == 0 || col == 10) ? Gravity : ((col >= 1 && col <= 8) || (col >= 11 && col <= 18))? Solid: None: None;  
            SDL_RenderCopy(renderer, tiles, &(SDL_Rect){tile_at_pos * 16, 0, 16, 16}, &(SDL_Rect){col * 64, row * 64, 64, 64}); 
        }
    }

    render_text(custom_menu->title, 64 * 10, 32, Middle, 2.5, renderer); 
    render_text(custom_menu->main_button_text, 64 * 3 + 32, 96, Middle, 1.25, renderer); 
    render_text(custom_menu->new_level_button_text, 16 * 64 + 32, 96, Middle, 1.25, renderer); 

    for (int row = 3; row <= 9; row += 2)  {
        for (int col = 1; col <= 11; col += 10) {
            if (custom_menu->level_previews[(row - 3) / 2 * 2 + col / 10].description != NULL) render_text(custom_menu->level_previews[(row - 3) / 2 * 2 + col / 10].description, col * 64, row * 64 + 32, MiddleLeft, 1, renderer);
            if (custom_menu->level_page * 8 + (row - 3) / 2 * 2 + col / 10 < num_custom_levels) render_text(custom_menu->edit_symbol, (col - 1) * 64 + 32, row * 64 + 18, Middle, 2, renderer); 
            if (custom_menu->level_previews[(row - 3) / 2 * 2 + col / 10].best_time != NULL) render_text(custom_menu->level_previews[(row - 3) / 2 * 2 + col / 10].best_time, (col + 8) * 64, row * 64 + 32, MiddleRight, 1.25, renderer); 
        }
    }
}

void handle_game_event(struct Game *game, SDL_Event event, enum AppState *next_app_state) {
    if (event.type == SDL_KEYDOWN) {
        if ((event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_a) && game->player.state == Playing) game->player.left_thruster = 1;
        else if ((event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_d) && game->player.state == Playing) game->player.right_thruster = 1;  
        else if (event.key.keysym.sym == SDLK_ESCAPE) *next_app_state = InPauseMenu; 
    }
    else if (event.type == SDL_KEYUP) {
        if ((event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_a) && game->player.state == Playing) game->player.left_thruster = 0; 
        else if ((event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_d) && game->player.state == Playing) game->player.right_thruster = 0;   
    }
}
void update_game(struct Game *game, float delta_time, TTF_Font *menu_font, SDL_Renderer *renderer, enum AppState *next_app_state) {
    if (game->player.state == Playing) {
        // UPDATE TIMER (do it before player so that if there is a win, the player knows the up to date value of the timer when that win took place)
        if (game->player.vel_x != 0 || game->player.vel_y != 0 || game->player.rot_vel != 0) { 
            game->timer += delta_time; 
            game->timer_animation_timer += delta_time; 
            if (game->timer_animation_timer > 0.1) {
                game->timer_animation_timer -= 0.1; 
                SDL_DestroyTexture(game->timer_texture); 
                char timer_string[8]; sprintf(timer_string, "%.1f", game->timer); 
                init_text(&game->timer_texture, timer_string, menu_font, game->timer < game->best_time ? (SDL_Color){0, 180, 0, 255}: (SDL_Color){180, 0, 0, 255}, renderer); 
            }
        }

        // UPDATE PLAYER
        float net_force_x = 0, net_force_y = 0; 
        float net_torque = 0; 

        // radiator effects
        for (int row = floorf(game->player.y - 8); row <= floorf(game->player.y + 8); ++row) {
            for (int col = floorf(game->player.x - 8); col <= floorf(game->player.x + 8); ++col) {
                float dist_squared = (powf(col + 0.5 - game->player.x, 2) + powf(row + 0.5 - game->player.y, 2));
                if (row >= 0 && row < MAP_H && col >= 0 && col < MAP_W && dist_squared <= 64) {
                    if (game->map[row][col] == Gravity) {
                        float dist = sqrt(dist_squared); 
                        net_force_x += (col + 0.5 - game->player.x)/dist * 3/dist_squared; 
                        net_force_y += (row + 0.5 - game->player.y)/dist * 3/dist_squared; 
                    }
                    else if (game->map[row][col] == AntiGravity) {
                        float dist = sqrt(dist_squared); 
                        net_force_x += (game->player.x - (col + 0.5))/dist * 3/dist_squared; 
                        net_force_y += (game->player.y - (row + 0.5))/dist * 3/dist_squared; 
                    }
                }
            }
        }

        // contact effects
        struct {int row, col;} corner_tile_coords[3] = { 
            {floorf(game->player.y + sinf(game->player.rot) * 0.375),floorf(game->player.x + cosf(game->player.rot) * 0.375)},
            {floorf(game->player.y + (-0.125 * sinf(game->player.rot) + 0.25 * cosf(game->player.rot))),floorf(game->player.x + (-0.125 * cosf(game->player.rot) - 0.25 * sinf(game->player.rot)))},
            {floorf(game->player.y + (-0.125 * sinf(game->player.rot) + -0.25 * cosf(game->player.rot))), floorf(game->player.x + (-0.125 * cosf(game->player.rot) - -0.25 * sinf(game->player.rot)))}
        }; 
        if (corner_tile_coords[0].col >= 0 && corner_tile_coords[0].col < MAP_W && corner_tile_coords[0].row >= 0 && corner_tile_coords[0].row < MAP_H && corner_tile_coords[1].col >= 0 && corner_tile_coords[1].col < MAP_W && corner_tile_coords[1].row >= 0 && corner_tile_coords[1].row < MAP_H && corner_tile_coords[2].col >= 0 && corner_tile_coords[2].col < MAP_W && corner_tile_coords[2].row >= 0 && corner_tile_coords[2].row < MAP_H ) { 
            if (game->map[corner_tile_coords[0].row][corner_tile_coords[0].col] == Solid || game->map[corner_tile_coords[1].row][corner_tile_coords[1].col] == Solid || game->map[corner_tile_coords[2].row][corner_tile_coords[2].col] == Solid || game->map[corner_tile_coords[0].row][corner_tile_coords[0].col] == Gravity || game->map[corner_tile_coords[1].row][corner_tile_coords[1].col] == Gravity || game->map[corner_tile_coords[2].row][corner_tile_coords[2].col] == Gravity || game->map[corner_tile_coords[0].row][corner_tile_coords[0].col] == AntiGravity || game->map[corner_tile_coords[1].row][corner_tile_coords[1].col] == AntiGravity || game->map[corner_tile_coords[2].row][corner_tile_coords[2].col] == AntiGravity) {
                game->player.state = Exploding; 
                game->player.current_frame = 0; 
            }
            if (game->map[corner_tile_coords[0].row][corner_tile_coords[0].col] == Win || game->map[corner_tile_coords[1].row][corner_tile_coords[1].col] == Win || game->map[corner_tile_coords[2].row][corner_tile_coords[2].col] == Win) {
                SDL_DestroyTexture(game->timer_texture); 
                char timer_string[8]; sprintf(timer_string, "%.1f", game->timer); 
                init_text(&game->timer_texture, timer_string, menu_font, game->timer < game->best_time ? (SDL_Color){0, 180, 0, 255}: (SDL_Color){180, 0, 0, 255}, renderer); 
                game->player.state = Winning; 
            }

            if (game->map[corner_tile_coords[0].row][corner_tile_coords[0].col] == Friction || game->map[corner_tile_coords[1].row][corner_tile_coords[1].col] == Friction || game->map[corner_tile_coords[2].row][corner_tile_coords[2].col] == Friction) {
                net_force_x += -0.75 * game->player.vel_x; 
                net_force_y += -0.75 * game->player.vel_y; 
                net_torque += -0.075 * game->player.rot_vel; 
            }
            if (game->map[corner_tile_coords[0].row][corner_tile_coords[0].col] == AntiFriction || game->map[corner_tile_coords[1].row][corner_tile_coords[1].col] == AntiFriction || game->map[corner_tile_coords[2].row][corner_tile_coords[2].col] == AntiFriction) {
                net_force_x += 0.75 * game->player.vel_x; 
                net_force_y += 0.75 * game->player.vel_y; 
                net_torque += 0.075 * game->player.rot_vel; 
            }

            if (game->map[corner_tile_coords[0].row][corner_tile_coords[0].col] == ThrustersOn || game->map[corner_tile_coords[1].row][corner_tile_coords[1].col] == ThrustersOn || game->map[corner_tile_coords[2].row][corner_tile_coords[2].col] == ThrustersOn) {
                if (game->player.right_thruster == 0) game->player.right_thruster = 1; 
                if (game->player.left_thruster == 0) game->player.left_thruster = 1;    
                game->player.animation_timer += delta_time; 
                if (game->player.animation_timer >= 0.2) {
                    game->player.animation_timer -= 0.2; 
                    game->player.current_frame = (game->player.current_frame + 1) % 4; 
                }
            }
            if (game->map[corner_tile_coords[0].row][corner_tile_coords[0].col] == ThrustersOff || game->map[corner_tile_coords[1].row][corner_tile_coords[1].col] == ThrustersOff || game->map[corner_tile_coords[2].row][corner_tile_coords[2].col] == ThrustersOff) {
                if (game->player.right_thruster == 1) game->player.right_thruster = 0; 
                if (game->player.left_thruster == 1) game->player.left_thruster = 0; 
                game->player.animation_timer += delta_time; 
                if (game->player.animation_timer >= 0.2) {
                    game->player.animation_timer -= 0.2; 
                    game->player.current_frame = (game->player.current_frame + 1) % 4; 
                } 
            }

            if (game->map[corner_tile_coords[0].row][corner_tile_coords[0].col] == StrongerThrusters || game->map[corner_tile_coords[1].row][corner_tile_coords[1].col] == StrongerThrusters  || game->map[corner_tile_coords[2].row][corner_tile_coords[2].col] == StrongerThrusters) {
                if (game->player.right_thruster) {
                    net_force_x += cosf(game->player.rot) * 0.75; 
                    net_force_y += sinf(game->player.rot) * 0.75;  
                    net_torque += 0.25 * 0.75; 
                }
                if (game->player.left_thruster) {
                    net_force_x += cosf(game->player.rot) * 0.75; 
                    net_force_y += sinf(game->player.rot) * 0.75; 
                    net_torque += -0.25 * 0.75; 
                }
            }
            if (game->map[corner_tile_coords[0].row][corner_tile_coords[0].col] == WeakerThrusters || game->map[corner_tile_coords[1].row][corner_tile_coords[1].col] == WeakerThrusters  || game->map[corner_tile_coords[2].row][corner_tile_coords[2].col] == WeakerThrusters) {
                if (game->player.right_thruster) {
                    net_force_x += cosf(game->player.rot) * -0.75; 
                    net_force_y += sinf(game->player.rot) * -0.75;  
                    net_torque += 0.25 * -0.75; 
                }
                if (game->player.left_thruster) {
                    net_force_x += cosf(game->player.rot) * -0.75; 
                    net_force_y += sinf(game->player.rot) * -0.75; 
                    net_torque += -0.25 * -0.75; 
                }
            }

            if (game->map[corner_tile_coords[0].row][corner_tile_coords[0].col] == DownForce || game->map[corner_tile_coords[1].row][corner_tile_coords[1].col] == DownForce  || game->map[corner_tile_coords[2].row][corner_tile_coords[2].col] == DownForce) net_force_y -= 1.75; 
            if (game->map[corner_tile_coords[0].row][corner_tile_coords[0].col] == UpForce || game->map[corner_tile_coords[1].row][corner_tile_coords[1].col] == UpForce  || game->map[corner_tile_coords[2].row][corner_tile_coords[2].col] == UpForce) net_force_y += 1.75; 
            if (game->map[corner_tile_coords[0].row][corner_tile_coords[0].col] == LeftForce || game->map[corner_tile_coords[1].row][corner_tile_coords[1].col] == LeftForce  || game->map[corner_tile_coords[2].row][corner_tile_coords[2].col] == LeftForce) net_force_x -= 1.75; 
            if (game->map[corner_tile_coords[0].row][corner_tile_coords[0].col] == RightForce || game->map[corner_tile_coords[1].row][corner_tile_coords[1].col] == RightForce  || game->map[corner_tile_coords[2].row][corner_tile_coords[2].col] == RightForce) net_force_x += 1.75; 
            
            if ((game->map[corner_tile_coords[0].row][corner_tile_coords[0].col] == CounterClockwiseTorque || game->map[corner_tile_coords[1].row][corner_tile_coords[1].col] == CounterClockwiseTorque  || game->map[corner_tile_coords[2].row][corner_tile_coords[2].col] == CounterClockwiseTorque)) net_torque += 0.2; 
            if ((game->map[corner_tile_coords[0].row][corner_tile_coords[0].col] == ClockwiseTorque || game->map[corner_tile_coords[1].row][corner_tile_coords[1].col] == ClockwiseTorque  || game->map[corner_tile_coords[2].row][corner_tile_coords[2].col] == ClockwiseTorque)) net_torque -= 0.2; 
        }
        else {
            game->player.state = Exploding; 
            game->player.current_frame = 0; 
        }

        // thrusters
        if (game->player.right_thruster) {
            net_force_x += cosf(game->player.rot); 
            net_force_y += sinf(game->player.rot);  
            net_torque += 0.25; 
        }
        if (game->player.left_thruster) {
            net_force_x += cosf(game->player.rot); 
            net_force_y += sinf(game->player.rot); 
            net_torque += -0.25; 
        }

        // apply force and torque 
        game->player.vel_x += net_force_x / 1 * delta_time; game->player.vel_y += net_force_y / 1 * delta_time; 
        game->player.x += game->player.vel_x * delta_time; game->player.y += game->player.vel_y * delta_time; 
        game->player.rot_vel += net_torque / 0.05 * delta_time; 
        game->player.rot += game->player.rot_vel * delta_time; 
    }
    else if (game->player.state == Exploding) {
        game->player.animation_timer += delta_time; 
        if (game->player.animation_timer >= 0.1) {
            game->player.animation_timer -= 0.1; 
            game->player.current_frame += 1; 
            if (game->player.current_frame >= 9) *next_app_state = InLoseMenu; 
        }
    }
    else if (game->player.state == Winning) {
        game->player.vel_x *= powf(0.25, delta_time); game->player.vel_y *= powf(0.25, delta_time); 
        game->player.x += game->player.vel_x * delta_time; game->player.y += game->player.vel_y * delta_time; 
        game->player.rot_vel *= powf(0.025, delta_time); 
        game->player.rot += game->player.rot_vel * delta_time; 
        if (game->render_scale >= 64) game->render_scale -= delta_time * 15; 
        if (sqrtf(game->player.vel_x * game->player.vel_x + game->player.vel_y * game->player.vel_y) < 0.1 && game->player.rot_vel < 0.1) *next_app_state = InWinMenu; 
    }   
}
void render_game(struct Game *game, SDL_Renderer *renderer, unsigned window_w, unsigned window_h) {
    // map 
    for (int row = floorf(game->player.y - window_h/2.0/game->render_scale); row <= floorf(game->player.y + window_h/2.0/game->render_scale); ++row) {
        for (int col = floorf(game->player.x - window_w/2.0/game->render_scale); col <= floorf(game->player.x + window_w/2.0/game->render_scale); ++col) {
            SDL_RenderCopy(renderer, game->tiles_texture, &(SDL_Rect){row >= 0 && row < MAP_H && col >= 0 && col < MAP_W ? game->map[row][col] * 16: 16, 0, 16, 16}, &(SDL_Rect){(col - game->player.x) * game->render_scale + window_w/2.0, window_h/2.0 - (row - game->player.y) * game->render_scale - game->render_scale, game->render_scale, game->render_scale}); 
        }
    } 

    // player
    if (game->player.state == Playing) {
        // base texture
        SDL_RenderCopyEx(renderer, game->player.base_texture, NULL, &(SDL_Rect){window_w/2.0 - game->render_scale * 0.125, window_h/2.0 - game->render_scale * 0.25, game->render_scale * 0.5, game->render_scale * 0.5}, -game->player.rot * 180/M_PI, &(SDL_Point){game->render_scale * 0.125, game->render_scale * 0.25}, SDL_FLIP_NONE); 

        struct {float x, y;} corner_coords[3] = {
            {game->player.x + cosf(game->player.rot) * 0.375, game->player.y + sinf(game->player.rot) * 0.375}, 
            {game->player.x + (-0.125 * cosf(game->player.rot) - 0.25 * sinf(game->player.rot)), game->player.y + (-0.125 * sinf(game->player.rot) + 0.25 * cosf(game->player.rot))}, 
            {game->player.x + (-0.125 * cosf(game->player.rot) - -0.25 * sinf(game->player.rot)), game->player.y + (-0.125 * sinf(game->player.rot) + -0.25 * cosf(game->player.rot))}
        }; 

        // thrusters 
        enum {Normal, Large, Small} thruster_type = ((game->map[(int)corner_coords[0].y][(int)corner_coords[0].x] == StrongerThrusters || game->map[(int)corner_coords[1].y][(int)corner_coords[1].x] == StrongerThrusters || game->map[(int)corner_coords[2].y][(int)corner_coords[2].x] == StrongerThrusters) && (game->map[(int)corner_coords[0].y][(int)corner_coords[0].x] != WeakerThrusters && game->map[(int)corner_coords[1].y][(int)corner_coords[1].x] != WeakerThrusters && game->map[(int)corner_coords[2].y][(int)corner_coords[2].x] != WeakerThrusters)) ? Large : ((game->map[(int)corner_coords[0].y][(int)corner_coords[0].x] == WeakerThrusters || game->map[(int)corner_coords[1].y][(int)corner_coords[1].x] == WeakerThrusters || game->map[(int)corner_coords[2].y][(int)corner_coords[2].x] == WeakerThrusters) && (game->map[(int)corner_coords[0].y][(int)corner_coords[0].x] != StrongerThrusters && game->map[(int)corner_coords[1].y][(int)corner_coords[1].x] != StrongerThrusters && game->map[(int)corner_coords[2].y][(int)corner_coords[2].x] != StrongerThrusters)) ? Small : Normal;
        if (game->player.left_thruster) SDL_RenderCopyEx(renderer, game->player.thrusters_texture, &(SDL_Rect){thruster_type * 16, 0, 16, 6}, &(SDL_Rect){(corner_coords[1].x - 0.25 - game->player.x) * game->render_scale + window_w/2.0, window_h/2.0 - (corner_coords[1].y - game->player.y) * game->render_scale, 0.25 * game->render_scale, 0.09375 * game->render_scale}, -game->player.rot * 180/M_PI, &(SDL_Point){0.25 * game->render_scale, 0}, SDL_FLIP_NONE); 
        if (game->player.right_thruster) SDL_RenderCopyEx(renderer, game->player.thrusters_texture, &(SDL_Rect){thruster_type * 16, 0, 16, 6}, &(SDL_Rect){(corner_coords[2].x - 0.25 - game->player.x) * game->render_scale + window_w/2.0, window_h/2.0 - (corner_coords[2].y + 0.09375 - game->player.y) * game->render_scale, 0.25 * game->render_scale, 0.09375 * game->render_scale}, -game->player.rot * 180/M_PI, &(SDL_Point){0.25 * game->render_scale, 0.09375 * game->render_scale}, SDL_FLIP_NONE); 
        
        // sparks
        if (game->map[(int)corner_coords[0].y][(int)corner_coords[0].x] == ThrustersOn || game->map[(int)corner_coords[1].y][(int)corner_coords[1].x] == ThrustersOn || game->map[(int)corner_coords[2].y][(int)corner_coords[2].x] == ThrustersOn || game->map[(int)corner_coords[0].y][(int)corner_coords[0].x] == ThrustersOff || game->map[(int)corner_coords[1].y][(int)corner_coords[1].x] == ThrustersOff || game->map[(int)corner_coords[2].y][(int)corner_coords[2].x] == ThrustersOff) {
            SDL_RenderCopyEx(renderer, game->player.sparks_texture, &(SDL_Rect){game->player.current_frame * 16, 0, 16, 8}, &(SDL_Rect){(corner_coords[1].x - game->player.x) * game->render_scale + window_w/2.0, window_h/2.0 - (corner_coords[1].y - game->player.y) * game->render_scale, 0.25 * game->render_scale, 0.125 * game->render_scale}, -game->player.rot * 180/M_PI, &(SDL_Point){0, 0}, SDL_FLIP_NONE); 
            SDL_RenderCopyEx(renderer, game->player.sparks_texture, &(SDL_Rect){game->player.current_frame * 16, 0, 16, 8}, &(SDL_Rect){(corner_coords[2].x - game->player.x) * game->render_scale + window_w/2.0, window_h/2.0 - (corner_coords[2].y + 0.125 - game->player.y) * game->render_scale, 0.25 * game->render_scale, 0.125 * game->render_scale}, -game->player.rot * 180/M_PI, &(SDL_Point){0, 0.125 * game->render_scale}, SDL_FLIP_NONE); 
        }
    }
    else if (game->player.state == Winning) SDL_RenderCopyEx(renderer, game->player.base_texture, NULL, &(SDL_Rect){window_w/2.0 - game->render_scale * 0.125, window_h/2.0 - game->render_scale * 0.25, game->render_scale * 0.5, game->render_scale * 0.5}, -game->player.rot * 180/M_PI, &(SDL_Point){game->render_scale * 0.125, game->render_scale * 0.25}, SDL_FLIP_NONE);
    else if (game->player.state == Exploding) SDL_RenderCopy(renderer, game->player.exploding_texture, &(SDL_Rect){game->player.current_frame * 34, 0, 30, 30}, &(SDL_Rect){window_w/2.0 - game->render_scale, window_h/2.0 - game->render_scale, game->render_scale * 2, game->render_scale * 2}); 

    // timer and level title
    render_text(game->timer_texture, 0, 32, MiddleLeft, 2, renderer); 
    render_text(game->level_description, window_w - 10, 32, MiddleRight, 1, renderer); 
}

void handle_win_menu_event(struct WinMenu *win_menu, SDL_Event event, enum AppState *next_app_state, unsigned *last_played_level_num, enum LevelType last_played_level_type) {
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        int row = event.button.y/64, col = event.button.x/64; 
        
        if (row == 9 && 1 <= col && col <= 8) *next_app_state = last_played_level_type == Normal? InMainMenu : InCustomMenu; 

        else if (row == 9 && 11 <= col && col <= 18 && last_played_level_type == Normal && *last_played_level_num < 10 - 1) {
            ++*last_played_level_num; 
            *next_app_state = InGame; 
        }
    }
}
void render_win_menu(struct WinMenu *win_menu, SDL_Renderer *renderer, unsigned window_w, unsigned window_h, SDL_Texture *tiles, unsigned last_played_level_num, enum LevelType last_played_level_type, int is_new_record) {
    for (unsigned row = 0; row < window_h/64 + 1; ++row) {
        for (unsigned col = 0; col < window_w/64; ++col) {
            enum Tile tile_at_pos = (row == 9 && 1 <= col && col <= 8)? Solid : (row == 9 && 11 <= col && col <= 18) ? (last_played_level_type == Normal && last_played_level_num < 10 - 1) ? Solid: Friction: None; 
            SDL_RenderCopy(renderer, tiles, &(SDL_Rect){tile_at_pos * 16, 0, 16, 16}, &(SDL_Rect){col * 64, row * 64, 64, 64}); 
        }
    }

    render_text(win_menu->completed, window_w/2, 32, Middle, 2.5, renderer); 
    render_text(win_menu->level_description, window_w/2, 160, Middle, 1.25, renderer); 
    if (is_new_record) render_text(win_menu->new_record, window_w/2, 288, Middle, 1.75, renderer); 
    render_text(win_menu->timer_label, window_w * 0.4, 352, Middle, 0.75, renderer); 
    render_text(win_menu->best_time_label, window_w * 0.6, 352, Middle, 0.75, renderer); 
    render_text(win_menu->timer, window_w * 0.4, 416, Middle, 2.5, renderer); 
    if (win_menu->best_time != NULL) render_text(win_menu->best_time, window_w * 0.6, 416, Middle, 2.5, renderer); 
    render_text(win_menu->menu_button_text, 320, 608, Middle, 1.25, renderer); 
    render_text(win_menu->next_button_text, 960, 608, Middle, 1.25, renderer);  
}

void handle_lose_menu_event(struct LoseMenu *lose_menu, SDL_Event event, enum AppState *next_app_state, enum LevelType last_played_level_type) {
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        int row = event.button.y/64, col = event.button.x/64; 
        if (row == 9 && 1 <= col && col <= 8) *next_app_state = last_played_level_type == Normal? InMainMenu : InCustomMenu; 
        
        else if (row == 9 && 11 <= col && col <= 18) *next_app_state = InGame; // no need to reset level num or type
    }
}
void render_lose_menu(struct LoseMenu *lose_menu, SDL_Renderer *renderer, unsigned window_w, unsigned window_h, SDL_Texture *tiles) {
    for (unsigned row = 0; row < window_h/64 + 1; ++row) {
        for (unsigned col = 0; col < window_w/64; ++col) {
            enum Tile tile_at_pos = (row == 9 && (1 <= col && col <= 8 || 11 <= col && col <= 18))? Solid: None; 
            SDL_RenderCopy(renderer, tiles, &(SDL_Rect){tile_at_pos * 16, 0, 16, 16}, &(SDL_Rect){col * 64, row * 64, 64, 64}); 
        }
    }

    render_text(lose_menu->destroyed, window_w/2, 32, Middle, 2.5, renderer); 
    render_text(lose_menu->level_description, window_w/2, 160, Middle, 1.25, renderer); 
    render_text(lose_menu->timer_label, window_w * 0.4, 352, Middle, 0.75, renderer); 
    render_text(lose_menu->best_time_label, window_w * 0.6, 352, Middle, 0.75, renderer); 
    render_text(lose_menu->timer, window_w * 0.4, 416, Middle, 2.5, renderer); 
    if (lose_menu->best_time != NULL) render_text(lose_menu->best_time, window_w * 0.6, 416, Middle, 2.5, renderer);
    render_text(lose_menu->menu_button_text, 320, 608, Middle, 1.25, renderer); 
    render_text(lose_menu->retry_button_text, 960, 608, Middle, 1.25, renderer);  
}

void handle_editor_event(struct Editor *editor, SDL_Event event, enum AppState *next_app_state, unsigned window_w, unsigned window_h, TTF_Font *menu_font, SDL_Renderer *renderer) {
    // select tool
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.x < 128) {
        int was_tool_set_description = editor->tool == SetDescription; 

        int row = event.button.y/64, col = event.button.x/64; 
        if (0 <= row && row <= 8 && !(row == 8 && col == 1)) {
            editor->tool = DrawTiles; 
            editor->selected_draw_tile = row * 2 + col; 
        }
        else if (row == 9 && col == 0) editor->tool = SetSpawn; 
        else if (row == 9 && col == 1) {
            editor->tool = RotateSpawn; 
            editor->spawn_rot += M_PI/4; 
        }
        else if (row == 10 && col == 0) {
            editor->tool = SetDescription; 
            SDL_StartTextInput(); 
        }
        else if (row == 10 && col == 1) {
            editor->tool = Leave; // this basically doesnt matter since it will only display for one frame, but i am just going to do it for consistancy 
            *next_app_state = InCustomMenu; 
        } 

        if (was_tool_set_description && editor->tool != SetDescription) SDL_StopTextInput();
    }

    // drawing tool (do seperate elifs that way camera movement can happen no matter the tool)
    else if (editor->tool == DrawTiles && event.type == SDL_MOUSEBUTTONDOWN && event.button.x > 136) {
        editor->drawing_tiles = 1; 
        int col = floorf((event.button.x - (window_w - 136)/2.0 - 136)/editor->render_scale + editor->camera_x); 
        int row = floorf((window_h/2.0 - event.button.y)/editor->render_scale + editor->camera_y); 
        if (row >= 0 && row < MAP_H && col >= 0 && col < MAP_W) editor->map[row][col] = editor->selected_draw_tile; 
    }
    else if (editor->tool == DrawTiles && editor->drawing_tiles && event.type == SDL_MOUSEMOTION && event.motion.x > 136) {
        int col = floorf((event.motion.x - (window_w - 136)/2.0 - 136)/editor->render_scale + editor->camera_x); 
        int row = floorf((window_h/2.0 - event.motion.y)/editor->render_scale + editor->camera_y); 
        if (row >= 0 && row < MAP_H && col >= 0 && col < MAP_W) editor->map[row][col] = editor->selected_draw_tile; 
    }
    else if (editor->tool == DrawTiles && event.type == SDL_MOUSEBUTTONUP) editor->drawing_tiles = 0; 

    // spawn set tool 
    else if (editor->tool == SetSpawn && event.type == SDL_MOUSEBUTTONDOWN && event.button.x > 136) {
        editor->spawn_x = (event.button.x - (window_w - 136)/2.0 - 136)/editor->render_scale + editor->camera_x; 
        editor->spawn_y = (window_h/2.0 - event.button.y)/editor->render_scale + editor->camera_y; 
    }

    // description set tool 
    else if (editor->tool == SetDescription && event.type == SDL_TEXTINPUT) {
        size_t len = strlen(editor->description); 
        if (len < 31) {
            editor->description[len] = event.text.text[0]; 
            editor->description[len + 1] = '\0'; 

            SDL_DestroyTexture(editor->description_texture); 
            init_text(&editor->description_texture, editor->description, menu_font, (SDL_Color){0, 180, 180, 255}, renderer); 
        }
    }
    else if (editor->tool == SetDescription && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE) {
        size_t len = strlen(editor->description); 
        if (len > 0) {
            editor->description[len - 1] = '\0';
            SDL_DestroyTexture(editor->description_texture); 
            init_text(&editor->description_texture, editor->description, menu_font, (SDL_Color){0, 180, 180, 255}, renderer); 
        }
    }

    // camera movement
    else if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_LEFT) editor->camera_x -= 0.5; 
        else if (event.key.keysym.sym == SDLK_RIGHT) editor->camera_x += 0.5; 
        else if (event.key.keysym.sym == SDLK_DOWN) editor->camera_y -= 0.5; 
        else if (event.key.keysym.sym == SDLK_UP) editor->camera_y += 0.5; 
        else if (event.key.keysym.sym == SDLK_i && editor->tool != SetDescription && editor->render_scale < 128) editor->render_scale += 16; 
        else if (event.key.keysym.sym == SDLK_o && editor->tool != SetDescription && editor->render_scale > 16) editor->render_scale -= 16;   
    }
}
void render_editor(struct Editor *editor, SDL_Renderer *renderer, unsigned window_w, unsigned window_h, SDL_Texture *tiles, SDL_Texture *player) {
    // canvas map 
    for (int row = floorf(editor->camera_y - window_h/2.0/editor->render_scale); row <= floorf(editor->camera_y + window_h/2.0/editor->render_scale); ++row) {
        for (int col = floorf(editor->camera_x - (window_w - 136)/2.0/editor->render_scale); col <= floorf(editor->camera_x + (window_w - 136)/2.0/editor->render_scale); ++col) {
            SDL_RenderCopy(renderer, tiles, &(SDL_Rect){row >= 0 && row < MAP_H && col >= 0 && col < MAP_W ? editor->map[row][col] * 16: 16, 0, 16, 16}, &(SDL_Rect){(col - editor->camera_x) * editor->render_scale + (window_w - 136)/2.0 + 136, window_h/2.0 - (row - editor->camera_y) * editor->render_scale - editor->render_scale, editor->render_scale, editor->render_scale}); 
        }
    }
    // canvas spawn point 
    SDL_RenderCopyEx(renderer, player, NULL, &(SDL_Rect){(editor->spawn_x - editor->camera_x) * editor->render_scale + (window_w - 136)/2.0 - editor->render_scale * 0.125 + 136, window_h/2.0 - (editor->spawn_y - editor->camera_y) * editor->render_scale - editor->render_scale * 0.25,  editor->render_scale * 0.5, editor->render_scale * 0.5}, -editor->spawn_rot * 180/M_PI,  &(SDL_Point){editor->render_scale * 0.125, editor->render_scale * 0.25}, SDL_FLIP_NONE); 

    // tool bar border
    SDL_SetRenderDrawColor(renderer, 64, 64, 64, 255); 
    SDL_RenderFillRect(renderer, &(SDL_Rect){128, 0, 8, window_h}); 
    
    // tool bar background 
    for (unsigned row = 0; row < window_h/64 + 1; ++row) {
        for (unsigned col = 0; col < 2; ++col) {
            enum Tile tile_at_pos = (0 <= row && row <= 8 && !(row == 8 && col == 1))? row * 2 + col : (row == 9 && col == 1)? CounterClockwiseTorque : (row == 10) ? Solid: None; 
            SDL_RenderCopy(renderer, tiles, &(SDL_Rect){tile_at_pos * 16, 0, 16, 16}, &(SDL_Rect){col * 64, row * 64, 64, 64}); 
        }
    }
    // overbutton player and text textures 
    SDL_RenderCopyEx(renderer, player, NULL, &(SDL_Rect){16, 9*64 + 16, 32, 32}, 270, &(SDL_Point){16, 16}, SDL_FLIP_NONE); 
    SDL_RenderCopyEx(renderer, player, NULL, &(SDL_Rect){80, 9*64 + 16, 32, 32}, 270, &(SDL_Point){16, 16}, SDL_FLIP_NONE); 
    render_text(editor->rename_button_text, 32, 10 * 64 + 32, Middle, 0.75 ,renderer); 
    render_text(editor->leave_button_text, 96, 10 * 64 + 32, Middle, 1.5, renderer); 

    // level description texture 
    render_text(editor->description_texture, 136, 32, MiddleLeft, 1, renderer); 

    // tool highlight 
    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255); 
    if (editor->tool == DrawTiles) SDL_RenderDrawRect(renderer, &(SDL_Rect){(editor->selected_draw_tile % 2) * 64, editor->selected_draw_tile / 2 * 64, 64, 64}); 
    else if (editor->tool == SetSpawn) SDL_RenderDrawRect(renderer, &(SDL_Rect){0, 9 * 64, 64, 64}); 
    else if (editor->tool == RotateSpawn) SDL_RenderDrawRect(renderer, &(SDL_Rect){64, 9 * 64, 64, 64}); 
    else if (editor->tool == SetDescription) SDL_RenderDrawRect(renderer, &(SDL_Rect){0, 10 * 64, 64, 64}); 
    else if (editor->tool == Leave) SDL_RenderDrawRect(renderer, &(SDL_Rect){64, 10 * 64, 64, 64}); 
}

void handle_pause_menu_event(struct PauseMenu *pause_menu, SDL_Event event, enum AppState *next_app_state, enum LevelType last_played_level_type) {
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        int row = event.button.y/64, col = event.button.x/64; 
        if (row == 9 && 1 <= col && col <= 8) *next_app_state = last_played_level_type == Normal? InMainMenu : InCustomMenu; 

        else if (row == 9 && 11 <= col && col <= 18) *next_app_state = InGame;
    }
}
void render_pause_menu(struct PauseMenu *pause_menu, SDL_Renderer *renderer, unsigned window_w, unsigned window_h, SDL_Texture *tiles) {
    for (unsigned row = 0; row < window_h/64 + 1; ++row) {
        for (unsigned col = 0; col < window_w/64; ++col) {
            enum Tile tile_at_pos = (row == 9 && (1 <= col && col <= 8 || 11 <= col && col <= 18))? Solid: None; 
            SDL_RenderCopy(renderer, tiles, &(SDL_Rect){tile_at_pos * 16, 0, 16, 16}, &(SDL_Rect){col * 64, row * 64, 64, 64}); 
        }
    }

    render_text(pause_menu->paused, window_w/2, 32, Middle, 2.5, renderer); 
    render_text(pause_menu->level_description, window_w/2, 160, Middle, 1.25, renderer); 
    render_text(pause_menu->timer_label, window_w * 0.4, 352, Middle, 0.75, renderer); 
    render_text(pause_menu->best_time_label, window_w * 0.6, 352, Middle, 0.75, renderer); 
    render_text(pause_menu->timer, window_w * 0.4, 416, Middle, 2.5, renderer); 
    if (pause_menu->best_time != NULL) render_text(pause_menu->best_time, window_w * 0.6, 416, Middle, 2.5, renderer);
    render_text(pause_menu->menu_button_text, 320, 608, Middle, 1.25, renderer); 
    render_text(pause_menu->resume_button_text, 960, 608, Middle, 1.25, renderer);  
}

void run_app(struct App *app) {
    unsigned long last_time = SDL_GetPerformanceCounter(); 
    unsigned long frequency = SDL_GetPerformanceFrequency(); 
    int window_w, window_h; SDL_GetWindowSize(app->window, &window_w, &window_h); 

    while (app->running) {
        unsigned long current_time = SDL_GetPerformanceCounter(); 
        app->delta_time = (current_time - last_time)/(float)frequency; 

        for (SDL_Event event; SDL_PollEvent(&event); ) {
            if (event.type == SDL_QUIT) app->running = 0;  
            else if (app->state == InMainMenu) handle_main_menu_event(&app->main_menu, event, &app->next_state, app->renderer, app->num_unlocked_levels, &app->last_played_level_num, &app->last_played_level_type); 
            else if (app->state == InCustomMenu) handle_custom_menu_event(&app->custom_menu, event, &app->next_state, app->renderer, app->main_menu.font, app->num_custom_levels, &app->last_played_level_num, &app->last_played_level_type, &app->last_edited_level_num); 
            else if (app->state == InGame) handle_game_event(&app->game, event, &app->next_state); 
            else if (app->state == InWinMenu) handle_win_menu_event(&app->win_menu, event, &app->next_state, &app->last_played_level_num, app->last_played_level_type); 
            else if (app->state == InLoseMenu) handle_lose_menu_event(&app->lose_menu, event, &app->next_state, app->last_played_level_type); 
            else if (app->state == InEditor) handle_editor_event(&app->editor, event, &app->next_state, window_w, window_h, app->main_menu.font, app->renderer);
            else if (app->state == InPauseMenu) handle_pause_menu_event(&app->pause_menu, event, &app->next_state, app->last_played_level_type); 
        }

        if (app->state == InMainMenu) render_main_menu(&app->main_menu, app->renderer, window_w, window_h, app->game.tiles_texture, app->num_unlocked_levels); 
        else if (app->state == InCustomMenu) render_custom_menu(&app->custom_menu, app->renderer, window_w, window_h, app->game.tiles_texture, app->num_custom_levels); 
        else if (app->state == InGame) {
            update_game(&app->game, app->delta_time, app->main_menu.font, app->renderer, &app->next_state);
            render_game(&app->game, app->renderer, window_w, window_h); 
        }
        else if (app->state == InWinMenu) render_win_menu(&app->win_menu, app->renderer, window_w, window_h, app->game.tiles_texture, app->last_played_level_num, app->last_played_level_type, app->game.timer < app->game.best_time); 
        else if (app->state == InLoseMenu) render_lose_menu(&app->lose_menu, app->renderer, window_w, window_h, app->game.tiles_texture); 
        else if (app->state == InEditor) render_editor(&app->editor, app->renderer, window_w, window_h, app->game.tiles_texture, app->game.player.base_texture); 
        else if (app->state == InPauseMenu) render_pause_menu(&app->pause_menu, app->renderer, window_w, window_h, app->game.tiles_texture); 
        
        SDL_RenderPresent(app->renderer); 

        if (app->state != app->next_state) { // if one of the state functions requested a state change
            if (!(app->state == InGame && app->next_state == InPauseMenu)) cleanup_app_state(app); // dont clean up game before pause menu 
            if (!(app->state == InPauseMenu && app->next_state == InGame)) { // do not initialize game after pause menu 
                app->state = app->next_state; 
                init_app_state(app); 
            }
            else app->state = app->next_state; 
        }
        last_time = current_time;   
    }
}
int main() {
    struct App app;  
    init_app(&app); 
    run_app(&app); 
    cleanup_app(&app); 
    printf("#########################      CHECK:        SDL Error: [[[%s]]]           ##########################\n", SDL_GetError());
    return EXIT_SUCCESS; 
}
