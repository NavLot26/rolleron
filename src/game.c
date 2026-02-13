#include <stdlib.h> 
#include <stdio.h> 
#include <SDL.h> 
#include <SDL_image.h> 
#include <SDL_mixer.h> 

#include "lib.c"

// sound channels 
#define LEFT_THRUSTER_CHANNEL 0
#define RIGHT_THRUSTER_CHANNEL 1
#define ALARM_CHANNEL 2
#define BOOST_CHANNEL 3
#define DRAG_CHANNEL 4
#define FORCE_CHANNEL 5
#define GRAVITY_CHANNEL 6

#define CAM_W 12.0
#define CAM_H 6.75


// particle system
struct Particle {
    float x, y; 
    float x_vel, y_vel; 
    float size; 
}; 

void update_particles(struct Particle particles[], unsigned num_particles, float delta_time, float life_time, unsigned char map[MAP_H][MAP_W]) {
    for (int i = 0; i < num_particles; ++i) {
        if (particles[i].size > 0) {
            particles[i].x += particles[i].x_vel * delta_time; 
            particles[i].y += particles[i].y_vel * delta_time; 
            particles[i].size -= 1/life_time * delta_time; 
            // if the updated position is inside of something, then it should be gone 
            if (0 > particles[i].x || particles[i].x >= MAP_W || 0 > particles[i].y || particles[i].y >= MAP_H || map[(int)floorf(particles[i].y)][(int)floorf(particles[i].x)] == Solid || map[(int)floorf(particles[i].y)][(int)floorf(particles[i].x)] == Gravity || map[(int)floorf(particles[i].y)][(int)floorf(particles[i].x)] == AntiGravity) particles[i].size = 0; 
        }
    }
}

void render_particles(SDL_Renderer *renderer, struct Particle particles[], unsigned num_particles, float player_x, float player_y) {
    SDL_Rect viewport; 
    SDL_RenderGetViewport(renderer, &viewport); 
    SDL_SetRenderDrawColor(renderer, 123, 226 , 237, 255); 
    for (int i = 0; i < num_particles; ++i) {
        if (particles[i].size > 0) {
            int screen_x = (particles[i].x - player_x + CAM_W/2 - particles[i].size/2)/CAM_W * viewport.w; 
            int screen_y = viewport.h - ((particles[i].y - player_y + CAM_H/2 + particles[i].size/2)/CAM_H * viewport.h); 
            SDL_RenderFillRect(renderer, &(SDL_Rect){screen_x, screen_y, particles[i].size/CAM_W * viewport.w, particles[i].size/CAM_H * viewport.h}); 
        }  
    }
}

// general texture renderer for game objects
void render_texture(SDL_Renderer *renderer, SDL_Texture *texture, float x, float y, float w, float h, float angle, float anchor_x, float anchor_y) {
    SDL_Rect viewport; 
    SDL_RenderGetViewport(renderer, &viewport); 

    SDL_RenderCopyEx(renderer, texture, NULL, &(SDL_Rect){(x-anchor_x)/CAM_W * viewport.w, viewport.h - (y-anchor_y + h)/CAM_H * viewport.h, w/CAM_W * viewport.w, h/CAM_H * viewport.h}, angle * -180/M_PI, &(SDL_Point){anchor_x/CAM_W * viewport.w, anchor_y/CAM_H * viewport.h}, SDL_FLIP_NONE); 
}




struct Game {
    unsigned char map[MAP_H][MAP_W]; 
    // some tiles need higher resolution than others 
    SDL_Texture *low_res_tiles; 
    SDL_Texture *high_res_tiles; 

    Mix_Music* music; 

    Mix_Chunk *alarm_sound; 
    Mix_Chunk *boost_sound; 
    Mix_Chunk *drag_sound; 
    Mix_Chunk *force_sound; 
    Mix_Chunk *gravity_sound; 
    Mix_Chunk *win_sound; 

    struct Player { 
        // basic underlying game info 
        enum PlayerState {Playing, Exploding, Winning} state; 
        float x, y; 
        float vel_x, vel_y; 
        float rot, rot_vel; 
        int right_thruster_control; 
        int left_thruster_control; 

        SDL_Texture *texture; 

        // particles for thruster, explosion, and force tiles 
        struct Particle particles[30]; 
        unsigned next_particle_i; 
        float particle_timer; 

        struct Particle explosion_particles[64]; 

