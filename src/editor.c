#include <stdlib.h> 
#include <stdio.h> 
#include <SDL.h> 
#include <SDL_image.h> 
#include <SDL_ttf.h> 

#include "lib.c"


void encode_bytes(unsigned bytes, char chars[6]) { // takes in pointer to an unsigned int storing the 4 raw bytes
    // we will take each group of 6 bits and encode it as a character 

    char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    for (int i = 0; i < 6; ++i) {// this represents each character
        // get the bits that the character corosponds to, shifting to the right and anding with 0b111111 to isolate the bytes 
        unsigned int bits = bytes >> (32 - 6 * (i + 1)) & 0b111111; 
        chars[i] = b64[bits]; // turn those bits into base 64
    }
}

void decode_bytes(char chars[6], unsigned *bytes) {
    *bytes = 0; 
    for (int i = 0; i < 6; ++i) {
        unsigned num = (chars[i] >= 'A' && chars[i] <= 'Z') ? (chars[i] - 'A') :
            (chars[i] >= 'a' && chars[i] <= 'z') ? (chars[i] - 'a' + 26) :
            (chars[i] >= '0' && chars[i] <= '9') ? (chars[i] - '0' + 52) :
            (chars[i] == '+') ? 62 :
            (chars[i] == '/') ? 63 : 0;
        *bytes |= num << (32 - 6 * (i + 1)); // use | and bitshifting to add the bytes from that number into the correct position
    }
}


struct Editor {
    // level data 
    char name[32]; 
    float spawn_x, spawn_y; 
    float spawn_rot; 
    unsigned char map[MAP_H][MAP_W]; 

    // level display info 
    float cam_x, cam_y; 
    float cam_w; 

    SDL_Texture *low_res_tiles; 
    SDL_Texture *high_res_tiles; 
    SDL_Texture *player; 

    // buttons 
    struct Button exit; 
    struct Button play; 
    struct Button import; 
    struct Button export; 

    SDL_Texture *name_texture; 

    // tools
    enum Tool {NoneSelected, Rename, Place, Draw} tool; 
    struct Button rename; // no sub tool s

    struct Button place_spawn; // sub tools: 
    struct Button rotate_spawn; 


   
    struct Button draw; // sub tools (excluding the grids, which do not have data)
    struct Button larger; 
    struct Button smaller; 
    enum Tile selected; 
    unsigned size; 
    int brush_down; 

    // these are shared by the place and draw tools 
    float disp_x, disp_y; // these are normalized from 0-1 on the display 

}; 

// no functinoality yet, lets just get something that displays the buttons with a blank level 

void init_editor(struct Editor *editor, SDL_Renderer *renderer, TTF_Font *font) {
    editor->low_res_tiles = IMG_LoadTexture(renderer, "assets/low_res_tiles.png"); 
    editor->high_res_tiles = IMG_LoadTexture(renderer, "assets/high_res_tiles.png"); 
    editor->player = IMG_LoadTexture(renderer, "assets/space_ship.png"); 

    // just buttons
    init_special_text_button(&editor->exit, 0, 0, 1, IMG_LoadTexture(renderer, "assets/exit.png")); 
    init_special_text_button(&editor->play, 0, 2, 1, IMG_LoadTexture(renderer, "assets/play.png")); 
    init_special_text_button(&editor->import, 1, 0, 1, IMG_LoadTexture(renderer, "assets/import.png")); 
    init_special_text_button(&editor->export, 1, 2, 1, IMG_LoadTexture(renderer, "assets/export.png")); 

    init_special_text_button(&editor->rename, 3, 0, 1, IMG_LoadTexture(renderer, "assets/rename.png")); 

    init_special_text_button(&editor->place_spawn, 3, 1, 1, IMG_LoadTexture(renderer, "assets/place_spawn.png")); 
    init_special_text_button(&editor->rotate_spawn, 5, 0, 1, IMG_LoadTexture(renderer, "assets/rotate_spawn.png")); 

    init_special_text_button(&editor->draw, 3, 2, 1, IMG_LoadTexture(renderer, "assets/edit.png")); 

    init_button(&editor->larger, 12, 0, 1, "+",  font, renderer); 
    init_button(&editor->smaller, 12, 2, 1, "-",  font, renderer); 
}

