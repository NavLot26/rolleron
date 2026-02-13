#include <stdio.h> 
#include <stdlib.h> 
#include <dirent.h> 
#include <SDL_image.h> 
#include <SDL_mixer.h> 

#include "lib.c"

struct GameOverlay {
    enum OverlayType {WinPage, LosePage, PausePage} type; 
    int pause_restart_game; 


    // page elements, going down in order. 
    SDL_Texture *completed; 
    SDL_Texture *destroyed; 
    SDL_Texture *paused; 


    SDL_Texture *name; // temp 


    SDL_Texture *time_label; 
    SDL_Texture *record_label; 


    SDL_Texture *time; // temp 
    SDL_Texture *record; // temp 


    struct Button exit; 
    struct Button restart; 
    struct Button next; 
    struct Button resume; 

}; 

void init_overlay(struct GameOverlay *overlay, SDL_Renderer *renderer, TTF_Font *font) {
    init_text(&overlay->completed, "Completed!", font, (SDL_Color){0, 180, 180, 255}, renderer);
    init_text(&overlay->destroyed, "Destroyed!", font, (SDL_Color){0, 180, 180, 255}, renderer);
    init_text(&overlay->paused, "Paused!", font, (SDL_Color){0, 180, 180, 255}, renderer);


    init_text(&overlay->time_label, "Time:", font, (SDL_Color){0, 180, 180, 255}, renderer);
    init_text(&overlay->record_label, "Record:", font, (SDL_Color){0, 180, 180, 255}, renderer);

    init_button(&overlay->exit, 12, 1, 6, "Exit", font, renderer); 
    init_button(&overlay->restart, 12, 9, 6, "Restart", font, renderer); 
    init_button(&overlay->next, 12, 17, 6, "Next", font, renderer); 
    init_button(&overlay->resume, 12, 17, 6, "Resume", font, renderer); 
}

void cleanup_overlay(struct GameOverlay *overlay) { 
    SDL_DestroyTexture(overlay->resume.text); 
    SDL_DestroyTexture(overlay->next.text); 
    SDL_DestroyTexture(overlay->restart.text); 
    SDL_DestroyTexture(overlay->exit.text); 

    SDL_DestroyTexture(overlay->record_label); 
    SDL_DestroyTexture(overlay->time_label); 

    SDL_DestroyTexture(overlay->paused); 
    SDL_DestroyTexture(overlay->destroyed); 
    SDL_DestroyTexture(overlay->completed); 
}

void enter_overlay_state(struct GameOverlay *overlay, enum OverlayType type, float time, float record, char *name, SDL_Renderer *renderer, TTF_Font *font, enum LevelType last_type, unsigned last_id) {
    overlay->type = type; 

    char time_str[8]; sprintf(time_str, "%.1f", time); 
    SDL_Color time_color = (time < record)? (SDL_Color){0, 180, 0, 255} : (SDL_Color){180, 0, 0, 255}; 
    init_text(&overlay->time, time_str, font, time_color, renderer); 


    if (record != INFINITY) {
        char record_str[8]; sprintf(record_str, "%.1f", record); 
        init_text(&overlay->record, record_str, font, (SDL_Color){0, 180, 180, 255}, renderer); 
    }
    else overlay->record = NULL; 
   
    init_text(&overlay->name, name, font, (SDL_Color){0, 180, 180, 255}, renderer); 

    if (overlay->type == WinPage && (last_type == CustomLevel || (last_type == OfficialLevel && last_id == NUM_OFFICIALS - 1))) {
        overlay->next.enabled = 0; 
    }
    else overlay->next.enabled = 1; 


    // we have to do this here instead of in the game because the game will just turn the sound back on after turning it off since it gets one more update sound call after handle events triggers the pause page
    if (overlay->type == PausePage) {
        for (int i = 0; i < 8; ++i) Mix_HaltChannel(i);
    }
}

void exit_overlay_state(struct GameOverlay *overlay) {
    SDL_DestroyTexture(overlay->name); 
    SDL_DestroyTexture(overlay->record); 
    SDL_DestroyTexture(overlay->time); 
    
}

void render_overlay(struct GameOverlay *overlay, SDL_Renderer *renderer, SDL_Texture *tiles, int mrow, int mcol) {
    SDL_Texture *header = (overlay->type == WinPage)? overlay->completed: (overlay->type == LosePage)? overlay->destroyed: overlay->paused; 
    render_menu_text(renderer, header, 1, UI_W/2.0, 1.5, Middle);

    render_menu_text(renderer, overlay->name, 3, UI_W/2.0, 1.0, Middle);

    render_menu_text(renderer, overlay->time_label, 5, UI_W * 0.25, 0.5, Middle); 
    render_menu_text(renderer, overlay->time, 6, UI_W * 0.25, 1.5, Middle); 

    if (overlay->record != NULL) {
        render_menu_text(renderer, overlay->record_label, 5, UI_W * 0.75, 0.5, Middle); 
        render_menu_text(renderer, overlay->record, 6, UI_W * 0.75, 1.5, Middle); 
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // give it a constant white outline to make it pop out of the bg 
    // when the blue outline happens, the render color is set to blue for the second, so it looks fine 

    render_button(&overlay->exit, renderer, tiles, mrow, mcol);
    render_menu_outline(renderer, overlay->exit.row, overlay->exit.col, overlay->exit.width, 1); 

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); 
    render_button(&overlay->restart, renderer, tiles, mrow, mcol);  
    render_menu_outline(renderer, overlay->restart.row, overlay->restart.col, overlay->restart.width, 1); 


    if (overlay->type == WinPage) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); 
        render_button(&overlay->next, renderer, tiles, mrow, mcol); 
        render_menu_outline(renderer, overlay->next.row, overlay->next.col, overlay->next.width, 1); 

    }
    else if (overlay->type == PausePage) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); 
        render_button(&overlay->resume, renderer, tiles, mrow, mcol); 
        render_menu_outline(renderer, overlay->resume.row, overlay->resume.col, overlay->resume.width, 1); 

    }
}

void handle_overlay_event(struct GameOverlay *overlay, SDL_Event *event, SDL_Renderer *renderer, enum LevelType last_type, enum AppState *next_state, unsigned *last_id, int came_from_editor) {
    // never exit to editor for now 
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        unsigned mrow, mcol; 
        get_mouse_coords(event->button.x, event->button.y, renderer, &mrow, &mcol); 

        if (is_mouse_over_button(&overlay->exit, mrow, mcol)) {
            if (last_type == OfficialLevel) *next_state = InOfficial; 
            else if (last_type == CustomLevel && !came_from_editor) *next_state = InCustom; 
            else if (last_type == CustomLevel && came_from_editor) *next_state = InEditor; 

            if (overlay->type == PausePage) overlay->pause_restart_game = 1; // exit the game when leaving overlay into the editor state 
        }

        else if (is_mouse_over_button(&overlay->restart, mrow, mcol)) {
            *next_state = InGame; 

            // exit and restart the game when exitinig with the restart button from pause page
            if (overlay->type == PausePage) overlay->pause_restart_game = 1; 
        }

        else if (overlay->type == WinPage && overlay->next.enabled && is_mouse_over_button(&overlay->next, mrow, mcol)) {
            ++*last_id; // increment the id for the next game 
            *next_state = InGame; 
        }

        else if (overlay->type == PausePage && is_mouse_over_button(&overlay->resume, mrow, mcol)) {
            *next_state = InGame; 
            
            // do not restart the game if exiting from unpause 
            overlay->pause_restart_game = 0; // set a flag to not restart the game and just let it be preserved 

        }
    }
}