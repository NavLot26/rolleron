#include <stdlib.h> 
#include <math.h> 

#include <SDL.h> 
#include <SDL_image.h> 

#define MAP_W 16
#define MAP_H 48
struct App {
    SDL_Window *window; 
    SDL_Renderer *renderer; 
    int running; 
    float delta_time; 
    enum AppState {InMenu, InGame} state; 

    struct Game {
        enum Tile {Solid, Gravity, None} map[MAP_H][MAP_W]; 
        SDL_Texture *tiles_texture;  

        struct Player {
            float x, y, vel_x, vel_y; 
            float rot, rot_vel; 
            int right_thruster, left_thruster; 
            SDL_Texture *texture; 
        } player; 
    } game; 

    struct Menu {

    } menu; 
}; 

void init_app(struct App *app) {
    app->window =  SDL_CreateWindow("Rolleron", 100, 100, 768, 768, SDL_WINDOW_SHOWN); 
    app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_ACCELERATED); 
    app->running = 1; 
    app->delta_time = 0;    
    app->state = InMenu; 
#   define X Solid
#   define _ None
#   define O Gravity
    memcpy(app->game.map, (enum Tile[MAP_H][MAP_W]){{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, X, X, X, X, X, X, X, X, X, X, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, X, X, X, X, X, X, X, X, X, X, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, X, X, X, X, X, X, X, X, X, X, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, X, X, X, X, X, X, X, X, X, X, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, _, _, _, _, _, _, _, _, _, _, _, _, _, _, X},
{X, O, O, O, O, O, _, _, _, _, O, O, O, O, O, X},
{X, O, O, O, O, O, _, _, _, _, O, O, O, O, O, X},
{X, O, O, O, O, O, _, _, _, _, O, O, O, O, O, X},
{X, O, O, O, O, O, _, _, _, _, O, O, O, O, O, X},
{X, O, O, O, O, O, _, _, _, _, O, O, O, O, O, X},
{X, O, O, O, O, O, _, _, _, _, O, O, O, O, O, X},}, sizeof(app->game.map)); 
    app->game.tiles_texture = IMG_LoadTexture(app->renderer, "assets/tiles.png"); 
    app->game.player.x = MAP_W * 0.5; app->game.player.y = 2; // a lot of this might be reset when the level is initialized, but idc, I might move all this to an init_game_state func later on tbh
    app->game.player.vel_x = 0; app->game.player.vel_y = 0; 
    app->game.player.rot = M_PI/2; app->game.player.rot_vel = 0; 
    app->game.player.right_thruster = 0; app->game.player.left_thruster = 0; 
    app->game.player.texture = IMG_LoadTexture(app->renderer, "assets/space_ship.png"); 
}

void destroy_app(struct App *app) {
    SDL_DestroyTexture(app->game.player.texture); 
    SDL_DestroyTexture(app->game.tiles_texture); 
    SDL_DestroyWindow(app->window); 
    SDL_DestroyRenderer(app->renderer); 
    IMG_Quit(); 
    SDL_Quit(); 
}

void handle_game_event(struct Game *game, SDL_Event event, enum AppState *app_state) {
    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_q) *app_state = InMenu; 
        if (event.key.keysym.sym == SDLK_LEFT) game->player.left_thruster = 1; 
        else if (event.key.keysym.sym == SDLK_RIGHT) game->player.right_thruster = 1;  
        
    }
    else if (event.type == SDL_KEYUP) {
        if (event.key.keysym.sym == SDLK_LEFT) game->player.left_thruster = 0; 
        else if (event.key.keysym.sym == SDLK_RIGHT) game->player.right_thruster = 0;   
    }
}