void cleanup_editor(struct Editor *editor) {
    SDL_DestroyTexture(editor->smaller.text); 
    SDL_DestroyTexture(editor->larger.text); 
    SDL_DestroyTexture(editor->draw.text); 
    SDL_DestroyTexture(editor->rotate_spawn.text); 
    SDL_DestroyTexture(editor->place_spawn.text); 
    SDL_DestroyTexture(editor->rename.text); 
    SDL_DestroyTexture(editor->export.text); 
    SDL_DestroyTexture(editor->import.text); 
    SDL_DestroyTexture(editor->play.text); 
    SDL_DestroyTexture(editor->exit.text); 

    SDL_DestroyTexture(editor->high_res_tiles); 
    SDL_DestroyTexture(editor->low_res_tiles); 
    SDL_DestroyTexture(editor->player); 
}

void enter_editor_state(struct Editor *editor, SDL_Renderer *renderer, TTF_Font *font, char *level_path) {
    // read out of the file path 
    FILE *file = fopen(level_path, "rb"); 
    fread(editor->name, sizeof(editor->name), 1, file); 
    fread(&editor->spawn_x, sizeof(float), 1, file); 
    fread(&editor->spawn_y, sizeof(float), 1, file); 
    fread(&editor->spawn_rot, sizeof(float), 1, file); 
    fseek(file, sizeof(float), SEEK_CUR); // skip record 
    fread(editor->map, sizeof(editor->map), 1, file); 
    fclose(file); 

    editor->cam_x = MAP_W/2.0; 
    editor->cam_y = MAP_H/2.0; 
    editor->cam_w = 60; 

    init_text(&editor->name_texture, editor->name, font, (SDL_Color){0, 180, 180, 255}, renderer); 

    editor->tool = NoneSelected; 
    editor->selected = Solid; 
    editor->size = 1; 
    editor->smaller.enabled = 0; 
    editor->larger.enabled = 1; 

    // these do not matter since the start mode is not draw anyway
    editor->disp_x = -1; 
    editor->disp_y = -1; 

    editor->place_spawn.enabled = 1; 
    editor->rename.enabled = 1; 
    editor->draw.enabled = 1; 


}

void exit_editor_state(struct Editor *editor, char *level_path) {
    SDL_DestroyTexture(editor->name_texture);
    
    unsigned char old_map[MAP_H][MAP_W]; 
    float old_spawn_x, old_spawn_y, old_spawn_rot; 
    float record;  

    FILE *file = fopen(level_path, "r+b"); 
    fseek(file, sizeof(editor->name), SEEK_SET); // skip name 
    fread(&old_spawn_x, sizeof(float), 1, file); 
    fread(&old_spawn_y, sizeof(float), 1, file); 
    fread(&old_spawn_rot, sizeof(float), 1, file); 
    fread(&record, sizeof(float), 1, file); 
    fread(old_map, sizeof(editor->map), 1, file); 

    // so if the map or spawn info is different, then we need to reset the record 

    if (editor->spawn_x != old_spawn_x || editor->spawn_y != old_spawn_y || editor->spawn_rot != old_spawn_rot) record = INFINITY; 
    else {
        for (int row = 0; row < MAP_H; ++row) {
            for (int col = 0; col < MAP_W; ++col) {
                if (editor->map[row][col] != old_map[row][col]) record = INFINITY; 
            }
        }
    }

    fseek(file, 0, SEEK_SET);
    fwrite(editor->name, sizeof(editor->name), 1, file); 
    fwrite(&editor->spawn_x, sizeof(float), 1, file); 
    fwrite(&editor->spawn_y, sizeof(float), 1, file); 
    fwrite(&editor->spawn_rot, sizeof(float), 1, file); 
    fwrite(&record, sizeof(float), 1, file); 
    fwrite(editor->map, sizeof(editor->map), 1, file); 
    fclose(file);     
}


