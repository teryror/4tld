/******************************************************************************
File: 4tld_interactive_terminal.cpp
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
******************************************************************************/
#include "4tld_user_interface.h"

// NOTE: This is only needed because directory_cd(6) is broken...
static bool
tld_change_directory(String *dir, String rel_path) {
    bool success = false;
    
    String _dir;
    _dir.str         = (char *) malloc(dir->memory_size);
    _dir.size        = 0;
    _dir.memory_size = dir->memory_size;
    
    append_ss(&_dir, *dir);
    
    while (rel_path.size > 0) {
        String rel_path_element;
        rel_path_element.str = rel_path.str;
        rel_path_element.size = 0;
        
        while (rel_path.str[rel_path_element.size] != '/' &&
               rel_path.str[rel_path_element.size] != '\\' &&
               rel_path_element.size < rel_path.size)
        {
            ++rel_path_element.size;
        }
        
        if (rel_path_element.size == rel_path.size) {
            rel_path.size = 0;
        } else {
            rel_path.str += rel_path_element.size + 1;
            rel_path.size -= rel_path_element.size + 1;
        }
        
        if (match_sc(rel_path_element, "..")) {
            if (_dir.size > 0 && (_dir.str[_dir.size - 1] == '/' ||
                                  _dir.str[_dir.size - 1] == '\\'))
            {
                _dir.size -= 1;
            }
            _dir = path_of_directory(_dir);
        } else if (match_sc(rel_path_element, ".")) {
            // Do nothing -- we're already in the current directory
        } else {
            append_ss(&_dir, rel_path_element);
            append_s_char(&_dir, '/');
        }
    }
    
    /* file_exists() returns false on directories,
     * and I don't want to write OS specific code here,
     * though this does make the cd command kind of annoying...
    if (file_exists(app, expand_str(_dir))) */
    {
        success = true;
        
        dir->size = 0;
        append_ss(dir, _dir);
    }
    
    free(_dir.str);
    return success;
}

#ifdef TLDPM_IMPLEMENT_COMMANDS
static void
tld_iterm_get_home_directory(Application_Links *app, String *dest) {
    if (tld_current_project.working_directory.str) {
        copy_ss(dest, tld_current_project.working_directory);
    } else {
        dest->size = directory_get_hot(app, dest->str, dest->memory_size);
    }
}
#else
static void
tld_iterm_get_home_directory(Application_Links *app, String *dest) {
    dest->size = directory_get_hot(app, dest->str, dest->memory_size);
}
#endif

static void
tld_iterm_print_file_list(Application_Links *app,
                          Buffer_Identifier buffer_id,
                          File_List *file_list)
{
    Buffer_Summary buffer = get_buffer(app, buffer_id.id, AccessAll);
    buffer_replace_range(app, &buffer, 0, buffer.size, literal("\n\nDIRECTORIES:\n"));
    
    int32_t line_length = 0;
    
    for (int i = 0; i < file_list->count; ++i) {
        File_Info info = file_list->infos[i];
        if (info.folder == 0) continue;
        
        if (line_length) {
            if (line_length <= 43 && info.filename_len <= 40) {
                while (line_length < 45) {
                    buffer_replace_range(app, &buffer, buffer.size, buffer.size, literal(" "));
                    ++line_length;
                }
                
                buffer_replace_range(app, &buffer, buffer.size, buffer.size,
                                     info.filename, info.filename_len);
                line_length += info.filename_len;
            } else {
                buffer_replace_range(app, &buffer, buffer.size, buffer.size, literal("\n"));
                line_length = 0;
            }
        }
        
        if (line_length == 0) {
            buffer_replace_range(app, &buffer, buffer.size, buffer.size, literal("  "));
            buffer_replace_range(app, &buffer, buffer.size, buffer.size,
                                 info.filename, info.filename_len);
            
            line_length = 2 + info.filename_len;
        }
        
        buffer_replace_range(app, &buffer, buffer.size, buffer.size, literal("/"));
    }
    
    buffer_replace_range(app, &buffer, buffer.size, buffer.size, literal("\n\nFILES:\n"));
    line_length = 0;
    
    for (int i = 0; i < file_list->count; ++i) {
        File_Info info = file_list->infos[i];
        if (info.folder) continue;
        
        if (line_length) {
            if (line_length <= 43 && info.filename_len <= 40) {
                while (line_length < 45) {
                    buffer_replace_range(app, &buffer, buffer.size, buffer.size, literal(" "));
                    ++line_length;
                }
                
                buffer_replace_range(app, &buffer, buffer.size, buffer.size,
                                     info.filename, info.filename_len);
                line_length += info.filename_len;
            } else {
                buffer_replace_range(app, &buffer, buffer.size, buffer.size, literal("\n"));
                line_length = 0;
            }
        }
        
        if (line_length == 0) {
            buffer_replace_range(app, &buffer, buffer.size, buffer.size, literal("  "));
            buffer_replace_range(app, &buffer, buffer.size, buffer.size,
                                 info.filename, info.filename_len);
            
            line_length = 2 + info.filename_len;
        }
    }
}