void update_game(struct Game *game, float delta_time, enum AppState *app_state) {
    // Player movement 
    float net_force_x = 0, net_force_y = 0; 
    float net_torque = 0; 

    for (size_t row = 0; row < MAP_H; ++row) {
        for (size_t col = 0; col < MAP_W; ++col) {
            if (game->map[row][col] == Gravity) {
                float dist_squared = (powf(col + 0.5 - game->player.x, 2) + powf(row + 0.5 - game->player.y, 2)); 
                float dist = sqrt(dist_squared); 
                net_force_x += (col + 0.5 - game->player.x)/dist * 2/dist_squared; 
                net_force_y += (row + 0.5 - game->player.y)/dist * 2/dist_squared; 
            }
        }
    }

    if (game->player.right_thruster) {
        net_force_x += cosf(game->player.rot); 
        net_force_y += sinf(game->player.rot);  
        net_torque += 0.25; // this is the x offset of the right thruster which the cross product operation simplifies to because of the clever coordinate space I chose
    }
    if (game->player.left_thruster) {
        net_force_x += cosf(game->player.rot); 
        net_force_y += sinf(game->player.rot); 
        net_torque += -0.25; // this is the x offset of the left thruster which the cross product operation simplifies to because of the clever coordinate space I chose
    }
    
    game->player.vel_x += net_force_x / 1 * delta_time; 
    game->player.vel_y += net_force_y / 1 * delta_time; 
    game->player.x += game->player.vel_x * delta_time; 
    game->player.y += game->player.vel_y * delta_time; 
    game->player.rot_vel += net_torque / 0.05 * delta_time; // super low moment of inertia to make the ship rotate easier
    game->player.rot += game->player.rot_vel * delta_time; 


    // player death
    // 0.75 is width before rotation while 0.5 is height before rotation
    int p1_col = floorf(game->player.x + cosf(game->player.rot) * 0.375); 
    int p1_row = ceilf(game->player.y + sinf(game->player.rot) * 0.375); 

    int p2_col = floorf(game->player.x + (0.333 - 0.375) * cosf(game->player.rot) - 0.25 * sinf(game->player.rot)); // simplified formula from rotation matrix 
    int p2_row = ceilf(game->player.y + (0.333 - 0.375) * sinf(game->player.rot) + 0.25 * cosf(game->player.rot)); 

    int p3_col = floorf(game->player.x + (0.333 - 0.375) * cosf(game->player.rot) - -0.25 * sinf(game->player.rot)); // simplified formula from rotation matrix
    int p3_row = ceilf(game->player.y + (0.333 - 0.375) * sinf(game->player.rot) + -0.25 * cosf(game->player.rot)); 

    if (p1_col < 0 || p1_col >= MAP_W || p1_row < 0 || p1_row >= MAP_H || game->map[p1_row][p1_col] != None || p2_col < 0 || p2_col >= MAP_W || p2_row < 0 || p2_row >= MAP_H || game->map[p2_row][p2_col] != None || p3_col < 0 || p3_col >= MAP_W || p3_row < 0 || p3_row >= MAP_H || game->map[p3_row][p3_col] != None) *app_state = InMenu; 


}

void render_game(struct Game *game, SDL_Renderer *renderer, unsigned window_w, unsigned window_h) {
    SDL_RenderCopyEx(renderer, game->player.texture, &(SDL_Rect){(game->player.right_thruster && game->player.left_thruster ? 0 : (game->player.left_thruster ? 1 : (game->player.right_thruster ? 2: 3))) * 48, 0, 48, 32}, &(SDL_Rect){game->player.x * window_w/MAP_W - (float)window_w/MAP_W * 0.375,  window_h * 0.75 - (float)window_w/MAP_W * 0.25, (float)window_w/MAP_W * 0.75, (float)window_w/MAP_W * 0.5}, -game->player.rot * 180/M_PI, &(SDL_Point){(float)window_w/MAP_W * 0.375, (float)window_w/MAP_W * 0.25}, SDL_FLIP_NONE); // so 0.75 is the width before rotation while 0.5 is the height before rotation, this means the half widht and half height are 0.375 and 0.25 which are used to offset the texture and as its rotation anchor 

    // remember screen_x = world_x * window_w/MAP_W, screen_y = window_h * 0.75 - window_w/MAP_W * (world_y - player_y)
    for (int row = ceilf(game->player.y + window_h * 0.75/((float)window_w/MAP_W)); row >= ceilf(game->player.y - window_h * 0.25/((float)window_w/MAP_W)); --row) {
        for (unsigned col = 0; col < MAP_W; ++col) {
            if (row >= 0 && row < MAP_H && game->map[row][col] != None) SDL_RenderCopy(renderer, game->tiles_texture, &(SDL_Rect){game->map[row][col] * 16, 0, 16, 16}, &(SDL_Rect){col * window_w/MAP_W, window_h * 0.75 - (float)window_w/MAP_W * (row - game->player.y), window_w/MAP_W, window_w/MAP_W}); 
        }
    }
}

void handle_menu_event(struct Menu *menu, SDL_Event event, enum AppState *app_state) {
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) *app_state = InGame; 
}

void render_menu(struct Menu *menu, SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 0255); 
    SDL_RenderFillRect(renderer, &(SDL_Rect){100, 100, 300, 300}); 
}

void run_app(struct App *app) {
    unsigned long last_time = SDL_GetPerformanceCounter(); 
    unsigned long frequency = SDL_GetPerformanceFrequency(); 
    float frame_delay = 1.0/60;  

    while (app->running) {
        unsigned long current_time = SDL_GetPerformanceCounter(); 
        app->delta_time = (current_time - last_time)/(float)frequency; 

        for (SDL_Event event; SDL_PollEvent(&event); ) {
            if (event.type == SDL_QUIT) app->running = 0; 
            else if (app->state == InGame) handle_game_event(&app->game, event, &app->state); 
            else if (app->state == InMenu) handle_menu_event(&app->menu, event, &app->state); 
        }

        SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 0255); 
        SDL_RenderClear(app->renderer); 

        if (app->state == InGame) {
            int window_w, window_h; SDL_GetWindowSize(app->window, &window_w, &window_h); 
            update_game(&app->game, app->delta_time, &app->state); 
            render_game(&app->game, app->renderer, window_w, window_h); 
        }
        else if (app->state == InMenu) render_menu(&app->menu, app->renderer); 

        SDL_RenderPresent(app->renderer);  
        
        if (app->delta_time < frame_delay) SDL_Delay((frame_delay - app->delta_time) * 1000); 
        last_time = current_time;   
    }
}

int main() {
    struct App app; 
    init_app(&app); 
    run_app(&app); 
    destroy_app(&app); 
    return EXIT_SUCCESS; 
}
