#include <stdlib.h> 
#include <math.h> 

#include <SDL.h> 
#include <SDL_image.h> 

#define MAP_W 48
#define MAP_H 32

struct App {
    SDL_Window *window; 
    SDL_Renderer *renderer; 
    int running; 
    float delta_time; 
    enum AppState {InGame, InMenu, InWinScreen, InLoseScreen} state; 

    struct Game {
        float render_scale; 
        enum Tile {None, Solid, Gravity, AntiGravity, Win, Friction, AntiFriction, NoThrusters} map[MAP_H][MAP_W]; 
        float spawn_x, spawn_y; 
        SDL_Texture *tiles_texture;

        struct Player {
            enum PlayerState {Playing, Exploding, Winning} state; 
            float x, y;
            float vel_x, vel_y; 
            float rot, rot_vel;  
            int right_thruster, left_thruster; 
            SDL_Texture *texture; 
            int current_frame; 
            float animation_timer; 
            SDL_Texture *exploding_texture; 
        } player; 
    } game; 

    struct Menu {

    } menu; 
}; 

void init_app(struct App *app) {
    SDL_Init(SDL_INIT_EVERYTHING); 
    IMG_Init(IMG_INIT_PNG); 
    app->window =  SDL_CreateWindow("Rolleron", 100, 100, 1280, 720, SDL_WINDOW_SHOWN); 
    app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_ACCELERATED); 
    app->running = 1; 
    app->delta_time = 0;    
    app->state = InMenu; 


    app->game.render_scale = 64; 
    // map
    FILE *level_file = fopen("assets/level.dat", "rb"); 
    fread(&app->game.spawn_x, sizeof(float), 1, level_file); 
    fread(&app->game.spawn_y, sizeof(float), 1, level_file); 
    fread(&app->game.map, sizeof(enum Tile), MAP_W * MAP_H, level_file); 
    fclose(level_file); 
    app->game.tiles_texture = IMG_LoadTexture(app->renderer, "assets/tiles.png"); 

    // player -- permanent stuff
    app->game.player.texture = IMG_LoadTexture(app->renderer, "assets/space_ship.png"); 
    app->game.player.exploding_texture = IMG_LoadTexture(app->renderer, "assets/explosion.png"); 
}

void destroy_app(struct App *app) {
    SDL_DestroyTexture(app->game.player.exploding_texture); 
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
        else if (event.key.keysym.sym == SDLK_LEFT && game->player.state == Playing) game->player.left_thruster = 1;
        else if (event.key.keysym.sym == SDLK_RIGHT && game->player.state == Playing) game->player.right_thruster = 1;  
        
    }
    else if (event.type == SDL_KEYUP) {
        if (event.key.keysym.sym == SDLK_LEFT && game->player.state == Playing) game->player.left_thruster = 0; 
        else if (event.key.keysym.sym == SDLK_RIGHT && game->player.state == Playing) game->player.right_thruster = 0;   
    }
}

