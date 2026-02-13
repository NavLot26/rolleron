// offical level select handling code 

#include <stdlib.h> 
#include <stdio.h> 
#include <SDL.h> 
#include <SDL_Image.h> 

#include "lib.c"

struct OfficialSelect {
    // custom vs official tabs 
    struct Button official; 
    struct Button custom; 

    // going to left and right level pages
    struct Button left; 
    struct Button right; 

    // previews themselves 
    struct LevelPreview previews[6]; 

    unsigned page; 
}; 

void init_official_select(struct OfficialSelect *select, SDL_Renderer *renderer, TTF_Font *font, unsigned num_completed) {
    // just intialize the permanent buttons that are not reset when the state is reentered 
    init_button(&select->official, 0, 2, 5, "Official", font, renderer); 
    select->official.enabled = 0; 
    init_button(&select->custom, 0, 9, 5, "Custom", font, renderer); 
    init_special_text_button(&select->left, 0, 0, 1, IMG_LoadTexture(renderer, "assets/left.png")); 
    init_special_text_button(&select->right, 0, UI_W - 1, 1, IMG_LoadTexture(renderer, "assets/right.png")); 
    select->page = num_completed / 6; 
}

void cleanup_official_select(struct OfficialSelect *select) {
    SDL_DestroyTexture(select->left.text); 
    SDL_DestroyTexture(select->right.text); 
    SDL_DestroyTexture(select->custom.text); 
    SDL_DestroyTexture(select->official.text); 
}

void enter_official_select(struct OfficialSelect *select, SDL_Renderer *renderer, TTF_Font *font, unsigned num_completed) {
    // create the level previews 
    for (int i = 0; i < 6; ++i) {
        unsigned level_num = select->page * 6 + i; 
        if (level_num < NUM_OFFICIALS) { // if the level exists then we can make it 
            char path[32]; 
            sprintf(path, "levels/official/%d.lvl", level_num); // level id is just the level number in official levels, in custom levels it is not since they can be deleted

            init_preview(&select->previews[i], font, renderer, path); 
        }
        else { // level does not exist, then set everything to null, and it will not be rendered
            select->previews[i].name = NULL; 
            select->previews[i].record = NULL; 
            select->previews[i].map = NULL; 
        }
    }

    // update if the left and right buttons are enabled
    select->left.enabled = select->page > 0; 
    select->right.enabled = select->page < NUM_OFFICIALS/6; 
}

void render_official_select(struct OfficialSelect *select, SDL_Renderer *renderer, SDL_Texture *tiles, int mouse_row, int mouse_col, unsigned num_completed) {
    // render the buttons and the level previews 
    render_background(renderer, tiles, mouse_row, mouse_col); 
    render_button(&select->official, renderer, tiles, mouse_row, mouse_col);
    render_button(&select->custom, renderer, tiles, mouse_row, mouse_col); 
    render_button(&select->left, renderer, tiles, mouse_row, mouse_col); 
    render_button(&select->right, renderer, tiles, mouse_row, mouse_col); 
    for (int i = 0; i < 6; ++i) {
        // like init, render assumes it exists
        if (select->page * 6 + i < NUM_OFFICIALS) { // only render existing levels, enable it if it is less than the number of completed levels, otherwise, it will not be enabled and the record of infinity will not show anyway since it was initialized to null 
            render_preview(&select->previews[i], i, renderer, tiles, mouse_row, mouse_col, select->page * 6 + i <= num_completed);
        }
    }
}

void handle_official_event(struct OfficialSelect *select, SDL_Event *event, SDL_Renderer *renderer, TTF_Font *font, int num_completed, enum AppState *next_state, enum LevelType *last_type, unsigned *last_id) {
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        unsigned mrow, mcol; 
        get_mouse_coords(event->button.x, event->button.y, renderer, &mrow, &mcol); 

        // left and right 
        if (select->left.enabled && is_mouse_over_button(&select->left, mrow, mcol)) {
            cleanup_previews(select->previews); 
            --select->page; 
            enter_official_select(select, renderer, font, num_completed); 
        }
        else if (select->right.enabled && is_mouse_over_button(&select->right, mrow, mcol)) {
            cleanup_previews(select->previews); 
            ++select->page; 
            enter_official_select(select, renderer, font, num_completed); 
        }
        

        // go to the custom tab 
        else if (is_mouse_over_button(&select->custom, mrow, mcol)) {
            *next_state = InCustom; 
        }
        
        // level select 
        int i = get_mouse_preview_i(mrow, mcol);  
        if (i != -1 && select->page * 6 + i <= num_completed && select->page * 6 + i < NUM_OFFICIALS) {
            puts("alert"); 
            *last_type = OfficialLevel; 
            *last_id = select->page * 6 + i; 
            *next_state = InGame; 
        }
    }
}
