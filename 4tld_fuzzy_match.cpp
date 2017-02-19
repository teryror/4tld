/******************************************************************************
File: 4tld_fuzzy_match.cpp
Author: Tristan Dannenberg
Notice: No warranty is offered or imlied; use this code at your own risk.
******************************************************************************/

#define TLDFM_DIR_FILTER_SIG(name) static bool name(char *filename, int32_t filename_len)

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

#ifdef TLDFM_IMPLEMENT_COMMANDS

#ifndef TLD_DEFAULT_OPEN_FILE_COMMAND
#define TLD_DEFAULT_OPEN_FILE_COMMAND tld_open_file_fuzzy
#endif

#ifndef TLDFM_DEFAULT_DIR_FILTER
#define TLDFM_DEFAULT_DIR_FILTER tld_ignore_hidden_files
TLDFM_DIR_FILTER_SIG(tld_ignore_hidden_files) {
    bool result = false;
    if (filename_len > 0) {
        result = (filename[0] == '.');
    }
    return result;
}
#endif

#ifndef TLDFM_FILE_LIST_CAPACITY
#define TLDFM_FILE_LIST_CAPACITY 1024
#endif

#ifndef TLDFM_DIR_QUEUE_CAPACITY
#define TLDFM_DIR_QUEUE_CAPACITY (TLDFM_FILE_LIST_CAPACITY / 16)
#endif

static tld_StringList
tld_fuzzy_construct_filename_list(Application_Links *app,
                                  String work_dir,
                                  Partition *memory,
                                  bool (*dir_filter)(char *, int32_t))
{
    tld_StringList result = {0};
    result.values = push_array(memory, String, TLDFM_FILE_LIST_CAPACITY);
    
    File_List dir_queue_infos[TLDFM_DIR_QUEUE_CAPACITY];
    String    dir_queue_names[TLDFM_DIR_QUEUE_CAPACITY];
    
    dir_queue_infos[0] = get_file_list(app, expand_str(work_dir));
    dir_queue_names[0] = make_lit_string("");
    int32_t dir_queue_index_read = 0;
    int32_t dir_queue_index_write = 1;
    
    while (dir_queue_index_read < dir_queue_index_write)
    {
        File_List dir_info = dir_queue_infos[dir_queue_index_read % TLDFM_DIR_QUEUE_CAPACITY];
        
        String dir_name = dir_queue_names[dir_queue_index_read % TLDFM_DIR_QUEUE_CAPACITY];
        dir_queue_index_read += 1;
        
        for (int i = 0; i < dir_info.count; ++i) {
            if (result.count >= TLDFM_FILE_LIST_CAPACITY) break;
            File_Info info_i = dir_info.infos[i];
            
            if (info_i.folder) {
                // No point in allocating at all if the queue is full
                if ((dir_queue_index_write - dir_queue_index_read) >= TLDFM_DIR_QUEUE_CAPACITY)
                    continue;
                
                if (dir_filter && dir_filter(info_i.filename, info_i.filename_len))
                    continue;
                
                int32_t filename_size = dir_name.size + info_i.filename_len + 1;
                char *filename_space = (char *)partition_allocate(memory, filename_size);
                if (filename_space)
                {
                    int32_t target_i = dir_queue_index_write % TLDFM_DIR_QUEUE_CAPACITY;
                    
                    dir_queue_names[target_i].str = filename_space;
                    dir_queue_names[target_i].memory_size = filename_size;
                    
                    copy_ss(&dir_queue_names[target_i], dir_name);
                    append_ss(&dir_queue_names[target_i],
                              make_string(info_i.filename, info_i.filename_len));
                    append_s_char(&dir_queue_names[target_i], '/');
                    
                    int32_t work_dir_old_size = work_dir.size;
                    append_ss(&work_dir, dir_queue_names[target_i]);
                    dir_queue_infos[target_i] = get_file_list(app, expand_str(work_dir));
                    work_dir.size = work_dir_old_size;
                    
                    dir_queue_index_write += 1;
                }
            } else {
                int32_t filename_size = dir_name.size + info_i.filename_len;
                char *filename_space = (char *)partition_allocate(memory, filename_size);
                if (filename_space) {
                    result.values[result.count].str = filename_space;
                    result.values[result.count].memory_size = filename_size;
                    copy_ss(&result.values[result.count], dir_name);
                    append_ss(&result.values[result.count],
                              make_string(info_i.filename, info_i.filename_len));
                    
                    result.count += 1;
                }
            }
        }
        
        free_file_list(app, dir_info);
    }
    
    return result;
}

CUSTOM_COMMAND_SIG(tld_open_file_fuzzy) {
    int32_t mem_size = 1024 * 1024;
    Partition mem = make_part(malloc(mem_size), mem_size);
    
    View_Summary view = get_active_view(app, AccessAll);
    
    char home_directory_space[1024];
    String home_directory = make_fixed_width_string(home_directory_space);
    tld_get_home_directory(app, &home_directory);
    tld_StringList search_space = tld_fuzzy_construct_filename_list(app, home_directory, &mem, TLDFM_DEFAULT_DIR_FILTER);
    
    Query_Bar search_bar;
    char search_bar_space[TLDFM_MAX_QUERY_SIZE];
    search_bar.prompt = make_lit_string("Find File: ");
    search_bar.string = make_fixed_width_string(search_bar_space);
    start_query_bar(app, &search_bar, 0);
    
    String selected_file = tld_query_list_fuzzy(app, &search_bar, search_space);
    if (selected_file.str) {
        char full_path_space[1024];
        String full_path = make_fixed_width_string(full_path_space);
        copy_ss(&full_path, home_directory);
        append_ss(&full_path, selected_file);
        
        view_open_file(app, &view, expand_str(full_path), false);
    }
    
    free(mem.base);
}

#endif