static bool
tld_iterm_handle_command(Application_Links *app,
                         View_Summary *view,
                         Buffer_Identifier buffer_id,
                         String cmd, String *dir,
                         File_List *file_list)
{
    String original_command = cmd;
    
    char *cmd_end = cmd.str + cmd.size;
    while (cmd.str < cmd_end && *cmd.str == ' ') {
        ++cmd.str;
    }
    
    String ident = {0};
    ident.str = cmd.str;
    while (cmd.str < cmd_end && *cmd.str != ' ') {
        ++cmd.str;
    }
    ident.size = (int)(cmd.str - ident.str);
    
    while (cmd.str < cmd_end && *cmd.str == ' ') {
        ++cmd.str;
    }
    
    int param_len = (int)(cmd_end - cmd.str);
    if (match_sc(ident, "cd")) {
        if (!tld_change_directory(dir, make_string(cmd.str, param_len)))
        {
            tld_show_error("Directory does not exist!");
        } else {
            free_file_list(app, *file_list);
            *file_list = get_file_list(app, expand_str(*dir));
            tld_iterm_print_file_list(app, buffer_id, file_list);
        }
    } else if (match_sc(ident, "home")) {
        tld_iterm_get_home_directory(app, dir);
        free_file_list(app, *file_list);
        *file_list = get_file_list(app, expand_str(*dir));
        tld_iterm_print_file_list(app, buffer_id, file_list);
    } else if (match_sc(ident, "new")) {
        char full_path_space[1024];
        String full_path = make_fixed_width_string(full_path_space);
        copy_ss(&full_path, *dir);
        append_ss(&full_path, make_string(cmd.str, param_len));
        if (file_exists(app, expand_str(full_path))) {
            // TODO: We'll probably want to query if it should be overwritten, then?
            tld_show_error("File already exists!");
        } else {
            // TODO(Test): Will this run the new_file_hook?
            view_open_file(app, view, expand_str(full_path), false);
            return true;
        }
    } else if (match_sc(ident, "open")) {
        char full_path_space[1024];
        String full_path = make_fixed_width_string(full_path_space);
        copy_ss(&full_path, *dir);
        append_ss(&full_path, make_string(cmd.str, param_len));
        if (file_exists(app, expand_str(full_path))) {
            view_open_file(app, view, expand_str(full_path), false);
            return true;
        } else {
            tld_show_error("File does not exist!");
        }
    } else {
        exec_system_command(app, view, buffer_id, dir->str, dir->size, expand_str(original_command),
                            CLI_OverlapWithConflict | CLI_CursorAtEnd);
        Buffer_Summary buffer = get_buffer(app, buffer_id.id, AccessAll);
        buffer_replace_range(app, &buffer, 0, 0, literal("\n\n"));
    }
    
    return false;
}

#ifndef TLDIT_COMMAND_HISTORY_CAPACITY
#define TLDIT_COMMAND_HISTORY_CAPACITY 128
#endif

struct tld_TerminalCommandHistory {
    String commands[TLDIT_COMMAND_HISTORY_CAPACITY];
    int32_t size;
    int32_t offset;
};

