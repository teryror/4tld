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

struct tld_StringList {
    String *values;
    int32_t count;
};

static String
tld_query_list_fuzzy(Application_Links *app, Query_Bar *search_bar, tld_StringList list) {
    String results[7];
    
    String empty = make_lit_string("");
    Query_Bar result_bars[ArrayCount(results)] = {0};
    for (int i = 0; i < ArrayCount(results); ++i) {
        result_bars[i].string = empty;
        result_bars[i].prompt = empty;
    }
    
    int result_count = 0;
    int result_selected_index = 0;
    
    while (true) {
        User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
        bool search_key_changed = false;
        bool selected_index_changed = false;
        
        if (in.abort) return {0};
        
        if (in.type == UserInputKey) {
            if (in.key.keycode == '\n') {
                if (result_count > 0) {
                    if (result_selected_index < 0)
                        result_selected_index = 0;
                    return result_bars[result_selected_index].prompt;
                }
            } else if (in.key.keycode == key_back) {
                if (search_bar->string.size > 0) {
                    --search_bar->string.size;
                    search_key_changed = true;
                }
            } else if (in.key.keycode == key_del) {
                search_bar->string.size = 0;
                result_selected_index = 0;
                
                for (int i = result_count - 1; i >= 0; --i) {
                    end_query_bar(app, &result_bars[i], 0);
                }
            } else if (in.key.keycode == key_up) {
                selected_index_changed = true;
                
                --result_selected_index;
                if (result_selected_index < 0) {
                    result_selected_index = result_count - 1;
                }
            } else if (in.key.keycode == key_down) {
                selected_index_changed = true;
                
                ++result_selected_index;
                if (result_selected_index >= result_count) {
                    result_selected_index = 0;
                }
            } else if (key_is_unmodified(&in.key) && in.key.character != 0) {
                append_s_char(&search_bar->string, in.key.character);
                search_key_changed = true;
            }
        }
        
        if (search_key_changed) {
            end_query_bar(app, search_bar, 0);
            result_selected_index = 0;
            
            for (int i = result_count - 1; i >= 0; --i) {
                end_query_bar(app, &result_bars[i], 0);
            }
            result_count = 0;
            
            int32_t min_score = 0x7FFFFFFF;
            int32_t min_index = -1;
            
            int32_t result_scores[ArrayCount(results)];
            
            for (int i = 0; i < list.count; ++i) {
                String candidate = list.values[i];
                
                int32_t score = tld_fuzzy_match_ss(search_bar->string, candidate);
                
                if (score > 0 && result_count < ArrayCount(results)) {
                    results[result_count] = candidate;
                    result_scores[result_count] = score;
                    
                    if (score < min_score) {
                        min_score = score;
                        min_index = result_count;
                    }
                    
                    ++result_count;
                } else if (score > min_score) {
                    results[min_index] = candidate;
                    result_scores[min_index] = score;
                    
                    min_index = 0;
                    min_score = result_scores[0];
                    
                    for (int j = 1; j <= ArrayCount(results); ++j) {
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
                result_bars[i].string = results[i];
                start_query_bar(app, &result_bars[i], 0);
            }
            start_query_bar(app, search_bar, 0);
        }
        
        if (selected_index_changed || search_key_changed) {
            for (int i = 0; i < result_count; ++i) {
                if (i == result_selected_index) {
                    result_bars[i].prompt = results[i];
                    result_bars[i].string = empty;
                } else {
                    result_bars[i].prompt = empty;
                    result_bars[i].string = results[i];
                }
            }
        }
    }
}

// TODO: This is terribly inefficient, and pretty unsafe to boot, but it is
// just a temporary solution until we bother implementing recursive file listing.
static tld_StringList
tld_fuzzy_construct_filename_list(File_List *files) {
    tld_StringList result = {0};
    result.values = (String *) malloc(sizeof(String) * files->count);
    
    for (int i = 0; i < files->count; ++i) {
        File_Info info = files->infos[i];
        if (info.folder) continue;
        
        result.values[result.count] = make_string(info.filename, info.filename_len);
        result.count += 1;
    }
    
    return result;
}

CUSTOM_COMMAND_SIG(tld_open_file_fuzzy) {
    View_Summary view = get_active_view(app, AccessAll);
    
    char home_directory_space[1024];
    String home_directory = make_fixed_width_string(home_directory_space);
    tld_get_home_directory(app, &home_directory);
    File_List files = get_file_list(app, expand_str(home_directory));
    tld_StringList search_space = tld_fuzzy_construct_filename_list(&files);
    
    Query_Bar search_bar;
    char search_bar_space[TLDFM_MAX_QUERY_SIZE];
    search_bar.prompt = make_lit_string("Find File: ");
    search_bar.string = make_fixed_width_string(search_bar_space);
    start_query_bar(app, &search_bar, 0);
    
    String selected_file = tld_query_list_fuzzy(app, &search_bar, search_space);
    
    char full_path_space[1024];
    String full_path = make_fixed_width_string(full_path_space);
    copy_ss(&full_path, home_directory);
    append_ss(&full_path, selected_file);
    
    view_open_file(app, &view, expand_str(full_path), false);
    
    free(search_space.values);
}