        struct Particle force_particles[12]; 
        unsigned next_force_particle_i; 
        float force_particle_timer; 

        SDL_Texture *drag_creasent; 

        Mix_Chunk *thruster_sound; 
        Mix_Chunk *explosion_sound; 

        // caches for collisions and graviy force so functions can share the easily and not recalculate
        int collision_cache[17]; 
        float grav_cache_x; 
        float grav_cache_y; 
    } player; 

    // timer info 
    float record; 
    float timer; 

    SDL_Texture *timer_texture; 
    float timer_animation_timer; 
    
    // for display at top 
    SDL_Texture *level_name; 
}; 

void init_game(struct Game *game, SDL_Renderer *renderer) { 
    game->low_res_tiles = IMG_LoadTexture(renderer, "assets/low_res_tiles.png"); 
    game->high_res_tiles = IMG_LoadTexture(renderer, "assets/high_res_tiles.png"); 

    game->music = Mix_LoadMUS("assets/music.mp3"); 
    Mix_VolumeMusic(32); 

    // sounds 
    Mix_ReserveChannels(7); // left thruster, right thruster, alarm, boost, drag, force, gravity
    game->alarm_sound = Mix_LoadWAV("assets/alarm.wav"); 
    game->boost_sound = Mix_LoadWAV("assets/boost.wav"); 
    game->drag_sound = Mix_LoadWAV("assets/drag.wav"); 
    game->force_sound = Mix_LoadWAV("assets/force.wav"); 
    game->gravity_sound = Mix_LoadWAV("assets/gravity.wav"); 
    game->win_sound = Mix_LoadWAV("assets/win.wav"); 
    Mix_Volume(GRAVITY_CHANNEL, 0); 

    // player 
    game->player.texture = IMG_LoadTexture(renderer, "assets/space_ship.png"); 
    game->player.drag_creasent = IMG_LoadTexture(renderer, "assets/drag_creasent.png"); 
    game->player.thruster_sound = Mix_LoadWAV("assets/thruster.wav");
    game->player.explosion_sound = Mix_LoadWAV("assets/explosion.wav");  
}

void cleanup_game(struct Game *game) {
    Mix_FreeChunk(game->player.explosion_sound); 
    Mix_FreeChunk(game->player.thruster_sound);
    SDL_DestroyTexture(game->player.drag_creasent); 
    SDL_DestroyTexture(game->player.texture); 

    Mix_FreeChunk(game->win_sound); 
    Mix_FreeChunk(game->gravity_sound); 
    Mix_FreeChunk(game->force_sound); 
    Mix_FreeChunk(game->drag_sound); 
    Mix_FreeChunk(game->boost_sound); 
    Mix_FreeChunk(game->alarm_sound); 

    Mix_FreeMusic(game->music); 

    SDL_DestroyTexture(game->high_res_tiles); 
    SDL_DestroyTexture(game->low_res_tiles); 
}

void enter_game(struct Game *game, char *level_path, TTF_Font *font, SDL_Renderer *renderer) {
    // read in from the file all important info 
    FILE *file = fopen(level_path, "rb"); 
    char level_name[32]; 
    fread(level_name, sizeof(level_name), 1, file); 
    fread(&game->player.x, sizeof(float), 1, file); 
    fread(&game->player.y, sizeof(float), 1, file); 
    fread(&game->player.rot, sizeof(float), 1, file); 
    fread(&game->record, sizeof(float), 1, file);
    fread(&game->map, sizeof(game->map), 1, file); 
    fclose(file); 

    // get base play info
    game->player.state = Playing; 
    game->player.vel_x = 0; game->player.vel_y = 0; 
    game->player.rot_vel = 0; 
    game->player.right_thruster_control = 0; game->player.left_thruster_control = 0;

    // initialize particles 
    for (int i = 0; i < 30; ++i) game->player.particles[i].size = 0; 
    game->player.next_particle_i = 0; 
    game->player.particle_timer = 0; 

    for (int i = 0; i < 12; ++i) game->player.force_particles[i].size = 0; 
    game->player.next_force_particle_i = 0; 
    game->player.force_particle_timer = 0; 

    Mix_PlayMusic(game->music, -1); 

    game->timer = 0; 

    init_text(&game->timer_texture, "0.0", font, (SDL_Color){0, 180, 0, 255}, renderer); 
    game->timer_animation_timer = 0; 

    init_text(&game->level_name, level_name, font, (SDL_Color){0, 180, 180, 255}, renderer); 

}