static bool
tld_iterm_query_user_command(Application_Links *app,
                             Buffer_Identifier buffer, View_Summary *view,
                             Query_Bar *cmd_bar, Query_Bar *dir_bar,
                             tld_TerminalCommandHistory *history,
                             File_List *file_list)
{
    int32_t cmd_history_index = history->size - 1;
    
    while (true) {
        User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc | EventOnButton);
        
        if (in.abort) return false;
        
        if (in.type == UserInputKey && in.key.keycode == '\t') {
            if (cmd_bar->string.size == 0) continue;
            
            Range incomplete_string = {0};
            incomplete_string.max = cmd_bar->string.size;
            incomplete_string.min = incomplete_string.max - 1;
            while (incomplete_string.min > 0 &&
                   cmd_bar->string.str[incomplete_string.min] != ' ' &&
                   cmd_bar->string.str[incomplete_string.min] != '\t')
            {
                --incomplete_string.min;
            }
            if (incomplete_string.min > 0) {
                ++incomplete_string.min;
            }
            
            String prefix = cmd_bar->string;
            prefix.str += incomplete_string.min;
            prefix.size = incomplete_string.max - incomplete_string.min;
            
            int32_t file_index = -1;
            while (in.type == UserInputKey && in.key.keycode == '\t') {
                for (++file_index; file_index < file_list->count; ++file_index) {
                    if (match_part_insensitive_cs(file_list->infos[file_index].filename, prefix)) {
                        break;
                    }
                }
                
                cmd_bar->string.size = incomplete_string.max;
                if (file_index < file_list->count) {
                    append_ss(&cmd_bar->string, make_string(file_list->infos[file_index].filename + prefix.size, file_list->infos[file_index].filename_len - prefix.size));
                } else {
                    file_index = -1;
                }
                
                in = get_user_input(app, EventOnAnyKey, EventOnEsc | EventOnButton);
            }
        }
        
        bool good_character = false;
        if (key_is_unmodified(&in.key) && in.key.character != 0) {
            good_character = true;
        }
        
        if (in.type == UserInputKey) {
            if (in.key.keycode == '\n') {
                int32_t offset = (history->size + history->offset) % TLDIT_COMMAND_HISTORY_CAPACITY;
                
                if (history->commands[offset].str)
                    free(history->commands[offset].str);
                
                history->commands[offset] = {0};
                history->commands[offset].str = (char*)malloc(cmd_bar->string.size);
                history->commands[offset].memory_size = cmd_bar->string.size;
                copy_partial_ss(&history->commands[offset], cmd_bar->string);
                
                if (history->size < TLDIT_COMMAND_HISTORY_CAPACITY) {
                    ++history->size;
                } else {
                    history->offset += 1;
                    history->offset %= TLDIT_COMMAND_HISTORY_CAPACITY;
                }
                cmd_history_index = history->size - 1;
                
                bool should_exit = tld_iterm_handle_command(app, view, buffer, cmd_bar->string, &dir_bar->prompt, file_list);
                cmd_bar->string.size = 0;
                
                return !should_exit;
            } else if (in.key.keycode == key_up) {
                if (history->size == 0) continue;
                
                int32_t _index = (cmd_history_index + history->offset) % TLDIT_COMMAND_HISTORY_CAPACITY;
                cmd_bar->string.size = 0;
                append_partial_ss(&cmd_bar->string, history->commands[_index]);
                
                if (cmd_history_index > 0)
                    --cmd_history_index;
                
            } else if (in.key.keycode == key_down) {
                if (history->size == 0) continue;
                
                cmd_bar->string.size = 0;
                if (cmd_history_index + 1 < history->size) {
                    ++cmd_history_index;
                    
                    int32_t _index = (cmd_history_index + history->offset) % TLDIT_COMMAND_HISTORY_CAPACITY;
                    append_partial_ss(&cmd_bar->string, history->commands[_index]);
                }
            } else if (in.key.keycode == 'q' && in.key.modifiers[MDFR_ALT]) {
                exec_command(app, kill_buffer);
                return false;
            } else if (in.key.keycode == 'p' && in.key.modifiers[MDFR_ALT]) {
                exec_command(app, cmdid_interactive_switch_buffer);
                return false;
            } else if (in.key.keycode == 'v' && in.key.modifiers[MDFR_ALT]) {
                char *append_pos =   cmd_bar->string.str + cmd_bar->string.size;
                int32_t append_len = cmd_bar->string.memory_size - cmd_bar->string.size;
                cmd_bar->string.size += clipboard_index(app, 0, 0, append_pos, append_len);
                if (cmd_bar->string.size > cmd_bar->string.memory_size) {
                    // NOTE: This happens sometimes because clipboard_index returns the length
                    // of the clipboard contents, not the number of bytes written.
                    
                    cmd_bar->string.size = cmd_bar->string.memory_size;
                }
            } else if (in.key.keycode == key_back) {
                if (cmd_bar->string.size > 0) {
                    --cmd_bar->string.size;
                }
            } else if (in.key.keycode == key_del) {
                cmd_bar->string.size = 0;
            } else if (good_character) {
                append_s_char(&cmd_bar->string, in.key.character);
            }
        }
    }
}

