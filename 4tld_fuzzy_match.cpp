/******************************************************************************
File: 4tld_fuzzy_match.cpp
Author: Tristan Dannenberg
Notice: No warranty is offered or imlied; use this code at your own risk.
******************************************************************************/

#ifndef TLD_HOME_DIRECTORY
#define TLD_HOME_DIRECTORY
static void
tld_get_home_directory(Application_Links *app, String *dest) {
    dest->size = directory_get_hot(app, dest->str, dest->memory_size);
}
#endif

#ifndef TLDFM_MAX_QUERY_SIZE
// This will be important once we have a fuzzy_match function that actually needs working memory.
#define TLDFM_MAX_QUERY_SIZE 64
#endif

// TODO: This isn't an actual heuristic to sort by, just a check whether val
// contains all characters in key, in order.
// 
// We're looking for a modified Wagner-Fischer algorithm (hopefully optimized
// to allocate constant memory!), with extra goodies, such as bonuses for
// proximity to beginning of val, consecutive matches, and matches following
// separators (CamelCase, snake_case, lisp-case, object.notation, file/paths).
static int32_t
tld_fuzzy_match_ss(String key, String val) {
    if (val.size < key.size) return 0;
    
    int i = 0, j = 0;
    while (i < key.size && j < val.size) {
        if ((char_is_upper(key.str[i]) && key.str[i] == val.str[j]) ||
            (key.str[i] == char_to_lower(val.str[j])))
        {
            ++i;
        }
        
        ++j;
    }
    
    if (i == key.size) return 1;
    return 0;
}

// TODO: This should find files in subdirectories as well!
CUSTOM_COMMAND_SIG(tld_open_file_fuzzy) {
    char home_directory_space[1024];
    String home_directory = make_fixed_width_string(home_directory_space);
    tld_get_home_directory(app, &home_directory);
    File_List files = get_file_list(app, expand_str(home_directory));
    
    char search_bar_space[TLDFM_MAX_QUERY_SIZE];
    
    Query_Bar search_bar;
    search_bar.prompt = make_lit_string("Find File: ");
    search_bar.string = make_fixed_width_string(search_bar_space);
    start_query_bar(app, &search_bar, 0);
    
    String empty = make_lit_string("");
    Query_Bar result_bars[7] = {0};
    for (int i = 0; i < ArrayCount(result_bars); ++i) {
        result_bars[i].string = empty;
        result_bars[i].prompt = empty;
    }
    
    int result_count = 0;
    int result_selected_index = -1;
    
    while (true) {
        User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
        bool search_key_changed = false;
        
        if (in.abort) break;
        
        if (in.type == UserInputKey) {
            if (result_selected_index >= 0) {
                result_bars[result_selected_index].string =
                    result_bars[result_selected_index].prompt;
                result_bars[result_selected_index].prompt = empty;
            }
            
            if (in.key.keycode == '\n') {
                // TODO: Open file
                break;
            } else if (in.key.keycode == key_back) {
                if (search_bar.string.size > 0) {
                    --search_bar.string.size;
                    search_key_changed = true;
                }
            } else if (in.key.keycode == key_del) {
                search_bar.string.size = 0;
                result_selected_index = -1;
                
                for (int i = result_count - 1; i >= 0; --i) {
                    end_query_bar(app, &result_bars[i], 0);
                }
            } else if (in.key.keycode == key_up) {
                --result_selected_index;
                if (result_selected_index < -1) {
                    result_selected_index = result_count - 1;
                }
            } else if (in.key.keycode == key_down) {
                ++result_selected_index;
                if (result_selected_index >= result_count) {
                    result_selected_index = -1;
                }
            } else if (key_is_unmodified(&in.key) && in.key.character != 0) {
                append_s_char(&search_bar.string, in.key.character);
                search_key_changed = true;
            }
            
            if (result_selected_index >= 0) {
                result_bars[result_selected_index].prompt =
                    result_bars[result_selected_index].string;
                result_bars[result_selected_index].string = empty;
            }
        }
        
        if (search_key_changed) {
            end_query_bar(app, &search_bar, 0);
            result_selected_index = -1;
            
            for (int i = result_count - 1; i >= 0; --i) {
                end_query_bar(app, &result_bars[i], 0);
            }
            result_count = 0;
            
            int32_t min_score = 0x7FFFFFFF;
            int32_t min_index = -1;
            
            int32_t result_scores[ArrayCount(result_bars)];
            
            for (int i = 0; i < files.count; ++i) {
                if (files.infos[i].folder) continue;
                String filename = make_string(files.infos[i].filename, files.infos[i].filename_len);
                
                int32_t score = tld_fuzzy_match_ss(search_bar.string, filename);
                
                if (score > 0 && result_count < ArrayCount(result_bars)) {
                    result_bars[result_count].string = filename;
                    result_scores[result_count] = score;
                    
                    if (score < min_score) {
                        min_score = score;
                        min_index = result_count;
                    }
                    
                    ++result_count;
                } else if (score > min_score) {
                    result_bars[min_index].string = filename;
                    result_scores[min_index] = score;
                    
                    min_index = 0;
                    min_score = result_scores[0];
                    
                    for (int j = 1; j <= ArrayCount(result_bars); ++j) {
                        if (result_scores[j] < min_score) {
                            min_score = result_scores[j];
                            min_index = j;
                        }
                    }
                }
            }
            
            for (int i = 0; i < result_count - 1; ++i) {
                for (int j = i + 1; j < result_count; ++j) {
                    if (result_scores[i] < result_scores[j]) {
                        String s = result_bars[i].string;
                        result_bars[i].string = result_bars[j].string;
                        result_bars[j].string = s;
                        
                        int32_t c = result_scores[i];
                        result_scores[i] = result_scores[j];
                        result_scores[j] = c;
                    }
                }
            }
            
            for (int i = result_count - 1; i >= 0; --i) {
                start_query_bar(app, &result_bars[i], 0);
            }
            start_query_bar(app, &search_bar, 0);
        }
    }
}