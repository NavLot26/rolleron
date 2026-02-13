/*
Library data structures and functions that are used across the entire codebase, mostly UI functionality. 
All components include this, there are many constants and enum definitions as well. 
*/

#include <stdlib.h> 
#include <stdio.h> 
#include <SDL.h> 
#include <SDL_ttf.h> 


#ifndef LIB_C
#define LIB_C 

// constants for the size of the UI grid and the map 
#define UI_W 24
#define UI_H 13.5 

#define MAP_W 48
#define MAP_H 32

enum AppState {InOfficial, InCustom, InGame, InEditor, InOverlay}; 

#define NUM_OFFICIALS 8

enum MenuTile {
    Blank, 
    Enabled, 
    Disabled = 3, 
}; 

enum LevelType {OfficialLevel, CustomLevel}; 

enum Tile {
    None, Solid, Win, 
    Gravity, AntiGravity, 
    Drag, Boost, 
    ThrustersOn, ThrustersOff, 
    StrongerThrusters, WeakerThrusters, 
    DownForce, UpForce, LeftForce, RightForce, 
    CounterClockwiseTorque, ClockwiseTorque
}; 


// TEXT AND TILES

void init_text(SDL_Texture **texture, char *string, TTF_Font *font, SDL_Color color, SDL_Renderer *renderer) {
    SDL_Surface *temp_surface = TTF_RenderText_Blended(font, string, color); 
    *texture = SDL_CreateTextureFromSurface(renderer, temp_surface); 
    SDL_FreeSurface(temp_surface); 
    SDL_SetTextureScaleMode(*texture, SDL_ScaleModeLinear);
}

enum TextAnchor {Left, Middle, Right}; 
void render_menu_text(SDL_Renderer *renderer, SDL_Texture *texture, unsigned row, float col, float height, enum TextAnchor anchor) {
    // render by in the corrrect aspect ratio 
    SDL_Rect viewport; 
    SDL_RenderGetViewport(renderer, &viewport); 

    int tw, th; 
    SDL_QueryTexture(texture, NULL, NULL, &tw, &th); 

    int display_h = height/UI_H * viewport.h; 
    int display_w = display_h * ((float)tw / th);

    int edge_spacing = 0.125/UI_W * viewport.w; 

    int x_offset = (anchor == Right)? display_w + edge_spacing: (anchor == Middle)? display_w/2 : -edge_spacing; 
    SDL_RenderCopy(renderer, texture, NULL, &(SDL_Rect){(float)col/UI_W * viewport.w - x_offset, ((row + 0.5)/UI_H) * viewport.h - display_h/2.0, display_w, display_h}); 
}

void render_menu_tile(SDL_Renderer *renderer, SDL_Texture *tiles, enum MenuTile tile, unsigned row, unsigned col) {
    SDL_Rect viewport; 
    SDL_RenderGetViewport(renderer, &viewport); 
    SDL_RenderCopy(renderer, tiles, &(SDL_Rect){tile * 16, 0, 16, 16}, &(SDL_Rect){(float)col/UI_W * viewport.w, (float)row/UI_H * viewport.h, 1.0/UI_W * viewport.w + 1, 1.0/UI_H * viewport.h + 1}); 
}

void render_menu_outline(SDL_Renderer *renderer, unsigned row, unsigned col, unsigned width, unsigned height) {
    // outline around a menu tile 
    SDL_Rect viewport; 
    SDL_RenderGetViewport(renderer, &viewport); 

    int thickness = 0.05/UI_W * viewport.w; 
    int p_width = (float)width/UI_W * viewport.w; 
    int p_height = (float)height/UI_H * viewport.h; 

    int p_x = (float)col/UI_W * viewport.w; 
    int p_y = (float)row/UI_H * viewport.h; 


    // left
    SDL_RenderFillRect(renderer, &(SDL_Rect){p_x, p_y, thickness, p_height}); 
    // right
    SDL_RenderFillRect(renderer, &(SDL_Rect){p_x + p_width - thickness, p_y, thickness, p_height}); 
    // top 
    SDL_RenderFillRect(renderer, &(SDL_Rect){p_x, p_y, p_width, thickness}); 
    // bottom 
    SDL_RenderFillRect(renderer, &(SDL_Rect){p_x, p_y + p_height - thickness, p_width, thickness}); 
}

