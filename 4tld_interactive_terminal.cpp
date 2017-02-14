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
            append_ss(&_dir, make_lit_string("/"));
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

static bool
tld_iterm_handle_command(Application_Links *app, String cmd, String *dir) {
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
        }
        
        return true;
    }
    
    return false;
}

CUSTOM_COMMAND_SIG(tld_iterm_session_start) {
    Buffer_Summary buffer;
    View_Summary view;
    tld_display_buffer_by_name(app, make_lit_string("*terminal*"),
                               &buffer, &view, false, AccessAll);
    Buffer_Identifier buffer_id = {0};
    buffer_id.id = buffer.buffer_id;
    
    char dir_space[1024];
    String dir = make_fixed_width_string(dir_space);
    dir.size = directory_get_hot(app, dir.str, dir.memory_size);
    Query_Bar dir_bar = {0};
    dir_bar.prompt = dir;
    
    char cmd_space[1024];
    String cmd = make_fixed_width_string(cmd_space);
    Query_Bar cmd_bar = {0};
    cmd_bar.prompt = make_lit_string("> ");
    cmd_bar.string = cmd;
    
    start_query_bar(app, &cmd_bar, 0);
    start_query_bar(app, &dir_bar, 0);
    
    while (tld_requery_user_string(app, &cmd_bar)) {
        if (!tld_iterm_handle_command(app, cmd_bar.string, &dir_bar.prompt)) {
            exec_system_command(app, &view, buffer_id,
                                expand_str(dir_bar.string), expand_str(cmd_bar.string),
                                CLI_OverlapWithConflict | CLI_CursorAtEnd);
        }
        
        cmd_bar.string.size = 0;
    }
}