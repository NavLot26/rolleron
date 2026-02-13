// custom level select handling code 

#include <stdio.h> 
#include <stdlib.h> 
#include <dirent.h> 
#include <SDL_image.h> 

#include "lib.c"

struct CustomSelect {
    // tabs 
    struct Button official; 
    struct Button custom; 

    // diff level page
    struct Button left; 
    struct Button right; 

    struct LevelPreview previews[6]; 

    unsigned page;

    // create a new one 
    struct Button new; 

    // buttons for editing and deleting the previews 
    struct Button edits[6]; 
    struct Button deletes[6]; 
    

    // since the level ids cannot be easily calculated like they can for the official levels, we keep a cache so we only have to calculate them when we need to 
    int ids_cache[6]; 
}; 

void get_level_ids(int page, int page_ids[6], int num_custom) {
    DIR *dir = opendir("levels/custom"); 

    int ids[num_custom]; // I usually dont use VLAs, but I will just use it here because it makes it way simpler 
    int curr = 0; 
    struct dirent* file; 

    while ((file = readdir(dir)) != NULL) {
        int id; 
        if (sscanf(file->d_name, "%d.lvl", &id) == 1) { // extract level name, but only actually add if it is one of the level files (it may be an invisible system file)
            ids[curr++] = id;
        } 
    }

    // sort the aray using insertion sort
    for (int i = 1; i < num_custom; ++i) {
        int val = ids[i]; 
        // go through each value before the value, and if it is greater than the value, move it forward (override the value, but we stored it)
        int j; 
        for (j = i - 1; j >= 0 && ids[j] > val; --j) {
            ids[j + 1] = ids[j]; 
        }
        // now there is a space for the value, so put it there
        ids[j + 1] = val; 
    }

    for (int i = 0; i < 6; ++i) {
        int level_num = 6 * page + i; 
        if (level_num < num_custom) page_ids[i] = ids[level_num]; 
        else page_ids[i] = -1; 
    }
}


void init_custom_select(struct CustomSelect *select, SDL_Renderer *renderer, TTF_Font *font, unsigned num_custom) {
    // create the permanent buttons that dont change as well as some other basic data 
    init_button(&select->official, 0, 2, 5, "Official", font, renderer); 
    init_button(&select->custom, 0, 9, 5, "Custom", font, renderer); 
    select->custom.enabled = 0; 

    
    init_button(&select->new, 0, 19, 3, "New", font, renderer); 
    
    
    // initialize the icons with nearest neighbor to make them smoother
    init_special_text_button(&select->left, 0, 0, 1, IMG_LoadTexture(renderer, "assets/left.png")); 
    init_special_text_button(&select->right, 0, UI_W - 1, 1, IMG_LoadTexture(renderer, "assets/right.png")); 
    SDL_Texture *edit = IMG_LoadTexture(renderer, "assets/edit.png"); 
    SDL_Texture *delete= IMG_LoadTexture(renderer, "assets/delete.png"); 
    for (int i = 0; i < 6; ++i) {
        init_special_text_button(&select->edits[i], 2 + 6 * (i / 3), 8 * (i % 3), 1, edit);
        init_special_text_button(&select->deletes[i], 3 + 6 * (i / 3), 8 * (i % 3), 1, delete);

    }
    select->page = num_custom / 6; 
}

void cleanup_custom_select(struct CustomSelect *select) {
    SDL_DestroyTexture(select->left.text); 
    SDL_DestroyTexture(select->right.text); 
    SDL_DestroyTexture(select->official.text); 
    SDL_DestroyTexture(select->custom.text); 
    SDL_DestroyTexture(select->new.text); 
    SDL_DestroyTexture(select->edits[0].text);
    SDL_DestroyTexture(select->deletes[0].text);
}

void enter_custom_select(struct CustomSelect *select, SDL_Renderer *renderer, TTF_Font *font, unsigned num_custom) {
    // get the new level ids for reload 
    get_level_ids(select->page, select->ids_cache, num_custom); 

    // initialize the previews based on the cached ids 
    for (int i = 0; i < 6; ++i) {
        unsigned level_num = select->page * 6 + i; 
        if (level_num < num_custom) { // if it exists, then initialize it (record can be infinity, this is handled by the init function )
            int level_id = select->ids_cache[i]; // get the level id 
            char path[32]; 
            sprintf(path, "levels/custom/%d.lvl", level_id);

            init_preview(&select->previews[i], font, renderer, path); 
        }
        else { // if it does not exist, set all assets to null 
            select->previews[i].name = NULL; 
            select->previews[i].record = NULL; 
            select->previews[i].map = NULL; 
        }
    }

    select->left.enabled = select->page > 0; 
    select->right.enabled = select->page < num_custom/6; 
}