void render_background(SDL_Renderer *renderer, SDL_Texture *tiles, int mouse_row, int mouse_col) {
    // render all the blank tiles in the background
    for (int row = 0; row < UI_H; ++row) {
        for (int col = 0; col < UI_W; ++col) {
            render_menu_tile(renderer, tiles, Blank, row, col); 
        }
    }
    
    SDL_SetRenderDrawColor(renderer, 180,  0, 0, 255); 
    render_menu_outline(renderer, mouse_row, mouse_col, 1, 1); 
}

void get_mouse_coords(int mx, int my, SDL_Renderer *renderer, unsigned *row, unsigned *col) {
    SDL_Rect viewport; SDL_RenderGetViewport(renderer, &viewport);
    *row = (float)(my - viewport.y)/viewport.h * UI_H; 
    *col = (float)(mx - viewport.x)/viewport.w * UI_W; 
}








// BUTTONS 
struct Button {
    unsigned row, col; 
    unsigned width; 
    SDL_Texture *text; 
    int enabled; 
}; 

int is_mouse_over_button(struct Button *button, int mouse_row, int mouse_col) {
    return (mouse_row == button->row && button->col <= mouse_col && mouse_col < button->col + button->width); 
}

void init_button(struct Button *button, unsigned row, unsigned col, unsigned width, char *text_str, TTF_Font *font, SDL_Renderer *renderer) {
    button->row = row; 
    button->col = col; 
    button->width = width; 
    // use the text functionality 
    init_text(&button->text, text_str, font, (SDL_Color){0, 180, 180, 255}, renderer); 
    button->enabled = 1; 
}
// special button who's text is not basic ascii from the font, so the text texture is passed in directly 
void init_special_text_button(struct Button *button, unsigned row, unsigned col, unsigned width, SDL_Texture *text) {
    SDL_SetTextureScaleMode(text, SDL_ScaleModeLinear);

    button->row = row; 
    button->col = col; 
    button->width = width; 
    button->text = text; 
    button->enabled = 1; 
}

void render_button(struct Button *button, SDL_Renderer *renderer, SDL_Texture *tiles, int mouse_row, int mouse_col) {
    // render the background tiles, then the text, and then the possible outline 

    // background tiles
    enum MenuTile bg = button->enabled? Enabled : Disabled; 
    for (unsigned col = button->col; col < button->col + button->width; ++col) {
        render_menu_tile(renderer, tiles, bg, button->row, col); 
    }

    // text
    render_menu_text(renderer, button->text, button->row, button->col + button->width/2.0, 0.9, Middle); 

    // outline
    if (is_mouse_over_button(button, mouse_row, mouse_col)) {
        button->enabled? SDL_SetRenderDrawColor(renderer, 0,  180, 180, 255): SDL_SetRenderDrawColor(renderer, 180,  0, 0, 255); 
        render_menu_outline(renderer, button->row, button->col, button->width, 1); 
    }
}









// LEVEL PREVIEWS (More specific button used by the level select menus)
struct LevelPreview {
    SDL_Texture *name; 
    SDL_Texture *record; 
    SDL_Texture *map; 
}; 


int get_mouse_preview_i(int mrow, int mcol) {
    // get the index of the preview given the position on the menu screen
    if (1 <= mcol % 8 && mcol % 8 <= 6 && 1 <= (mrow - 1) % 6)  {
        int grid_row = mrow / 7, grid_col = mcol/8; 
        int i = grid_row * 3 + grid_col; 
        return i; 
    }
    else return -1; 

}

