/******************************************************************************
File: 4tld_fuzzy_match.cpp
Author: Tristan Dannenberg
Notice: No warranty is offered or imlied; use this code at your own risk.
*******************************************************************************
LICENSE

This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.
*******************************************************************************
This file hosts a fuzzy list panel, as you may know it from Sublime Text,
TextMate, Atom Text, and other editors. If you would like to know how this is
implemented, there is a detailed write-up about it at txt/fuzz.md.

Preprocessor Variables:
* TLDFM_MAX_QUERY_SIZE - The maximum length of a pattern to search a list with.
  This is not intended to be modified, as some of the optimizations that make
  the search with long patterns fast rely on this being no greater than 64,
  and setting it to anything below would be pointless in most use cases.
* TLDFM_DUMB_HEURISTIC - If this is defined, you will get a faster heuristic,
  which will give very different results. To some users, this may be more
  intuitive, while others may want to search so ridiculously big data sets they
  cannot afford the smart heuristic.
* TLDFM_IMPLEMENT_COMMANDS - If this is defined, the custom commands listed
  below are implemented. This is set up this way, so that you can define your
  own variations without compiling commands you won't use, and other command
  packs can utilize these commands if you _are_ using them.
* TLD_HOME_DIRECTORY - a flag designating whether the function
  tld_get_home_directory(ApplicatioN_Links, String) is defined. This is used
  to determine what directory to look in when constructing the list of files
  you can open with the fuzzy open command. If you first include
  4tld_project_management, the project's working directory is used instead.
* TLDFM_DEFAULT_DIR_FILTER - The name of the function that filters whether the
  function that constructs the list of files that can be opened should recurse
  on a given directory name. If this is not defined, a default implementation
  ignoring hidden directories (starting with a '.' character) is provided.
  You can easily define a matching function with the TLDFM_DIR_FILTER_SIG
  macro, which works just like the CUSTOM_COMMAND_SIG macro you should already
  know.
* TLDFM_FILE_LIST_CAPACITY - The number of file names the file list
  construction function should allocate space for. This defaults to (16 * 1024).
  
Provided Commands:
* tld_open_file_fuzzy
* tld_switch_buffer_fuzzy
* tld_execute_arbitrary_command_fuzzy
* tld_list_named_commands

Note that, to use tld_execute_arbitrary_command_fuzzy, you will need to
initialize the global command list. This requires use of the
tld_push_named_command function. A number of rarely used commands from the
default customization layer are pushed in the tld_push_default_command_names
function.
******************************************************************************/

#define TLDFM_MAX_QUERY_SIZE 64

#ifdef TLDFM_DUMB_HEURISTIC
// Check if `val` contains all characters in `key` in the same order.
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

static inline b32_4tech
tld_char_is_separator(char a) {
    return (a == ' ' || a == '_' || a == '-' ||
            a == '.' || a == '/' || a == '\\');
}

static inline b32_4tech
tld_fuzzy_match_char(char a, char b) {
    return ((char_to_lower(a) == char_to_lower(b)) ||
            (char_is_upper(a) && a == b) ||
            (a == ' ' && tld_char_is_separator(b))
            );
}

#ifndef max
#define max(a, b) ((a > b) ? a : b)
#endif
#ifndef min
#define min(a, b) ((a < b) ? a : b)
#endif