void update_game(struct Game *game, float delta_time, enum AppState *app_state) {
    if (game->player.state == Playing)  {// handling playing state
        float net_force_x = 0, net_force_y = 0; 
        float net_torque = 0; 

        // Thrusters effect on player while playing
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

        // No-touch tiles effect on player while playing (gravity and anti-gravity)
        for (size_t row = 0; row < MAP_H; ++row) {
            for (size_t col = 0; col < MAP_W; ++col) {
                if (game->map[row][col] == Gravity) {
                    float dist_squared = (powf(col + 0.5 - game->player.x, 2) + powf(row + 0.5 - game->player.y, 2)); 
                    float dist = sqrt(dist_squared); 
                    net_force_x += (col + 0.5 - game->player.x)/dist * 3/dist_squared; 
                    net_force_y += (row + 0.5 - game->player.y)/dist * 3/dist_squared; 
                }
                else if (game->map[row][col] == AntiGravity) {
                    float dist_squared = (powf(col + 0.5 - game->player.x, 2) + powf(row + 0.5 - game->player.y, 2)); 
                    float dist = sqrt(dist_squared); 
                    net_force_x += (game->player.x - (col + 0.5))/dist * 3/dist_squared; 
                    net_force_y += (game->player.y - (row + 0.5))/dist * 3/dist_squared; 
                }
            }
        }

        // Yes-touch tiles effect on player while playing (solid, gravity, anti-gravity, win, friction, AntiFriction)
        struct {int row; int col;} collision_point_cords[3] = { // get the collision points to test for collisions with
            {floorf(game->player.y + sinf(game->player.rot) * 0.375),floorf(game->player.x + cosf(game->player.rot) * 0.375)}, 
            {floorf(game->player.y + (0.333 - 0.375) * sinf(game->player.rot) + 0.25 * cosf(game->player.rot)),floorf(game->player.x + (0.333 - 0.375) * cosf(game->player.rot) - 0.25 * sinf(game->player.rot))}, 
            {floorf(game->player.y + (0.333 - 0.375) * sinf(game->player.rot) + -0.25 * cosf(game->player.rot)), floorf(game->player.x + (0.333 - 0.375) * cosf(game->player.rot) - -0.25 * sinf(game->player.rot))}
        }; 
        for (int i = 0; i < 3; ++i) { // loop through each collision point
            // collision with solid blocks causes explosion 
            if (collision_point_cords[i].row < 0 || collision_point_cords[i].row >= MAP_H || collision_point_cords[i].col < 0 || collision_point_cords[i].col >= MAP_W || game->map[collision_point_cords[i].row][collision_point_cords[i].col] == Solid || game->map[collision_point_cords[i].row][collision_point_cords[i].col] == Gravity || game->map[collision_point_cords[i].row][collision_point_cords[i].col] == AntiGravity) game->player.state = Exploding;  // because we checked for out of bounds here, there is no more need to check for it for the rest of these collision point checks 
            // collision with win block causes win 
            else if (game->map[collision_point_cords[i].row][collision_point_cords[i].col] == Win) {
                game->player.state = Winning; 
                game->player.right_thruster = 0; game->player.left_thruster = 0;
            }
            else if (game->map[collision_point_cords[i].row][collision_point_cords[i].col] == Friction) {
                net_force_x += -0.25 * game->player.vel_x; 
                net_force_y += -0.25 * game->player.vel_y; 
                net_torque += -0.025 * game->player.rot_vel; 
            }
            else if (game->map[collision_point_cords[i].row][collision_point_cords[i].col] == AntiFriction) {
                net_force_x += 0.25 * game->player.vel_x; 
                net_force_y += 0.25 * game->player.vel_y; 
                net_torque += 0.025 * game->player.rot_vel; 
            }
            else if (game->map[collision_point_cords[i].row][collision_point_cords[i].col] == NoThrusters) {
                game->player.left_thruster = 0; 
                game->player.right_thruster = 0; 
            }
        }

        // Apply movement effects while playing
        game->player.vel_x += net_force_x / 1 * delta_time; game->player.vel_y += net_force_y / 1 * delta_time; 
        game->player.x += game->player.vel_x * delta_time; game->player.y += game->player.vel_y * delta_time; 
        game->player.rot_vel += net_torque / 0.05 * delta_time; 
        game->player.rot += game->player.rot_vel * delta_time; 
    }
    else if (game->player.state == Exploding) {
        game->player.animation_timer += delta_time; 
        if (game->player.animation_timer >= 0.1) {
            game->player.animation_timer = 0.1 - game->player.animation_timer; 
            game->player.current_frame += 1; 
            if (game->player.current_frame >= 9) *app_state = InLoseScreen; 
        }
    }
    else if (game->player.state == Winning) {
        game->player.vel_x *= powf(0.25, delta_time); game->player.vel_y *= powf(0.25, delta_time); 
        game->player.x += game->player.vel_x * delta_time; game->player.y += game->player.vel_y * delta_time; 
        game->player.rot_vel *= powf(0.025, delta_time); 
        game->player.rot += game->player.rot_vel * delta_time; 
        if (sqrtf(game->player.vel_x * game->player.vel_x + game->player.vel_y * game->player.vel_y) < 0.1) *app_state = InWinScreen; 
    }
}