void exit_game(struct Game *game, char *level_path, enum LevelType last_type, unsigned last_id, unsigned *num_completed) {
    Mix_FadeOutMusic(500);
    SDL_DestroyTexture(game->timer_texture); 
    SDL_DestroyTexture(game->level_name); 

    // if they won in less time than the record, update the record 
    if (game->player.state == Winning && game->timer < game->record) {
        FILE *file = fopen(level_path, "r+b"); 
        fseek(file, 32 * sizeof(char) + 3 * sizeof(float), SEEK_SET); 
        fwrite(&game->timer, sizeof(float), 1, file); 
        fclose(file); 
    }

    // if they won and unlocked a new level, update that progress data
    if (game->player.state == Winning && last_type == OfficialLevel && last_id == *num_completed) {
        ++*num_completed; 
        FILE *progress = fopen("levels/progress.dat", "r+b"); 

        fwrite(num_completed, sizeof(unsigned), 1, progress); 

        fclose(progress); 
    }
}





// only called if playing 
void update_player_caches(struct Player *player, unsigned char map[MAP_H][MAP_W]) {
    // reset the cache 
    for (int i = 0; i < 17; ++i) player->collision_cache[i] = 0; 

    // player has six collision points that we check for 
    struct {float x, y;} collision_points[6] = { 
        {player->x + cosf(player->rot) * 0.75 * 0.5, player->y + sinf(player->rot) * 0.75 * 0.5}, // front
        {player->x - cosf(player->rot) * 11.0/64 * 0.5, player->y - sinf(player->rot) * 11.0/64 * 0.5}, // back 

        {player->x + (-0.25 * 0.5 * cosf(player->rot) - 0.5 * 0.5 * sinf(player->rot)), player->y + (-0.25 * 0.5 * sinf(player->rot) + 0.5 * 0.5 * cosf(player->rot))}, // back left thruster
        {player->x + (-0.25 * 0.5 * cosf(player->rot) - -0.5 * 0.5 * sinf(player->rot)), player->y + (-0.25 * 0.5 * sinf(player->rot) + -0.5 * 0.5 * cosf(player->rot))}, // back right thruster

        {player->x + (0.25 * 0.5 * cosf(player->rot) - 27.0/64 * 0.5 * sinf(player->rot)), player->y + (0.25 * 0.5 * sinf(player->rot) + 27.0/64 * 0.5 * cosf(player->rot))}, // front left thruster
        {player->x + (0.25 * 0.5 * cosf(player->rot) - -27.0/64 * 0.5 * sinf(player->rot)), player->y + (0.25 * 0.5 * sinf(player->rot) + -27.0/64 * 0.5 * cosf(player->rot))}, // front right thruster
    }; 

    // fill the cache based on which tiles are collided
    for (int i = 0; i < 6; ++i) {
        if (0 <= collision_points[i].x && collision_points[i].x < MAP_W && 0 <= collision_points[i].y && collision_points[i].y < MAP_H) player->collision_cache[map[(int)floorf(collision_points[i].y)][(int)floorf(collision_points[i].x)]] = 1; 
        else player->collision_cache[Solid] = 1;  
    }

    // gravity and antigravity vector caches (stores force)
    player->grav_cache_x = 0; player->grav_cache_y = 0; 
    for (int row = floorf(player->y - 8); row <= floorf(player->y + 8); ++row) {
        for (int col = floorf(player->x - 8); col <= floorf(player->x + 8); ++col) {
            float dist_squared = (powf(col + 0.5 - player->x, 2) + powf(row + 0.5 - player->y, 2));
            if (row >= 0 && row < MAP_H && col >= 0 && col < MAP_W && dist_squared <= 64 && (map[row][col] == Gravity || map[row][col] == AntiGravity)) {
                float dist = sqrt(dist_squared); 
                player->grav_cache_x += (col + 0.5 - player->x)/dist * 3/dist_squared * (map[row][col] == AntiGravity? -1: 1); 
                player->grav_cache_y += (row + 0.5 - player->y)/dist * 3/dist_squared * (map[row][col] == AntiGravity? -1: 1); 
            }
        }
    }
}