// This is where the magic happens. Return higher scores for better matches.
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
    
    int j_lo = j;
    
    for (; j < val.size; ++j) {
        int32_t diag = 1;
        int32_t diag_l = 0;
        
        // Optimization 2: Skip triangular table sections that don't affect the result
        int i_lo = max(j + key.size - val.size, 0);
        int i_hi = min(j - j_lo + 1, key.size);
        
        if (i_lo > 0) {
            diag = row[i_lo - 1];
            diag_l = lml[i_lo - 1];
        }
        
        for (int i = i_lo; i < i_hi; ++i) {
            int32_t row_old = row[i];
            int32_t lml_old = lml[i];
            
            int32_t match = tld_fuzzy_match_char(key.str[i], val.str[j]);
            lml[i] = match * (diag_l + 1);
            
            if (diag > 0 && match) {
                // Sequential match bonus:
                int32_t value = lml[i];
                
                if (j == 0 || (char_is_lower(val.str[j - 1]) && char_is_upper(val.str[j])) ||
                    tld_char_is_separator(val.str[j - 1]))
                {   // Abbreviation bonus:
                    value += 4;
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

// A String with meta data that allows us to speed up the query
struct tld_String {
    String value;
    uint64_t bit_stack;
};

// An array of tld_String to be used by the query function
struct tld_StringList {
    tld_String *items;
    int32_t count;
};

// Query a user for a pattern to search the list with, show the top 7 results
// (or fewer, if not enough strings match the pattern, even with a low score).
// Navigate the result list with the arrow keys, press enter to accept the selected String.
// 
// Returns the index of the selected string in the provided StringList,
// or -1 if the query was canceled with ESC.
static int32_t
tld_query_list_fuzzy(Application_Links *app, Query_Bar *search_bar, tld_StringList list) {
    int32_t result_indices[7];
    
    String empty = make_lit_string("");
    Query_Bar result_bars[ArrayCount(result_indices)] = {0};
    for (int i = 0; i < ArrayCount(result_indices); ++i) {
        result_bars[i].string = empty;
        result_bars[i].prompt = empty;
    }
    
    int result_count = 0;
    int result_selected_index = 0;
    bool full_rescan = true;
    uint64_t current_mask = 1;
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
            
            int32_t result_scores[ArrayCount(result_indices)];
            
            for (int i = 0; i < list.count; ++i) {
                String candidate = list.items[i].value;
                
                if (full_rescan || list.items[i].bit_stack & current_mask) {
                    int32_t score = tld_fuzzy_match_ss(search_bar->string, candidate);
                    for (int j = 1; j < search_bar->string.size; ++j) {
                        char temp = search_bar->string.str[j - 1];
                        search_bar->string.str[j - 1] = search_bar->string.str[j];
                        search_bar->string.str[j] = temp;
                        
                        int32_t score_transpose = tld_fuzzy_match_ss(search_bar->string, candidate) / 4;
                        
                        search_bar->string.str[j] = search_bar->string.str[j - 1];
                        search_bar->string.str[j - 1] = temp;
                        
                        if (score < score_transpose) {
                            score = score_transpose;
                        }
                    }
                    
                    if (score <= 0) {
                        list.items[i].bit_stack &= ~(1ULL << search_bar->string.size);
                    } else {
                        list.items[i].bit_stack |= 1ULL << search_bar->string.size;
                        
                        if (result_count < ArrayCount(result_indices)) {
                            result_indices[result_count] = i;
                            result_scores[result_count] = score;
                            
                            if (score > min_score)
                            {
                                min_score = score;
                                min_index = result_count;
                            }
                            
                            ++result_count;
                        } else if (score > result_scores[min_index]) {
                            result_indices[min_index] = i;
                            result_scores[min_index] = score;
                            
                            min_index = 0;
                            min_score = result_scores[0];
                            
                            for (int j = 1; j <= ArrayCount(result_indices); ++j) {
                                if (result_scores[j] < min_score) {
                                    min_score = result_scores[j];
                                    min_index = j;
                                }
                            }
                        }
                    }
                }
            }
            
            for (int i = 0; i < result_count - 1; ++i) {
                for (int j = i + 1; j < result_count; ++j) {
                    if (result_scores[i] < result_scores[j])
                    {
                        int32_t s  = result_indices[i];
                        result_indices[i] = result_indices[j];
                        result_indices[j] = s;
                        
                        int32_t c = result_scores[i];
                        result_scores[i] = result_scores[j];
                        result_scores[j] = c;
                    }
                }
            }
            
            for (int i = result_count - 1; i >= 0; --i) {
                result_bars[i].string = list.items[result_indices[i]].value;
                start_query_bar(app, &result_bars[i], 0);
            }
            
            start_query_bar(app, search_bar, 0);
        }
        
        if (selected_index_changed || search_key_changed) {
            for (int i = 0; i < result_count; ++i) {
                if (i == result_selected_index) {
                    result_bars[i].prompt = list.items[result_indices[i]].value;
                    result_bars[i].string = empty;
                } else {
                    result_bars[i].prompt = empty;
                    result_bars[i].string = list.items[result_indices[i]].value;
                }
            }
        }
        
        User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
        selected_index_changed = false;
        search_key_changed = false;
        full_rescan = false;
        
        if (in.abort) {
            for (int i = 0; i < ArrayCount(result_indices); ++i) {
                end_query_bar(app, &result_bars[i], 0);
            }
            
            return -1;
        }
        
        if (in.type == UserInputKey) {
            if (in.key.keycode == '\n') {
                if (result_count > 0) {
                    if (result_selected_index < 0)
                        result_selected_index = 0;
                    
                    for (int i = 0; i < ArrayCount(result_indices); ++i) {
                        end_query_bar(app, &result_bars[i], 0);
                    }
                    
                    return result_indices[result_selected_index];
                }
            } else if (in.key.keycode == key_back) {
                if (search_bar->string.size > 0) {
                    --search_bar->string.size;
                    search_key_changed = true;
                }
                current_mask = 1ULL << search_bar->string.size;
            } else if (in.key.keycode == key_del) {
                search_bar->string.size = 0;
                result_selected_index = 0;
                
                for (int i = result_count - 1; i >= 0; --i) {
                    end_query_bar(app, &result_bars[i], 0);
                }
                
                full_rescan = true;
                current_mask = 1;
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
                
                current_mask = 1ULL << (search_bar->string.size - 1);
            }
        }
    }
}

#ifdef TLDFM_IMPLEMENT_COMMANDS

#ifndef TLD_HOME_DIRECTORY
#define TLD_HOME_DIRECTORY
static void
tld_get_home_directory(Application_Links *app, String *dest) {
    dest->size = directory_get_hot(app, dest->str, dest->memory_size);
}
#endif

#ifndef TLD_DEFAULT_OPEN_FILE_COMMAND
#define TLD_DEFAULT_OPEN_FILE_COMMAND tld_open_file_fuzzy
#endif

#define TLDFM_DIR_FILTER_SIG(name) static bool name(char *filename, int32_t filename_len)

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
#define TLDFM_FILE_LIST_CAPACITY (16 * 1024)
#endif

#ifndef TLDFM_DIR_QUEUE_CAPACITY
#define TLDFM_DIR_QUEUE_CAPACITY (TLDFM_FILE_LIST_CAPACITY / 16)
#endif

// Recursively gets all files in a directory and all its subdirectories.
static tld_StringList
tld_fuzzy_construct_filename_list(Application_Links *app,
                                  String work_dir,
                                  Partition *memory,
                                  bool (*dir_filter)(char *, int32_t))
{
    tld_StringList result = {0};
    result.items = push_array(memory, tld_String, TLDFM_FILE_LIST_CAPACITY);
    
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
                    result.items[result.count].value.str = filename_space;
                    result.items[result.count].value.memory_size = filename_size;
                    copy_ss(&result.items[result.count].value, dir_name);
                    append_ss(&result.items[result.count].value,
                              make_string(info_i.filename, info_i.filename_len));
                    
                    result.count += 1;
                }
            }
        }
        
        free_file_list(app, dir_info);
    }
    
    return result;
}