// fix this whole tile inconsistancy thing, figure out a better system of ownership 
// I need consistant abstractions shared between different parts of the program, so get on that once this is done 
void render_editor(struct Editor *editor, SDL_Renderer *renderer, SDL_Texture *tiles, int mr, int mc) {
    SDL_Rect viewport; 
    SDL_RenderGetViewport(renderer, &viewport); 

    float cam_h = editor->cam_w * UI_H/(UI_W - 3.25); 
    int display_w = (UI_W - 3.25)/UI_W * viewport.w; 

    SDL_RenderGetViewport(renderer, &viewport); 
    for (int row = floorf(editor->cam_y - cam_h/2.0); row <= floorf(editor->cam_y + cam_h/2.0); ++row) {
        for (int col = floorf(editor->cam_x - editor->cam_w/2.0); col <= floorf(editor->cam_x + editor->cam_w/2.0); ++col) {
            
            int screen_x = (col - editor->cam_x + editor->cam_w/2.0)/editor->cam_w * display_w + 3.25/UI_W * viewport.w; 
            int screen_y = viewport.h - (row - editor->cam_y + cam_h/2.0 + 1)/cam_h * viewport.h; 

            int tile_res = (row >= 0 && row < MAP_H && col >= 0 && col < MAP_W && (editor->map[row][col] >= DownForce && editor->map[row][col] <= ClockwiseTorque))? 64: 16; 
            int tile_offset = tile_res == 64? (editor->map[row][col] - 11) * 64 : row >= 0 && row < MAP_H && col >= 0 && col < MAP_W? editor->map[row][col] * 16: 16; 
            SDL_RenderCopy(renderer, tile_res == 64? editor->high_res_tiles: editor->low_res_tiles, &(SDL_Rect){tile_offset, 0, tile_res, tile_res}, &(SDL_Rect){screen_x, screen_y, display_w/editor->cam_w + 1, viewport.h/cam_h + 1});  // add one to dimensions to cover up disconnects 
        }
    }
    // player 
    int screen_x = (editor->spawn_x - editor->cam_x + editor->cam_w/2.0 - 0.125)/editor->cam_w * display_w + 3.25/UI_W * viewport.w; 
    int screen_y = viewport.h - (editor->spawn_y - editor->cam_y + cam_h/2.0 - 0.25 + 0.5)/cam_h * viewport.h; 
    SDL_RenderCopyEx(renderer, editor->player, NULL, &(SDL_Rect){screen_x, screen_y, 0.5/editor->cam_w * display_w, 0.5/cam_h * viewport.h}, editor->spawn_rot * -180/M_PI, &(SDL_Point){0.125/editor->cam_w * display_w, 0.25/cam_h * viewport.h}, SDL_FLIP_NONE); 
    
    // drawing outline 
    if (editor->tool == Draw) {
        // get map x and map y based on the normalized display mouse coordinate 
        float map_x = editor->disp_x * editor->cam_w + (editor->cam_x - editor->cam_w/2.0);  
        float map_y = (1 - editor->disp_y) * cam_h + (editor->cam_y - cam_h/2.0); 

        if (0 < map_x && map_x < MAP_W && 0 < map_y && map_y < MAP_H) {
            // get row and col at the top left of the draw square 
            int row = roundf(map_y + editor->size/2.0); 
            int col = roundf(map_x - editor->size/2.0); 

            // turn those into pixil coordinates 
            int p_x = (col - editor->cam_x + editor->cam_w/2.0)/editor->cam_w * display_w + 3.25/UI_W * viewport.w; 
            int p_y = viewport.h - (row - editor->cam_y + cam_h/2.0)/cam_h * viewport.h; 


            int p_width = (float)editor->size/editor->cam_w * display_w; 
            int p_height = (float)editor->size/cam_h * viewport.h; 

            int thickness = 0.05/UI_W * viewport.w; // just use the same thickness as on buttons, regardless of the camera zoom 


            // left
            SDL_RenderFillRect(renderer, &(SDL_Rect){p_x, p_y, thickness, p_height}); 
            // right
            SDL_RenderFillRect(renderer, &(SDL_Rect){p_x + p_width - thickness, p_y, thickness, p_height}); 
            // top 
            SDL_RenderFillRect(renderer, &(SDL_Rect){p_x, p_y, p_width, thickness}); 
            // bottom 
            SDL_RenderFillRect(renderer, &(SDL_Rect){p_x, p_y + p_height - thickness, p_width, thickness}); 
        }
    }

    

    // place shadow of ship  
    if (editor->tool == Place) {
        float map_x = editor->disp_x * editor->cam_w + (editor->cam_x - editor->cam_w/2.0);  
        float map_y = (1 - editor->disp_y) * cam_h + (editor->cam_y - cam_h/2.0); 

        if (0 < map_x && map_x < MAP_W && 0 < map_y && map_y < MAP_H) {
            int screen_x = (map_x - editor->cam_x + editor->cam_w/2.0 - 0.125)/editor->cam_w * display_w + 3.25/UI_W * viewport.w; 
            int screen_y = viewport.h - (map_y - editor->cam_y + cam_h/2.0 - 0.25 + 0.5)/cam_h * viewport.h; 
            SDL_SetTextureAlphaMod(editor->player, 128);
            SDL_RenderCopyEx(renderer, editor->player, NULL, &(SDL_Rect){screen_x, screen_y, 0.5/editor->cam_w * display_w, 0.5/cam_h * viewport.h}, editor->spawn_rot * -180/M_PI, &(SDL_Point){0.125/editor->cam_w * display_w, 0.25/cam_h * viewport.h}, SDL_FLIP_NONE);
            SDL_SetTextureAlphaMod(editor->player, 255);
        }

    }

    else if (editor->tool == Rename) {
        render_menu_outline(renderer, 0, 4, 5, 1); 
    }

    // tool bar background 
    for (int row = 0; row < UI_H; ++row) {
        for (int col = 0; col < 3; ++col) {
            render_menu_tile(renderer, tiles, Blank, row, col); 
        }
    }   
    // buttons 
    render_button(&editor->exit, renderer, tiles, mr, mc); 
    render_button(&editor->play, renderer, tiles, mr, mc); 
    render_button(&editor->import, renderer, tiles, mr, mc); 
    render_button(&editor->export, renderer, tiles, mr, mc); 
    

    // tools 
    render_button(&editor->rename, renderer, tiles, mr, mc); 
    render_button(&editor->place_spawn, renderer, tiles, mr, mc); 
    render_button(&editor->draw, renderer, tiles, mr, mc); 

    if (editor->tool == Place) {
        render_button(&editor->rotate_spawn, renderer, tiles, mr, mc); 
    }
    else if (editor->tool == Draw) {
        render_button(&editor->larger, renderer, tiles, mr, mc); 
        render_button(&editor->smaller, renderer, tiles, mr, mc); 

        for (int row = 5; row < 11; ++row) {
            for (int col = 0; col < 3; ++col) {
                int tile = (row - 5) * 3 + col; 
                int tile_res = tile >= DownForce? 64 : 16;  
                int tile_offset = tile_res == 64? (tile - 11) * 64: tile * 16; 
                SDL_RenderCopy(renderer, tile_res == 64? editor->high_res_tiles: editor->low_res_tiles, &(SDL_Rect){tile_offset, 0, tile_res, tile_res},  &(SDL_Rect){(float)col/UI_W * viewport.w, (float)row/UI_H * viewport.h, 1.0/UI_W * viewport.w + 1, 1.0/UI_H * viewport.h + 1}); 
            }
        }

        int row = editor->selected/ 3 + 5; 
        int col = editor->selected % 3; 
        SDL_SetRenderDrawColor(renderer,180, 180, 180, 255); 
        render_menu_outline(renderer, row, col, 1, 1); 


        if (editor->tool == Draw && 5 <= mr && mr <= 10 && mc <= 2 && !(mr == 10 && mc == 2)) { 
            SDL_SetRenderDrawColor(renderer, 0, 180, 180, 255); 
            render_menu_outline(renderer, mr, mc, 1, 1); 
        }

    }

    // seperator 
    SDL_SetRenderDrawColor(renderer, 0, 180 , 180, 255); 
    SDL_RenderFillRect(renderer, &(SDL_Rect){3.0/UI_W * viewport.w, 0, 0.25/UI_W * viewport.w, viewport.h}); 

    // name 
    render_menu_text(renderer, editor->name_texture, 0, 4, 0.75, Left); 


   




}