// only called if playing 
void update_player_state(struct Player *player, Mix_Chunk *win_sound) {
    // state change to explosion 
    if (player->state == Playing && (player->collision_cache[Solid] || player->collision_cache[Gravity] || player->collision_cache[AntiGravity])) {
        player->state = Exploding; 
        // explositon particles generation
        for (int i = 0; i < 64; ++i) {
            player->explosion_particles[i].x = player->x; 
            player->explosion_particles[i].y = player->y; 
            float rot = (float)rand()/RAND_MAX * 6.28;
            float speed = 0.5 + (float)rand()/RAND_MAX * 3; 
            player->explosion_particles[i].x_vel = cos(rot) * speed; 
            player->explosion_particles[i].y_vel = sin(rot) * speed; 
            player->explosion_particles[i].size = 0.1 + (float)rand()/RAND_MAX * 0.1; 
        }

        // sound
        Mix_HaltMusic(); 
        for (int i = 0; i < 8; ++i) Mix_HaltChannel(i);
        Mix_PlayChannel(-1, player->explosion_sound, 0); 
    }

    // state change to win 
    if (player->state == Playing && player->collision_cache[Winning]) {
        player->state = Winning; 
        for (int i = 0; i < 8; ++i) Mix_HaltChannel(i);
        Mix_PlayChannel(-1, win_sound, 0); 
        Mix_VolumeMusic(64); 
    }
}


void update_timer(struct Game *game, float delta_time, TTF_Font *font, SDL_Renderer *renderer) {
    // update the timer every tenth of a second 
    if (game->player.vel_x != 0 || game->player.vel_y != 0 || game->player.rot_vel != 0) {
        game->timer += delta_time; 
        game->timer_animation_timer += delta_time; 
        if (game->timer_animation_timer > 0.1) {
            game->timer_animation_timer -= 0.1; 
            
            SDL_DestroyTexture(game->timer_texture); 
            char string[8]; sprintf(string, "%.1f", game->timer); 
            SDL_Color color = (game->timer < game->record) ? (SDL_Color){0, 180, 0, 255} : (SDL_Color){180, 0, 0, 255}; 
            init_text(&game->timer_texture, string, font, color, renderer); 
        }
    }
}

// I am going to keep this as one function since it does a lot of small inter-related things
void update_player_movement(struct Player *player, float delta_time) {
    float net_force_x = 0, net_force_y = 0; 
    float net_torque = 0;

    // thrusters 
    if ((player->right_thruster_control || player->collision_cache[ThrustersOn]) && !player->collision_cache[ThrustersOff]) {
        float power = player->collision_cache[StrongerThrusters]? 2: player->collision_cache[WeakerThrusters]? 0.5: 1; 
        net_force_x += cosf(player->rot) * power; 
        net_force_y += sinf(player->rot) * power;  
        net_torque += 0.25 * power;  
    }
    if ((player->left_thruster_control || player->collision_cache[ThrustersOn]) && !player->collision_cache[ThrustersOff]) {
        float power = player->collision_cache[StrongerThrusters]? 2: player->collision_cache[WeakerThrusters]? 0.5: 1; 
        net_force_x += cosf(player->rot) * power; 
        net_force_y += sinf(player->rot) * power;  
        net_torque += -0.25 * power; 
    }

    // gravity and antigravity
    net_force_x += player->grav_cache_x; 
    net_force_y += player->grav_cache_y; 

    // drag and boost
    if (player->collision_cache[Drag]) {
        net_force_x -= 0.75 * player->vel_x; 
        net_force_y -= 0.75 * player->vel_y; 
        net_torque -= 0.075 * player->rot_vel; 
    }
    if (player->collision_cache[Boost]) {
        net_force_x += 0.75 * player->vel_x; 
        net_force_y += 0.75 * player->vel_y; 
        net_torque += 0.075 * player->rot_vel; 
    }

    // forces and torques
    if (player->collision_cache[DownForce]) net_force_y -= 1.75; 
    if (player->collision_cache[UpForce]) net_force_y += 1.75; 
    if (player->collision_cache[LeftForce]) net_force_x -= 1.75; 
    if (player->collision_cache[RightForce]) net_force_x += 1.75; 
    if (player->collision_cache[ClockwiseTorque]) net_torque -= 0.2; 
    if (player->collision_cache[CounterClockwiseTorque]) net_torque += 0.2; 

    // force to velocity and position update 
    player->vel_x += net_force_x / 1 * delta_time; player->vel_y += net_force_y / 1 * delta_time; 
    player->x += player->vel_x * delta_time; player->y += player->vel_y * delta_time; 
    player->rot_vel += net_torque / 0.05 * delta_time; // 0.05 is best to balance manuverabliity with challenge 
    player->rot += player->rot_vel * delta_time; 
}