// Construct the file list from the given working directory,
// perform the fuzzy query, and open the selected file.
// Returns `true` if a file was opened, `false` otherwise.
static bool32
__tld_open_file_fuzzy_impl(Application_Links *app, View_Summary *view, String work_dir) {
    bool32 result;
    
    int32_t mem_size = 1024 * 1024;
    Partition mem = make_part(malloc(mem_size), mem_size);
    
    tld_StringList search_space = tld_fuzzy_construct_filename_list(app, work_dir, &mem, TLDFM_DEFAULT_DIR_FILTER);
    
    Query_Bar search_bar;
    char search_bar_space[TLDFM_MAX_QUERY_SIZE];
    search_bar.prompt = make_lit_string("Find File: ");
    search_bar.string = make_fixed_width_string(search_bar_space);
    start_query_bar(app, &search_bar, 0);
    
    int32_t selected_file_index = tld_query_list_fuzzy(app, &search_bar, search_space);
    result = (selected_file_index >= 0);
    
    if (result) {
        String selected_file = search_space.items[selected_file_index].value;
        char full_path_space[1024];
        String full_path = make_fixed_width_string(full_path_space);
        copy_ss(&full_path, work_dir);
        append_ss(&full_path, selected_file);
        
        view_open_file(app, view, expand_str(full_path), false);
    }
    
    free(mem.base);
    end_query_bar(app, &search_bar, 0);
    return result;
}

// Open a file in the home directory or one of its subdirectories.
CUSTOM_COMMAND_SIG(tld_open_file_fuzzy) {
    View_Summary view = get_active_view(app, AccessAll);
    
    char home_directory_space[1024];
    String home_directory = make_fixed_width_string(home_directory_space);
    tld_get_home_directory(app, &home_directory);
    
    __tld_open_file_fuzzy_impl(app, &view, home_directory);
}

// Switch to the selected buffer.
CUSTOM_COMMAND_SIG(tld_switch_buffer_fuzzy) {
    tld_String list_space[128];
    tld_StringList search_space = {0};
    search_space.items = list_space;
    
    for (Buffer_Summary buffer = get_buffer_first(app, AccessAll);
         buffer.exists && search_space.count < 128;
         get_buffer_next(app, &buffer, AccessAll))
    {
        String buffer_name = make_string(buffer.buffer_name, buffer.buffer_name_len);
        search_space.items[search_space.count].value = buffer_name;
        
        ++search_space.count;
    }
    
    Query_Bar search_bar;
    char search_bar_space[TLDFM_MAX_QUERY_SIZE];
    search_bar.prompt = make_lit_string("Switch Bufer: ");
    search_bar.string = make_fixed_width_string(search_bar_space);
    start_query_bar(app, &search_bar, 0);
    
    int32_t buffer_name_index = tld_query_list_fuzzy(app, &search_bar, search_space);
    if (buffer_name_index >= 0) {
        String buffer_name = search_space.items[buffer_name_index].value;
        View_Summary view = get_active_view(app, AccessAll);
        Buffer_Summary buffer = get_buffer_by_name(app, expand_str(buffer_name), AccessAll);
        view_set_buffer(app, &view, buffer.buffer_id, 0);
    }
}

