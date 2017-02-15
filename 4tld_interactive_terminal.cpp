/******************************************************************************
File: 4tld_interactive_terminal.cpp
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
******************************************************************************/
#include "4tld_user_interface.h"

#ifndef TLDIT_COMMAND_HISTORY_CAPACITY
#define TLDIT_COMMAND_HISTORY_CAPACITY 128
#endif

static struct {
    String commands[TLDIT_COMMAND_HISTORY_CAPACITY];
    int32_t size;
    int32_t offset;
} tld_iterm_command_history = {0};

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
tld_iterm_handle_command(Application_Links *app, View_Summary *view, Buffer_Identifier buffer_id, String cmd, String *dir) {
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
    
    // TODO: Clear buffer manually
    
    int param_len = (int)(cmd_end - cmd.str);
    if (match_sc(ident, "cd")) {
        if (!tld_change_directory(dir, make_string(cmd.str, param_len)))
        {
            tld_show_error("Directory does not exist!");
        } else {
            // TODO: Do this manually using a file list, which we can use for
            // better formatting and auto complete
            exec_system_command(app, view, buffer_id, dir->str, dir->size,
                                literal("dir /OGN"),
                                CLI_OverlapWithConflict | CLI_CursorAtEnd);
        }
    } else if (match_sc(ident, "home")) {
        tld_iterm_get_home_directory(app, dir);
    } else if (match_sc(ident, "open")) {
        char full_path_space[1024];
        String full_path = make_fixed_width_string(full_path_space);
        copy_ss(&full_path, *dir);
        append_ss(&full_path, make_string(cmd.str, param_len));
        view_open_file(app, view, expand_str(full_path), false);
    } else {
        exec_system_command(app, view, buffer_id, dir->str, dir->size, expand_str(original_command),
                            CLI_OverlapWithConflict | CLI_CursorAtEnd);
    }
    
    // TODO: Insert whitespace to make text beneath query bars visible
}

static char tld_iterm_working_directory_space[1024] = {0};
static String tld_iterm_working_directory = {0};

CUSTOM_COMMAND_SIG(tld_iterm_session_start) {
    Buffer_Summary buffer;
    View_Summary view;
    tld_display_buffer_by_name(app, make_lit_string("*terminal*"),
                               &buffer, &view, false, AccessAll);
    Buffer_Identifier buffer_id = {0};
    buffer_id.id = buffer.buffer_id;
    
    if (tld_iterm_working_directory.str == 0) {
        tld_iterm_working_directory = make_fixed_width_string(tld_iterm_working_directory_space);
        tld_iterm_get_home_directory(app, &tld_iterm_working_directory);
    }
    Query_Bar dir_bar = {0};
    dir_bar.prompt = tld_iterm_working_directory;
    
    char cmd_space[1024];
    String cmd = make_fixed_width_string(cmd_space);
    Query_Bar cmd_bar = {0};
    cmd_bar.prompt = make_lit_string("> ");
    cmd_bar.string = cmd;
    
    start_query_bar(app, &cmd_bar, 0);
    start_query_bar(app, &dir_bar, 0);
    
    int32_t cmd_history_index = tld_iterm_command_history.size - 1;
    
    while (true) {
        User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc | EventOnButton);
        
        if (in.abort) return;
        
        bool good_character = false;
        if (key_is_unmodified(&in.key) && in.key.character != 0) {
            good_character = true;
        }
        
        if (in.type == UserInputKey) {
            if (in.key.keycode == '\n') {
                int32_t offset = (tld_iterm_command_history.size + tld_iterm_command_history.offset) % TLDIT_COMMAND_HISTORY_CAPACITY;
                
                if (tld_iterm_command_history.commands[offset].str)
                    free(tld_iterm_command_history.commands[offset].str);
                
                tld_iterm_command_history.commands[offset] = {0};
                tld_iterm_command_history.commands[offset].str = (char*)malloc(cmd_bar.string.size);
                tld_iterm_command_history.commands[offset].memory_size = cmd_bar.string.size;
                copy_partial_ss(&tld_iterm_command_history.commands[offset], cmd_bar.string);
                
                if (tld_iterm_command_history.size < TLDIT_COMMAND_HISTORY_CAPACITY) {
                    ++tld_iterm_command_history.size;
                } else {
                    tld_iterm_command_history.offset += 1;
                    tld_iterm_command_history.offset %= TLDIT_COMMAND_HISTORY_CAPACITY;
                }
                cmd_history_index = tld_iterm_command_history.size - 1;
                
                tld_iterm_handle_command(app, &view, buffer_id,
                                         cmd_bar.string, &dir_bar.prompt);
                tld_iterm_working_directory = dir_bar.prompt;
                cmd_bar.string.size = 0;
            } else if (in.key.keycode == key_up) {
                if (tld_iterm_command_history.size == 0) continue;
                
                int32_t _index = (cmd_history_index + tld_iterm_command_history.offset) % TLDIT_COMMAND_HISTORY_CAPACITY;
                cmd_bar.string.size = 0;
                append_partial_ss(&cmd_bar.string, tld_iterm_command_history.commands[_index]);
                
                if (cmd_history_index > 0)
                    --cmd_history_index;
                
            } else if (in.key.keycode == key_down) {
                if (tld_iterm_command_history.size == 0) continue;
                
                cmd_bar.string.size = 0;
                if (cmd_history_index + 1 < tld_iterm_command_history.size) {
                    ++cmd_history_index;
                    
                    int32_t _index = (cmd_history_index + tld_iterm_command_history.offset) % TLDIT_COMMAND_HISTORY_CAPACITY;
                    append_partial_ss(&cmd_bar.string, tld_iterm_command_history.commands[_index]);
                }
            } else if (in.key.keycode == 'q' && in.key.modifiers[MDFR_ALT]) {
                exec_command(app, kill_buffer);
                return;
            } else if (in.key.keycode == 'v' && in.key.modifiers[MDFR_ALT]) {
                char *append_pos =   cmd_bar.string.str + cmd_bar.string.size;
                int32_t append_len = cmd_bar.string.memory_size - cmd_bar.string.size;
                cmd_bar.string.size += clipboard_index(app, 0, 0, append_pos, append_len);
                if (cmd_bar.string.size > cmd_bar.string.memory_size) {
                    // NOTE: This happens sometimes because clipboard_index returns the length
                    // of the clipboard contents, not the number of bytes written.
                    
                    cmd_bar.string.size = cmd_bar.string.memory_size;
                }
            } else if (in.key.keycode == '\t') {
                // TODO: Auto complete
            } else if (in.key.keycode == key_back) {
                if (cmd_bar.string.size > 0) {
                    --cmd_bar.string.size;
                }
            } else if (in.key.keycode == key_del) {
                cmd_bar.string.size = 0;
            } else if (good_character) {
                append_s_char(&cmd_bar.string, in.key.character);
            }
        }
    }
}