void emit_player_particles(struct Player *player, float delta_time) {
    // check if it is time for a new particle 
    int spawns[2] = {
        (player->left_thruster_control || player->collision_cache[ThrustersOn]) && !player->collision_cache[ThrustersOff], // left
        (player->right_thruster_control || player->collision_cache[ThrustersOn]) && !player->collision_cache[ThrustersOff]
    }; 

    int is_spawn_time = 0; 
    if (spawns[0] || spawns[1]) {
        player->particle_timer += delta_time; 
        if (player->particle_timer >= 0.025) {
            player->particle_timer -= 0.025; 
            is_spawn_time = 1; 
        }
    }

    // if so, emit a new one 
    if (is_spawn_time) {
        int offset_mults[] = {1, -1}; 
        for (int i = 0; i < 2; ++i) {
            if (spawns[i]) {
                player->particles[player->next_particle_i].x = player->x + -0.125 * cosf(player->rot) - offset_mults[i] * 0.2 * sinf(player->rot); 
                player->particles[player->next_particle_i].y = player->y + -0.125 * sinf(player->rot) + offset_mults[i] * 0.2 * cosf(player->rot);
                float rot = player->rot + (float)rand()/RAND_MAX * 0.25 - 0.125; 
                float speed = player->collision_cache[StrongerThrusters]? 2: player->collision_cache[WeakerThrusters]? 0.5: 1; 
                player->particles[player->next_particle_i].x_vel = player->vel_x - cos(rot) * speed; 
                player->particles[player->next_particle_i].y_vel = player->vel_y - sin(rot) * speed; 
                player->particles[player->next_particle_i].size = player->collision_cache[StrongerThrusters]? 0.125: player->collision_cache[WeakerThrusters]? 0.075: 0.1; 
                player->next_particle_i = (player->next_particle_i + 1) % 30; 
            }
        }
    }
}

void emit_player_force_particles(struct Player *player, float delta_time) {
    int should_spawn = (player->collision_cache[LeftForce] || player->collision_cache[RightForce] || player->collision_cache[UpForce] || player->collision_cache[DownForce] || player->collision_cache[ClockwiseTorque] || player->collision_cache[CounterClockwiseTorque]); 
    
    if (should_spawn) {
        int is_spawn_time = 0; 
        player->force_particle_timer += delta_time; 
        if (player->force_particle_timer > 0.025) {
            player->force_particle_timer -= 0.025; 
            is_spawn_time = 1; 
        }

        if (is_spawn_time) {
            // chose whether to come from the fuselodge or the wings, and then emit in the correct direction 
            int choice = rand() > RAND_MAX/2; 
            float start_x, start_y, back_x, end_y; 
            if (choice) {
                start_x = player->x + cosf(player->rot) * 0.375, start_y = player->y + sinf(player->rot) * 0.375; 
                back_x = player->x - cosf(player->rot) * 0.0625, end_y = player->y - sinf(player->rot) * 0.0625; 
            }
            else {
                start_x = player->x + (12.0/16 * -0.125 * cosf(player->rot) - 0.25 * sinf(player->rot)), start_y = player->y + (-0.125 * sinf(player->rot) + 0.25 * cosf(player->rot)); 
                back_x = player->x + (12.0/16 * -0.125 * cosf(player->rot) - -0.25 * sinf(player->rot)), end_y = player->y + (-0.125 * sinf(player->rot) + -0.25 * cosf(player->rot)); 
            }
            float dist = (float)rand()/RAND_MAX; 
            player->force_particles[player->next_force_particle_i].x = choice ? start_x + dist * (back_x - start_x): start_x + dist * (back_x - start_x);  
            player->force_particles[player->next_force_particle_i].y = choice ? start_y + dist * (end_y - start_y): start_y + dist * (end_y - start_y);  
            player->force_particles[player->next_force_particle_i].x_vel = player->vel_x + ((player->collision_cache[LeftForce] && !player->collision_cache[RightForce])? 1.75 : (player->collision_cache[RightForce] && !player->collision_cache[LeftForce])? -1.75: 0); 
            player->force_particles[player->next_force_particle_i].y_vel = player->vel_y + ((player->collision_cache[DownForce] && !player->collision_cache[UpForce])? 1.75 : (player->collision_cache[UpForce] && !player->collision_cache[DownForce])? -1.75: 0);
            player->force_particles[player->next_force_particle_i].size = 0.1; 
            player->next_force_particle_i = (player->next_force_particle_i + 1) % 12; 
        }
    }
}