typedef Custom_Command_Function *Custom_Command_Function_Pointer;
static  tld_StringList tld_command_names = {0};
static  Custom_Command_Function_Pointer *tld_command_functions = 0;

// Select a command from the fuzzy list and execute it.
CUSTOM_COMMAND_SIG(tld_execute_arbitrary_command_fuzzy) {
    if (tld_command_functions == 0) return;
    
    char search_bar_space[TLDFM_MAX_QUERY_SIZE];
    Query_Bar search_bar;
    search_bar.prompt = make_lit_string("Command: ");
    search_bar.string = make_fixed_width_string(search_bar_space);
    
    start_query_bar(app, &search_bar, 0);
    int32_t command_index = tld_query_list_fuzzy(app, &search_bar, tld_command_names);
    end_query_bar(app, &search_bar, 0);
    
    if (command_index >= 0) {
        exec_command(app, tld_command_functions[command_index]);
    }
}

// List all commands on the fuzzy list in the *help* buffer.
CUSTOM_COMMAND_SIG(tld_list_named_commands) {
    Buffer_Summary buffer;
    tld_display_buffer_by_name(app, make_lit_string("*help*"), &buffer, 0, 0, AccessAll);
    buffer_replace_range(app, &buffer, 0, buffer.size, 0, 0);
    
    for (int i = 0; i < tld_command_names.count; ++i) {
        buffer_replace_range(app, &buffer, buffer.size, buffer.size, expand_str(tld_command_names.items[i].value));
        buffer_replace_range(app, &buffer, buffer.size, buffer.size, literal("\n"));
    }
}

// Add a command to the fuzzy list.
static inline bool32
tld_push_named_command(Custom_Command_Function *cmd, String cmd_name) {
    if (tld_command_names.items == 0) {
        tld_command_names.items = (tld_String *)malloc(sizeof(tld_String) * 1024);
    }
    
    if (tld_command_functions == 0) {
        tld_command_functions = (Custom_Command_Function_Pointer *)malloc(sizeof(Custom_Command_Function_Pointer) * 1024);
    }
    
    if (tld_command_names.count < 1024) {
        tld_command_names.items[tld_command_names.count] = {0};
        tld_command_names.items[tld_command_names.count].value = cmd_name;
        
        tld_command_functions[tld_command_names.count] = cmd;
        
        ++tld_command_names.count;
        return true;
    }
    
    return false;
}

// Add the default commands to the fuzzy list.
static inline void
tld_push_default_command_names() {
    tld_push_named_command(exit_4coder, make_lit_string("System: exit 4coder"));
    tld_push_named_command(toggle_mouse, make_lit_string("System: toggle mouse suppression"));
    tld_push_named_command(tld_list_named_commands, make_lit_string("System: list named commands"));
    
    tld_push_named_command(to_uppercase, make_lit_string("Fancy Editing: range to uppercase"));
    tld_push_named_command(to_lowercase, make_lit_string("Fancy Editing: range to lowercase"));
    tld_push_named_command(if0_off, make_lit_string("Fancy Editing: surround with if 0"));
    tld_push_named_command(auto_tab_range, make_lit_string("Fancy Editing: autotab range"));
    tld_push_named_command(auto_tab_whole_file,
                           make_lit_string("Fancy Editing: autotab whole file"));
    
    tld_push_named_command(show_scrollbar, make_lit_string("View: show scrollbar"));
    tld_push_named_command(hide_scrollbar, make_lit_string("View: hide scrollbar"));
    tld_push_named_command(open_panel_vsplit, make_lit_string("View: split vertically"));
    tld_push_named_command(open_panel_hsplit, make_lit_string("View: split horizontally"));
    tld_push_named_command(close_panel, make_lit_string("View: close"));
    
    tld_push_named_command(kill_buffer, make_lit_string("Buffer: close"));
    tld_push_named_command(eol_nixify, make_lit_string("Buffer: nixify line endings"));
    tld_push_named_command(eol_dosify, make_lit_string("Buffer: dosify line endings"));
    tld_push_named_command(toggle_line_wrap, make_lit_string("Buffer: toggle line wrap"));
    tld_push_named_command(toggle_virtual_whitespace,
                           make_lit_string("Buffer: toggle virtual whitespace"));
    
    tld_push_named_command(open_file_in_quotes, make_lit_string("Files: open filename in quotes"));
    tld_push_named_command(open_matching_file_cpp,
                           make_lit_string("Files: open matching cpp/h file"));
}

#endif