void render_custom_select(struct CustomSelect *select, SDL_Renderer *renderer, SDL_Texture *tiles, int mouse_row, int mouse_col, unsigned num_custom) {
    // render buttons using abstracted functions 
    render_background(renderer, tiles, mouse_row, mouse_col); 
    render_button(&select->official, renderer, tiles, mouse_row, mouse_col);
    render_button(&select->custom, renderer, tiles, mouse_row, mouse_col); 
    render_button(&select->left, renderer, tiles, mouse_row, mouse_col); 
    render_button(&select->right, renderer, tiles, mouse_row, mouse_col); 
    render_button(&select->new, renderer, tiles, mouse_row, mouse_col); 
    for (int i = 0; i < 6; ++i) {
        if (select->page * 6 + i < num_custom) {
            render_button(&select->edits[i], renderer, tiles, mouse_row, mouse_col); 
            render_button(&select->deletes[i], renderer, tiles, mouse_row, mouse_col); 
            render_preview(&select->previews[i], i, renderer, tiles, mouse_row, mouse_col, 1); 
        }
    }
}

void handle_custom_event(struct CustomSelect *select, SDL_Event *event, SDL_Renderer *renderer, TTF_Font *font, unsigned *num_custom, enum AppState *next_state, enum LevelType *last_type, unsigned *last_id) {
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        unsigned mrow, mcol; 
        get_mouse_coords(event->button.x, event->button.y, renderer, &mrow, &mcol); 


        // left and right 
        if (select->left.enabled && is_mouse_over_button(&select->left, mrow, mcol)) {
            --select->page; 
            cleanup_previews(select->previews); 
            enter_custom_select(select, renderer, font, *num_custom); 
        }
        else if (select->right.enabled && is_mouse_over_button(&select->right, mrow, mcol)) {
            ++select->page; 
            cleanup_previews(select->previews); 
            enter_custom_select(select, renderer, font, *num_custom); 
        }

        // switch to to official 
        else if (is_mouse_over_button(&select->official, mrow, mcol)) {
            *next_state = InOfficial; 
        }

        // new level creation 
        else if (is_mouse_over_button(&select->new, mrow, mcol)) {
            // read 
            ++*num_custom; 
            // change num custom in file, change next_id in file, retrieve next_id from file
            FILE *progress = fopen("levels/progress.dat", "r+b");
            fseek(progress, sizeof(unsigned), SEEK_SET); 
            fwrite(num_custom, sizeof(unsigned), 1, progress);
            unsigned next_id; 
            fseek(progress, sizeof(unsigned) * 2, SEEK_SET); // even though the write cursor is now in the right pos, c standard requires this 
            fread(&next_id, sizeof(unsigned), 1, progress); 
            unsigned new_next_id = next_id + 1;  
            fseek(progress, sizeof(unsigned) * 2, SEEK_SET); // go back to it 
            fwrite(&new_next_id, sizeof(unsigned), 1, progress); 
            fclose(progress); 
            
            // make it unnamed and just some basic presents 
            char description[32] = "Unnamed"; 
            float spawn_x = 0.5, spawn_y = 0.5; 

            float spawn_rot = 0; 
            float record = INFINITY; 
            unsigned char map[MAP_H][MAP_W] = {0}; 


            char path[32];
            sprintf(path, "levels/custom/%d.lvl", next_id);
            FILE *file = fopen(path, "wb");

            // now write out all of that data 
            fwrite(description, sizeof(description), 1, file); 
            fwrite(&spawn_x, sizeof(float), 1, file); 
            fwrite(&spawn_y, sizeof(float), 1, file); 
            fwrite(&spawn_rot, sizeof(float), 1, file); 
            fwrite(&record, sizeof(float), 1, file); 
            fwrite(map, sizeof(map), 1, file); 
            
            fclose(file); 
            
            // swithch over to the editor 
            *last_type = CustomLevel; 
            *last_id = next_id; 
            *next_state = InEditor; 
        }

        int i = get_mouse_preview_i(mrow, mcol);  
        if (i != -1 && select->page * 6 + i < *num_custom) {
            *last_type = CustomLevel; 
            *last_id = select->ids_cache[i]; // use the ids cache to retreive the id of the level 
            *next_state = InGame; 
        }



        for (int i = 0; i < 6; ++i) {
            if (is_mouse_over_button(&select->edits[i], mrow, mcol) && select->page * 6 + i <= *num_custom) {
                *last_type = CustomLevel; 
                *last_id = select->ids_cache[i]; 
                *next_state = InEditor; 
            }

            else if (is_mouse_over_button(&select->deletes[i], mrow, mcol) && select->page * 6 + i <= *num_custom) {
                char path[32];
                sprintf(path, "levels/custom/%d.lvl", select->ids_cache[i]);
                remove(path); 

                --*num_custom; 
                FILE *progress = fopen("levels/progress.dat", "r+b");
                fseek(progress, sizeof(unsigned), SEEK_SET); 
                fwrite(num_custom, sizeof(unsigned), 1, progress);
                fclose(progress); 


                cleanup_previews(select->previews); 
                enter_custom_select(select, renderer, font, *num_custom); 
            }
        }
    }
}