// keep this together as well since it is made of a bunch of small interrelated parts 
void update_game_sound(struct Game *game) {
    // thrusters on or off
    int left_should_be_playing = (game->player.left_thruster_control || game->player.collision_cache[ThrustersOn]) && !game->player.collision_cache[ThrustersOff]; 
    if (!Mix_Playing(LEFT_THRUSTER_CHANNEL) && left_should_be_playing) Mix_FadeInChannel(LEFT_THRUSTER_CHANNEL, game->player.thruster_sound, -1, 100); 
    else if (Mix_Playing(LEFT_THRUSTER_CHANNEL) && !left_should_be_playing) Mix_FadeOutChannel(LEFT_THRUSTER_CHANNEL, 100); 
    
    int right_should_be_playing = (game->player.right_thruster_control || game->player.collision_cache[ThrustersOn]) && !game->player.collision_cache[ThrustersOff];  
    if (!Mix_Playing(RIGHT_THRUSTER_CHANNEL) && right_should_be_playing) Mix_FadeInChannel(RIGHT_THRUSTER_CHANNEL, game->player.thruster_sound, -1, 100); 
    else if (Mix_Playing(RIGHT_THRUSTER_CHANNEL) && !right_should_be_playing) Mix_FadeOutChannel(RIGHT_THRUSTER_CHANNEL, 100); 

    // thruster volume 
    int correct_volume = game->player.collision_cache[StrongerThrusters]? 128: game->player.collision_cache[WeakerThrusters]? 32: 64; 
    if (Mix_Volume(LEFT_THRUSTER_CHANNEL, -1) != correct_volume) Mix_Volume(LEFT_THRUSTER_CHANNEL, correct_volume); 
    if (Mix_Volume(RIGHT_THRUSTER_CHANNEL, -1) != correct_volume) Mix_Volume(RIGHT_THRUSTER_CHANNEL, correct_volume); 

    // alarm sound 
    if (!Mix_Playing(ALARM_CHANNEL) && (game->player.collision_cache[ThrustersOn] || game->player.collision_cache[ThrustersOff])) Mix_PlayChannel(ALARM_CHANNEL, game->alarm_sound, -1); 
    else if (Mix_Playing(ALARM_CHANNEL) && !game->player.collision_cache[ThrustersOn]  && !game->player.collision_cache[ThrustersOff]) Mix_HaltChannel(ALARM_CHANNEL);  


    // boost 
    if (!Mix_Playing(BOOST_CHANNEL) && (game->player.collision_cache[Boost])) Mix_FadeInChannel(BOOST_CHANNEL, game->boost_sound, -1, 250); 
    else if (Mix_Playing(BOOST_CHANNEL) && !game->player.collision_cache[Boost]) Mix_FadeOutChannel(BOOST_CHANNEL, 250); 
    // boost volume 
    if (game->player.collision_cache[Boost]) {
        float speed = sqrt(game->player.vel_x * game->player.vel_x + game->player.vel_y * game->player.vel_y); 
        int volume = 32 + 15 * speed > 128? 128: 32 + 15 * speed; 
        Mix_Volume(BOOST_CHANNEL, volume); 
    } 

    // drag
    if (!Mix_Playing(DRAG_CHANNEL) && (game->player.collision_cache[Drag])) Mix_FadeInChannel(DRAG_CHANNEL, game->drag_sound, -1, 250); 
    else if (Mix_Playing(DRAG_CHANNEL) && !game->player.collision_cache[Drag]) Mix_FadeOutChannel(DRAG_CHANNEL, 250);
    // drag volume
    if (game->player.collision_cache[Drag]) {
        float speed = sqrt(game->player.vel_x * game->player.vel_x + game->player.vel_y * game->player.vel_y); 
        int volume = 24 * speed > 128? 128: 24 * speed; 
        Mix_Volume(DRAG_CHANNEL, volume); 
    }

    // force
    int force_collision = game->player.collision_cache[UpForce] || game->player.collision_cache[DownForce] || game->player.collision_cache[LeftForce] || game->player.collision_cache[RightForce] || game->player.collision_cache[ClockwiseTorque] || game->player.collision_cache[CounterClockwiseTorque]; 
    if (!Mix_Playing(FORCE_CHANNEL) && force_collision) Mix_FadeInChannel(FORCE_CHANNEL, game->force_sound, -1, 250); 
    else if (Mix_Playing(FORCE_CHANNEL) && !force_collision) Mix_FadeOutChannel(FORCE_CHANNEL, 250); 

    // gravity
    if (!Mix_Playing(GRAVITY_CHANNEL) && (game->player.grav_cache_x != 0 || game->player.grav_cache_y != 0)) Mix_FadeInChannel(GRAVITY_CHANNEL, game->gravity_sound, -1, 150); 
    else if (Mix_Playing(GRAVITY_CHANNEL) && (game->player.grav_cache_x == 0 && game->player.grav_cache_y == 0)) Mix_FadeOutChannel(GRAVITY_CHANNEL, 150); 
    
    // gravity volume
    if (game->player.grav_cache_x != 0 || game->player.grav_cache_y != 0) {
        float mag = sqrt(game->player.grav_cache_x * game->player.grav_cache_x + game->player.grav_cache_y * game->player.grav_cache_y); 
        int volume = mag * 96 > 128? 128: 96 * mag; 
        Mix_Volume(GRAVITY_CHANNEL, volume); 
    }
}