void render_game(struct Game *game, SDL_Renderer *renderer, unsigned window_w, unsigned window_h) { 
    for (int row = floorf(game->player.y - window_h/2.0/game->render_scale); row <= floorf(game->player.y + window_h/2.0/game->render_scale); ++row) {
        for (int col = floorf(game->player.x - window_w/2.0/game->render_scale); col <= floorf(game->player.x + window_w/2.0/game->render_scale); ++col) {
            SDL_RenderCopy(renderer, game->tiles_texture, &(SDL_Rect){row >= 0 && row < MAP_H && col >= 0 && col < MAP_W ? game->map[row][col] * 16: 16, 0, 16, 16}, &(SDL_Rect){(col - game->player.x) * game->render_scale + window_w/2.0, window_h * 0.5 - (row - game->player.y) * game->render_scale - game->render_scale, game->render_scale, game->render_scale}); 
        }
    }

    if (game->player.state == Playing || game->player.state == Winning) SDL_RenderCopyEx(renderer, game->player.texture, &(SDL_Rect){ (game->player.right_thruster && game->player.left_thruster ? 0 : (game->player.left_thruster ? 1 : (game->player.right_thruster ? 2: 3))) * 48, 0, 48, 32}, &(SDL_Rect){window_w/2.0 - game->render_scale * 0.375, window_h/2.0 - game->render_scale * 0.25, game->render_scale * 0.75, game->render_scale * 0.5}, -game->player.rot * 180/M_PI, &(SDL_Point){game->render_scale * 0.375, game->render_scale * 0.25}, SDL_FLIP_NONE); 
    else if (game->player.state == Exploding) SDL_RenderCopy(renderer, game->player.exploding_texture, &(SDL_Rect){game->player.current_frame * 34, 0, 30, 30}, &(SDL_Rect){window_w/2.0 - game->render_scale, window_h/2.0 - game->render_scale, game->render_scale * 2, game->render_scale * 2}); 
}

void handle_menu_event(struct Menu *menu, SDL_Event event, enum AppState *app_state, struct Game *game) { 
    if (*app_state == InMenu && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) {
        // set the game state stuff that needs to be reset
        game->player.state = Playing; 
        game->player.x = game->spawn_x; game->player.y = game->spawn_y; // set these to spawn x and y later on
        game->player.vel_x = 0; game->player.vel_y = 0; 
        game->player.rot = M_PI/2;game->player.rot_vel = 0; 
        game->player.right_thruster = 0; game->player.left_thruster = 0; 
        game->player.current_frame = 0; 
        game->player.animation_timer = 0; 
        *app_state = InGame; 
    }
    else if (*app_state == InWinScreen && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) *app_state = InMenu; 
    else if (*app_state == InLoseScreen && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) *app_state = InMenu; 
}

void render_menu(struct Menu *menu, SDL_Renderer *renderer, enum AppState app_state) {
    if (app_state == InMenu) SDL_SetRenderDrawColor(renderer, 0, 0, 100, 0255); 
    else if (app_state == InWinScreen) SDL_SetRenderDrawColor(renderer, 0, 100, 0, 0255); 
    else if (app_state == InLoseScreen) SDL_SetRenderDrawColor(renderer, 100, 0, 0, 0255); 
    SDL_RenderClear(renderer); 
}

void run_app(struct App *app) {
    unsigned long last_time = SDL_GetPerformanceCounter(); 
    unsigned long frequency = SDL_GetPerformanceFrequency(); 
    float frame_delay = 1.0/60;  
    int window_w, window_h; SDL_GetWindowSize(app->window, &window_w, &window_h); 
    while (app->running) {
        unsigned long current_time = SDL_GetPerformanceCounter(); 
        app->delta_time = (current_time - last_time)/(float)frequency; 

        for (SDL_Event event; SDL_PollEvent(&event); ) {
            if (event.type == SDL_QUIT) app->running = 0; 
            else if (app->state == InGame) handle_game_event(&app->game, event, &app->state); 
            else if (app->state == InMenu || app->state == InWinScreen || app->state == InLoseScreen) handle_menu_event(&app->menu, event, &app->state, &app->game); 
        }

        SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255); 
        SDL_RenderClear(app->renderer); 

        if (app->state == InGame) {
            update_game(&app->game, app->delta_time, &app->state); 
            render_game(&app->game, app->renderer, window_w, window_h); 
        }
        else render_menu(&app->menu, app->renderer, app->state); 

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