void init_preview(struct LevelPreview *preview, TTF_Font *font, SDL_Renderer *renderer, char *path) {
    // initialize the preview, including its text, record, and map preview 
    FILE *file = fopen(path, "rb"); 
    
    char name[32]; 
    fread(&name, sizeof(name), 1, file); 

    float spawn_x, spawn_y; 
    fread(&spawn_x, sizeof(float), 1, file);
    fread(&spawn_y, sizeof(float), 1, file); 

    fseek(file, sizeof(float), SEEK_CUR); // skip rotation 
    
    float record; 
    fread(&record, sizeof(float), 1, file); 

    unsigned char map[MAP_H][MAP_W]; 
    fread(&map, sizeof(map), 1, file); 
    fclose(file); 

    // name 
    init_text(&preview->name, name, font, (SDL_Color){0, 180, 180, 255}, renderer); 

    // record (if infinity, just set it to be null)
    if (record != INFINITY) {
        char record_str[8]; sprintf(record_str, "%.1f", record); 
        init_text(&preview->record, record_str, font, (SDL_Color){0, 180, 180, 255}, renderer);
    }
    else preview->record = NULL; 

    // MAP 
    // create the map preview by creating a texture manually, and then we will render it with the preview  
    preview->map = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_STREAMING,MAP_W, MAP_H);

    SDL_PixelFormat *fmt = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);
    Uint32 purple = SDL_MapRGBA(fmt, 46, 16, 107, 255); 
    Uint32 blue = SDL_MapRGBA(fmt, 0, 180, 180, 255); 

    // code to traverse the texture image using pointer logic 
    void *start; 
    int byte_width; 
    SDL_LockTexture(preview->map, NULL, &start, &byte_width); 

    for (int row = 0; row < MAP_H; ++row) {
        Uint32 *row_ptr = (Uint32*)((Uint8*)start + (MAP_H - 1 - row) * byte_width);      
        for (int col = 0; col < MAP_W; ++col) {
            row_ptr[col] = (map[row][col] == Solid || map[row][col] == Gravity || map[row][col] == AntiGravity) ? purple: (map[row][col] == Win)? blue: 0x00000000;
        }
    }

    Uint32 *spawn_row_ptr = (Uint32*)((Uint8*)start + (MAP_H - 1 - (int)spawn_y) * byte_width); 
    spawn_row_ptr[(int)spawn_x] = blue; 

    SDL_FreeFormat(fmt);
    SDL_UnlockTexture(preview->map); 
}

void cleanup_previews(struct LevelPreview previews[]) {
    for (int i = 0; i < 6; ++i) {
        SDL_DestroyTexture(previews[i].name); 
        SDL_DestroyTexture(previews[i].record); 
        SDL_DestroyTexture(previews[i].map); 
    }
}

// renders the preview, based on if it is enabled (not stored with the preview since it is grid based)
void render_preview(struct LevelPreview *preview, int grid_i, SDL_Renderer *renderer, SDL_Texture *tiles, int mouse_row, int mouse_col, int enabled) {
    int start_col = 1 + 8 * (grid_i % 3), start_row = 2 + 6 * (grid_i / 3); 

    // background 
    enum MenuTile bg = enabled? Enabled : Disabled; 
    for (int row = start_row; row < start_row + 5; ++row) {
        for (int col = start_col; col < start_col + 6; ++col) {
            render_menu_tile(renderer, tiles, bg, row, col); 
        }
    }

    // name and record 
    render_menu_text(renderer, preview->name, start_row, start_col, 0.75, Left); 
    render_menu_text(renderer, preview->record, start_row, start_col + 6, 0.5, Right);
    
    
    SDL_Rect viewport; SDL_RenderGetViewport(renderer, &viewport);
    SDL_RenderCopy(renderer, preview->map, NULL, &(SDL_Rect){(start_col + 0.5)/UI_W * viewport.w, (start_row + 1 + 0.5 * 2.0/3)/UI_H * viewport.h, 5.0/UI_W * viewport.w, (4 - 2.0/3)/UI_H * viewport.h}); 

    // outline if needed 
    if (mouse_row >= start_row && mouse_row < start_row + 5 && mouse_col >= start_col && mouse_col < start_col + 6) {
        enabled? SDL_SetRenderDrawColor(renderer, 0,  180, 180, 255): SDL_SetRenderDrawColor(renderer, 180,  0, 0, 255); 
        render_menu_outline(renderer, start_row, start_col, 6, 5); 
    }
}

#endif