void update_game(struct Game *game, float delta_time, TTF_Font *font, SDL_Renderer *renderer, enum AppState *next_state) {
    // update the caches if they might be needed in the state, then update the player state. Then execute different functions based on the state that was just updated
    if (game->player.state == Playing || game->player.state == Winning) update_player_caches(&game->player, game->map); 
    update_player_state(&game->player, game->win_sound); 
    
    // updates if playing 
    if (game->player.state == Playing) {
        update_timer(game, delta_time, font, renderer); 

        update_player_movement(&game->player, delta_time); 

        update_particles(game->player.particles, 30, delta_time, 3.0, game->map); 
        emit_player_particles(&game->player, delta_time); 

        update_particles(game->player.force_particles, 12, delta_time, 3.0, game->map);
        emit_player_force_particles(&game->player, delta_time); 

        update_game_sound(game); 
    }
    else if (game->player.state == Winning) {
        // update win movement 
        if (game->player.collision_cache[Solid] || game->player.collision_cache[Gravity] || game->player.collision_cache[AntiGravity]) {
            // very basic bouncing that works at such low speed
            game->player.vel_x *= -1; 
            game->player.vel_y *= -1; 
            game->player.rot_vel *= -1; 
        }
        
        game->player.vel_x *= powf(0.005, delta_time); game->player.vel_y *= powf(0.05, delta_time); 
        game->player.x += game->player.vel_x * delta_time; game->player.y += game->player.vel_y * delta_time; 
        game->player.rot_vel *= powf(0.0005, delta_time); 
        game->player.rot += game->player.rot_vel * delta_time;  

        // game exit 
        if (fabs(game->player.vel_x) < 0.005 && fabs(game->player.vel_y) < 0.005 && fabs(game->player.rot_vel) < 0.005) {
            *next_state = InOverlay; 
        }

        

        // update particles (no more emmission, but update)
        update_particles(game->player.particles, 30, delta_time, 3.0, game->map); 
        update_particles(game->player.force_particles, 12, delta_time, 3.0, game->map); 
    }

    else if (game->player.state == Exploding) {
        update_particles(game->player.explosion_particles, 64, delta_time, 5.0, game->map);

        int done_exploding = 1; 
        for (int i = 0; i < 64; ++i) {
            if (game->player.explosion_particles[i].size > 0) {
                done_exploding = 0; 
                break; 
            }
        }

        if (done_exploding) {
            *next_state = InOverlay; 
        }
    }
}