void update_editor(struct Editor *editor) {
    if (editor->tool == Draw && editor->brush_down) {
        float cam_h = editor->cam_w * UI_H/(UI_W - 3.25); 

        float map_x = editor->disp_x * editor->cam_w + (editor->cam_x - editor->cam_w/2.0);  
        float map_y = (1 - editor->disp_y) * cam_h + (editor->cam_y - cam_h/2.0); 

        if (0 < map_x && map_x < MAP_W && 0 < map_y && map_y < MAP_H) {
            // get row and col at the top left of the draw square 
            int row = roundf(map_y - editor->size/2.0); 
            int col = roundf(map_x - editor->size/2.0); 

            for (int i = row; i < row + (int)editor->size; ++i) {
                for (int j = col; j < col + (int)editor->size; ++j) {
                    if (0 <= i && i < MAP_H && 0 <= j && j < MAP_W) editor->map[i][j] = editor->selected; 
                }
            }
        }
    }
    
}

void handle_editor_event(struct Editor *editor, SDL_Event *event, SDL_Renderer *renderer, TTF_Font *font, enum AppState *next_state) {
    // buttons 
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        unsigned mrow, mcol; 
        get_mouse_coords(event->button.x, event->button.y, renderer, &mrow, &mcol); 

        if (is_mouse_over_button(&editor->rename, mrow, mcol)) {
            editor->rename.enabled = 0; 
            editor->place_spawn.enabled = 1; 
            editor->draw.enabled = 1; 
            editor->tool = Rename; 
        }
        else if (is_mouse_over_button(&editor->place_spawn, mrow, mcol)) {
            editor->rename.enabled = 1; 
            editor->place_spawn.enabled = 0; 
            editor->draw.enabled = 1; 
            editor->tool = Place; 
        }
        else if (is_mouse_over_button(&editor->draw, mrow, mcol)) {
            editor->rename.enabled = 1; 
            editor->place_spawn.enabled = 1; 
            editor->draw.enabled = 0; 
            editor->tool = Draw; 
        }

        else if (editor->tool == Draw && 5 <= mrow && mrow <= 10 && mcol <= 2 && !(mrow == 10 && mcol == 2)) {
            editor->selected = (mrow - 5) * 3 + mcol; 
        }
        else if (editor->tool == Draw && is_mouse_over_button(&editor->larger, mrow, mcol) && editor->larger.enabled) {
            ++editor->size; 
            if (editor->size == 6) editor->larger.enabled = 0; 
            if (editor->size > 1) editor->smaller.enabled = 1; 
        }
        else if (editor->tool == Draw && is_mouse_over_button(&editor->smaller, mrow, mcol) && editor->smaller.enabled) {
            --editor->size; 
            if (editor->size == 1) editor->smaller.enabled = 0; 
            if (editor->size < 6) editor->larger.enabled = 1; 
        } 

        else if (editor->tool == Place && is_mouse_over_button(&editor->rotate_spawn, mrow, mcol)) editor->spawn_rot -= M_PI/4; 


        else if (is_mouse_over_button(&editor->export, mrow, mcol)) {
            // we will construct the hash by using the name, adding the 3 float encodings, and then adding one character per tile 
            // this will be 32 + 6 + 6 + 6 + 32x48

            char result[32 + 6 + 6 + 6 + (32 * 48) + 1]; 
            result[32 + 6 + 6 + 6 + (32 * 48)] = '\0'; 

            unsigned name_len = strlen(editor->name); 
            for (int i = 0; i < 32; ++i) result[i] = (i < name_len) ? editor->name[i] : '=';
            
            // next lets encode each of the floats and add them 

            // we get the adress, cast it to an unsigned int pointer, then deref the pointer
            encode_bytes(*(unsigned *)&editor->spawn_x, result + 32); 
            encode_bytes(*(unsigned *)&editor->spawn_y, result + 32 + 6); 
            encode_bytes(*(unsigned *)&editor->spawn_rot, result + 32 + 6 + 6); 
            
            char tile_chars[] = "abcdefghijklmnopq"; 
            // just encode each tile as a character rather than using base 64 with compressed 5 bit tiles, since it is so much simpler while only having about 300 extra characters
            for (int row = 0; row < MAP_H; ++row) {
                for (int col = 0; col < MAP_W; ++col) {
                    result[32 + 6 + 6 + 6 + row * MAP_W + col] = tile_chars[editor->map[row][col]]; 
                }
            }

            SDL_SetClipboardText(result);

        }

        else if (is_mouse_over_button(&editor->import, mrow, mcol)) {
            char *import = SDL_GetClipboardText();

            if (strlen(import) == 32 + 6 + 6 + 6 + 32 * 48) {
                // skip name for now and come back to it because idk how to approach unknown length issue 
                decode_bytes(import + 32, (unsigned *)&editor->spawn_x); 
                decode_bytes(import + 32 + 6, (unsigned *)&editor->spawn_y); 
                decode_bytes(import + 32 + 6 + 6, (unsigned *)&editor->spawn_rot); 

                for (int row = 0; row < MAP_H; ++row) {
                    for (int col = 0; col < MAP_W; ++col) {
                        editor->map[row][col] = import[32 + 6 + 6 + 6 + row * MAP_W + col] - 'a'; 
                    }
                }
            }
            
        }

        else if (is_mouse_over_button(&editor->exit, mrow, mcol)) *next_state = InCustom; 


        else if (is_mouse_over_button(&editor->play, mrow, mcol)) *next_state = InGame; // level type and id are already set from editor 
    }
    // movement 
    else if (event->type == SDL_KEYDOWN) {
        float step = editor->cam_w/40.0; 
        if (event->key.keysym.sym == SDLK_LEFT) editor->cam_x -= step; 
        else if (event->key.keysym.sym == SDLK_RIGHT) editor->cam_x += step; 
        else if (event->key.keysym.sym == SDLK_DOWN) editor->cam_y -= step; 
        else if (event->key.keysym.sym == SDLK_UP) editor->cam_y += step; 
        else if (event->key.keysym.sym == SDLK_o && editor->tool != Rename && editor->cam_w < 60) editor->cam_w += 10; 
        else if (event->key.keysym.sym == SDLK_i && editor->tool != Rename && editor->cam_w > 10) editor->cam_w -= 10;   
    }



    // draw and place inputs
    if ((editor->tool == Draw || editor->tool == Place) && event->type == SDL_MOUSEMOTION) {
        SDL_Rect viewport; SDL_RenderGetViewport(renderer, &viewport);
        if (event->motion.x > viewport.w * 3.125/UI_W) {
            editor->disp_x = (event->motion.x - viewport.x - viewport.w * 3.125/UI_W)/(viewport.w  * (UI_W - 3.125)/UI_W); 
            editor->disp_y = (float)(event->motion.y - viewport.y)/viewport.h; 
        }
        else {// if it is off of the display, then just set it to be -1, -1
            editor->disp_x = -1; 
            editor->disp_y = -1; 
        }
        
    }

    // specific draw inputs (since you can hold )
    else if (editor->tool == Draw && event->type == SDL_MOUSEBUTTONDOWN) {
        editor->brush_down = 1; 
    }
    
    else if (editor->tool == Draw && event->type == SDL_MOUSEBUTTONUP) {
        editor->brush_down = 0; 
    }

    else if (editor->tool == Place && event->type == SDL_MOUSEBUTTONDOWN) {
        float cam_h = editor->cam_w * UI_H/(UI_W - 3.25); 
        float map_x = editor->disp_x * editor->cam_w + (editor->cam_x - editor->cam_w/2.0);  
        float map_y = (1 - editor->disp_y) * cam_h + (editor->cam_y - cam_h/2.0); 


        SDL_Rect viewport; SDL_RenderGetViewport(renderer, &viewport);
        if (0 < map_x && map_x < MAP_W && 0 < map_y && map_y < MAP_H  && (event->button.x > viewport.w * 3.125/UI_W)) {
            editor->spawn_x = map_x; 
            editor->spawn_y = map_y; 
        } 
    }


    else if (editor->tool == Rename && event->type == SDL_TEXTINPUT) {
        unsigned len = strlen(editor->name); 
        // we only want to add the character if it is less than 31 characters and the size will not go over
        if (len < 31) {
            editor->name[len] = event->text.text[0]; 
            editor->name[len + 1] = '\0'; 

            // create the texture to test if it can be done 

            SDL_Texture *new_name; 
            init_text(&new_name, editor->name, font, (SDL_Color){0, 180, 180, 255}, renderer);

            // lets see if new name is wider than 5 menu tiles 
            int tw, th; 
            SDL_QueryTexture(new_name, NULL, NULL, &tw, &th); 
            float ui_width = 0.75 * tw/th; // displayed at 0.75 height 

            if (ui_width < 5) {
                SDL_DestroyTexture(editor->name_texture); 
                editor->name_texture = new_name; 
            }
            else { // does not fit, destroy test and return string to prior state
                SDL_DestroyTexture(new_name); 
                editor->name[len] = '\0'; 
            }
        }
    }

    else if (editor->tool == Rename && event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_BACKSPACE) { 
        unsigned len = strlen(editor->name); 
        if (len > 0) {
            editor->name[len - 1] = '\0';
            SDL_DestroyTexture(editor->name_texture); 
            init_text(&editor->name_texture, editor->name, font, (SDL_Color){0, 180, 180, 255}, renderer); 
        }
    }
}



