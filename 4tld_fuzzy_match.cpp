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

#ifdef TLDFM_DUMB_HEURISTIC
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
#else

static inline bool
tld_char_is_separator(char a) {
    return (a == ' ' || a == '_' || a == '-' ||
            a == '.' || a == '/' || a == '\\');
}

static inline bool
tld_fuzzy_match_char(char a, char b) {
    return ((char_to_lower(a) == char_to_lower(b)) ||
            (char_is_upper(a) && a == b) ||
            (a == ' ' && tld_char_is_separator(b))
            );
}

#ifndef max
#define max(a, b) ((a > b) ? a : b)
#endif

static int32_t
tld_fuzzy_match_ss(String key, String val) {
    if (key.size == 0) {
        return 1;
    }
    
    // Optimization 1: Skip table rows
    int j = 0;
    while (j < val.size && !tld_fuzzy_match_char(key.str[0], val.str[j])) {
        ++j;
    }
    
    if (val.size - j < key.size) {
        return 0;
    }
    
    int32_t row[TLDFM_MAX_QUERY_SIZE] = {0}; // Current row of scores table
    int32_t lml[TLDFM_MAX_QUERY_SIZE] = {0}; // Current row of auxiliary table (match lengths)
    
    for (; j < val.size; ++j) {
        int32_t diag = 1;
        int32_t diag_l = 0;
        
        for (int i = 0; i < key.size; ++i) {
            int32_t row_old = row[i];
            int32_t lml_old = lml[i];
            
            lml[i] = tld_fuzzy_match_char(key.str[i], val.str[j]) * (diag_l + 1);
            
            if (diag > 0 && tld_fuzzy_match_char(key.str[i], val.str[j])) {
                // Sequential match bonus:
                int32_t value = lml[i];
                
                if (j == 0 || (char_is_lower(val.str[j - 1]) && char_is_upper(val.str[j])) ||
                    tld_char_is_separator(val.str[j - 1]))
                {   // Abbreviation bonus:
                    value += 2;
                }
                
                row[i] = max(diag + value, row[i]);
            }
            
            
            diag = row_old;
            diag_l = lml_old;
        }
    }
    
    return row[key.size - 1];
}

#endif

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
    bool search_key_changed = true;
    bool selected_index_changed = true;
    
    while (true) {
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
                    
                    if (score > min_score)
                    {
                        min_score = score;
                        min_index = result_count;
                    }
                    
                    ++result_count;
                } else if (score > min_score)
                {
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
                    if (result_scores[i] < result_scores[j])
                    {
                        String s = results[i];
                        results[i] = results[j];
                        results[j] = s;
                        
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
        
        User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
        selected_index_changed = false;
        search_key_changed = false;
        
        if (in.abort) return {0};
        
        if (in.type == UserInputKey) {
            if (in.key.keycode == '\n') {
                if (result_count > 0) {
                    if (result_selected_index < 0)
                        result_selected_index = 0;
                    return results[result_selected_index];
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

static void
__tld_open_file_fuzzy_impl(Application_Links *app, View_Summary *view, String work_dir) {
    int32_t mem_size = 1024 * 1024;
    Partition mem = make_part(malloc(mem_size), mem_size);
    
    tld_StringList search_space = tld_fuzzy_construct_filename_list(app, work_dir, &mem, TLDFM_DEFAULT_DIR_FILTER);
    
    Query_Bar search_bar;
    char search_bar_space[TLDFM_MAX_QUERY_SIZE];
    search_bar.prompt = make_lit_string("Find File: ");
    search_bar.string = make_fixed_width_string(search_bar_space);
    start_query_bar(app, &search_bar, 0);
    
    String selected_file = tld_query_list_fuzzy(app, &search_bar, search_space);
    if (selected_file.str) {
        char full_path_space[1024];
        String full_path = make_fixed_width_string(full_path_space);
        copy_ss(&full_path, work_dir);
        append_ss(&full_path, selected_file);
        
        view_open_file(app, view, expand_str(full_path), false);
    }
    
    free(mem.base);
}

CUSTOM_COMMAND_SIG(tld_open_file_fuzzy) {
    View_Summary view = get_active_view(app, AccessAll);
    
    char home_directory_space[1024];
    String home_directory = make_fixed_width_string(home_directory_space);
    tld_get_home_directory(app, &home_directory);
    
    __tld_open_file_fuzzy_impl(app, &view, home_directory);
}

CUSTOM_COMMAND_SIG(tld_switch_buffer_fuzzy) {
    String list_space[128];
    tld_StringList search_space = {0};
    search_space.values = list_space;
    
    for (Buffer_Summary buffer = get_buffer_first(app, AccessAll);
         buffer.exists && search_space.count < 128;
         get_buffer_next(app, &buffer, AccessAll))
    {
        String buffer_name = make_string(buffer.buffer_name, buffer.buffer_name_len);
        search_space.values[search_space.count] = buffer_name;
        
        ++search_space.count;
    }
    
    Query_Bar search_bar;
    char search_bar_space[TLDFM_MAX_QUERY_SIZE];
    search_bar.prompt = make_lit_string("Switch Bufer: ");
    search_bar.string = make_fixed_width_string(search_bar_space);
    start_query_bar(app, &search_bar, 0);
    
    String buffer_name = tld_query_list_fuzzy(app, &search_bar, search_space);
    if (buffer_name.str) {
        View_Summary view = get_active_view(app, AccessAll);
        Buffer_Summary buffer = get_buffer_by_name(app, expand_str(buffer_name), AccessAll);
        view_set_buffer(app, &view, buffer.buffer_id, 0);
    }
}

#endif