#ifdef TLDIT_IMPLEMENT_COMMANDS

static tld_TerminalCommandHistory tld_iterm_command_history = {0};
static char tld_iterm_working_directory_space[1024] = {0};
static String tld_iterm_working_directory = {0};

static void
tld_iterm_init_session(Application_Links *app,
                       Buffer_Identifier *buffer, View_Summary *view,
                       Query_Bar *cmd_bar, String cmd_space,
                       Query_Bar *dir_bar,
                       File_List *file_list)
{
    if (tld_iterm_working_directory.str == 0) {
        tld_iterm_working_directory = make_fixed_width_string(tld_iterm_working_directory_space);
        tld_iterm_get_home_directory(app, &tld_iterm_working_directory);
    }
    
    Buffer_Summary buffer_summary;
    tld_display_buffer_by_name(app, make_lit_string("*terminal*"),
                               &buffer_summary, view, false, AccessAll);
    *buffer = {0};
    buffer->id = buffer_summary.buffer_id;
    
    *cmd_bar = {0};
    cmd_bar->prompt = make_lit_string("> ");
    cmd_bar->string = cmd_space;
    
    *dir_bar = {0};
    dir_bar->prompt = tld_iterm_working_directory;
    
    start_query_bar(app, cmd_bar, 0);
    start_query_bar(app, dir_bar, 0);
    
    *file_list = get_file_list(app, expand_str(dir_bar->prompt));
    if (buffer_summary.size == 0) {
        tld_iterm_print_file_list(app, *buffer, file_list);
    }
}

CUSTOM_COMMAND_SIG(tld_terminal_begin_interactive_session) {
    View_Summary view;
    Buffer_Identifier buffer_id;
    Query_Bar cmd_bar, dir_bar;
    File_List file_list;
    
    char cmd_space[1024];
    String cmd = make_fixed_width_string(cmd_space);
    
    tld_iterm_init_session(app, &buffer_id, &view, &cmd_bar, cmd, &dir_bar, &file_list);
    
    bool ran_command;
    do {
        ran_command = tld_iterm_query_user_command(app, buffer_id, &view, &cmd_bar, &dir_bar, &tld_iterm_command_history, &file_list);
    } while (ran_command);
    
    tld_iterm_working_directory = dir_bar.prompt;
}

CUSTOM_COMMAND_SIG(tld_terminal_single_command) {
    View_Summary view;
    Buffer_Identifier buffer_id;
    Query_Bar cmd_bar, dir_bar;
    File_List file_list;
    
    char cmd_space[1024];
    String cmd = make_fixed_width_string(cmd_space);
    
    tld_iterm_init_session(app, &buffer_id, &view, &cmd_bar, cmd, &dir_bar, &file_list);
    tld_iterm_query_user_command(app, buffer_id, &view, &cmd_bar, &dir_bar, &tld_iterm_command_history, &file_list);
    tld_iterm_working_directory = dir_bar.prompt;
}

#endif