void render_game(struct Game *game, SDL_Renderer *renderer) {
    // tile Background 
    SDL_Rect viewport; 
    SDL_RenderGetViewport(renderer, &viewport); 
    for (int row = floorf(game->player.y - CAM_H/2.0); row <= floorf(game->player.y + CAM_H/2.0); ++row) {
        for (int col = floorf(game->player.x - CAM_W/2.0); col <= floorf(game->player.x + CAM_W/2.0); ++col) {
            // lets do these coordinate transformations clearly 
            int screen_x = (col - game->player.x + CAM_W/2.0)/CAM_W * viewport.w; 
            int screen_y = viewport.h - ((row - game->player.y + CAM_H/2.0 + 1)/CAM_H * viewport.h); 
            int tile_res = (row >= 0 && row < MAP_H && col >= 0 && col < MAP_W && (game->map[row][col] == DownForce || game->map[row][col] == UpForce || game->map[row][col] == LeftForce || game->map[row][col] == RightForce || game->map[row][col] == CounterClockwiseTorque || game->map[row][col] == ClockwiseTorque))? 64: 16; 
            int tile_offset = tile_res == 64? (game->map[row][col] - 11) * 64 : row >= 0 && row < MAP_H && col >= 0 && col < MAP_W? game->map[row][col] * 16: 16; 
            SDL_RenderCopy(renderer, tile_res == 64? game->high_res_tiles: game->low_res_tiles, &(SDL_Rect){tile_offset, 0, tile_res, tile_res}, &(SDL_Rect){screen_x, screen_y, viewport.w/CAM_W + 1, viewport.h/CAM_H + 1});  // add one to dimensions to cover up disconnects 
        }
    }  

    // timer and level name 
    render_menu_text(renderer, game->timer_texture, 0, 0, 1, Left); 
    render_menu_text(renderer, game->level_name, 0, UI_W , 0.75, Right);

    // player and particles 
    if (game->player.state == Playing || game->player.state == Winning) {
        render_particles(renderer, game->player.particles, 30, game->player.x, game->player.y); 
        render_particles(renderer, game->player.force_particles, 12, game->player.x, game->player.y); 

        render_texture(renderer, game->player.texture, CAM_W/2.0, CAM_H/2, 0.5, 0.5, game->player.rot, 0.125, 0.25); 
    }
    // drag creasent and boost trail 
    if (game->player.state == Playing) {
        // drag creasent 
        if (game->player.collision_cache[Drag]) {
            float speed = sqrt(game->player.vel_x * game->player.vel_x + game->player.vel_y * game->player.vel_y); 
            float x_offset = game->player.vel_x/speed * 0.5; 
            float y_offset = game->player.vel_y/speed * 0.5; 

            if (game->map[(int)floorf(game->player.y + y_offset)][(int)floorf(game->player.x + x_offset)] == Drag) { // only apply the drag creasent if their is a drag collision but also the creasent would be on the drag block 
                SDL_SetTextureAlphaMod(game->player.drag_creasent, speed * 48 < 256? speed * 48 : 255);
                render_texture(renderer, game->player.drag_creasent, CAM_W/2.0 + x_offset, CAM_H/2 + y_offset, 0.5, 1, atan2(game->player.vel_y, game->player.vel_x), 0.5, 0.5); 
            }
        }

        // boost trail 
        if (game->player.collision_cache[Boost]) {
            for (int i = 0; i < 8; ++i) {
                float x_offset = game->player.vel_x * -0.005 * i; 
                float y_offset = game->player.vel_y * -0.005 * i; 
                float rot_offset = game->player.rot_vel * -0.025 * i; // do more time back for the ration because it makes the trail look less static and lets the player see the rotation differences 
                SDL_SetTextureAlphaMod(game->player.texture, 80 - 8 * i);
                render_texture(renderer, game->player.texture, CAM_W/2.0 + x_offset, CAM_H/2 + y_offset, 0.5, 0.5, game->player.rot + rot_offset, 0.125, 0.25); 
            }
            SDL_SetTextureAlphaMod(game->player.texture, 255);
        }
    }

    // explosion particles 
    if (game->player.state == Exploding) {
        render_particles(renderer, game->player.explosion_particles, 64, game->player.x, game->player.y); 
    }
}


void handle_game_event(struct Game *game, SDL_Event *event, enum AppState *next_state) {
    if (event->type == SDL_KEYDOWN) {
        // left and right keys 
        if (event->key.keysym.sym == SDLK_LEFT) game->player.left_thruster_control = 1; 
        else if (event->key.keysym.sym == SDLK_RIGHT) game->player.right_thruster_control = 1; 
        
        // pause screen 
        else if (event->key.keysym.sym == SDLK_ESCAPE && game->player.state == Playing) {
            *next_state = InOverlay; 
            // turn both of the thruster controls off
            game->player.left_thruster_control = 0; 
            game->player.right_thruster_control = 0; 
        }
    }
    else if (event->type == SDL_KEYUP) {
        if (event->key.keysym.sym == SDLK_LEFT) game->player.left_thruster_control = 0; 
        else if (event->key.keysym.sym == SDLK_RIGHT) game->player.right_thruster_control = 0; 
    }
}

