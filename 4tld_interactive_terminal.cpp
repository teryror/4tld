/******************************************************************************
File: 4tld_interactive_terminal.cpp
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
*******************************************************************************
LICENSE

This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.
*******************************************************************************
This file provides a looped and improved version of the default execute_any_cli
command. It includes a command history, which can be traversed with the up and
down arrow keys, auto-completion of file paths, as well as other shortcuts to
directly go to other interactive commands.

The built-in commands the terminal understands:
* cd   - Accepts a path relative to the working directory, sets the working
  directory to it, and pretty prints the new working directory's contents.
* home - Gets the working directory, sets the working directory to it, and
  pretty prints its contents. Does not accept any arguments.
* new  - Opens a new buffer with the specified file name, complaining if the
  specified file already exists.
* open - Opens the specified file in a new buffer, complaining if it does not
  exist. Accepts a relative path as argument.
* quit - Exits 4coder. Does not accept any arguments.

Preprocessor Variables:
* TLD_HOME_DIRECTORY - a flag designating whether the function
  tld_get_home_directory(ApplicatioN_Links, String) is defined. This is used
  to determine what directory to change to when executing the built-in
  command `home`. If this is not defined, a default implementation is
  provided. If you first include 4tld_project_management, the project's
  working directory is used instead.
* TLDFM_IMPLEMENT_COMMANDS - If this is defined, the terminal assumes that the
  commands provided by 4tld_fuzzy_match.cpp are available, and will use them
  instead.
* TLDIT_IMPLEMENT_COMMANDS - If this is defined, the custom commands listed
  below are implemented. This is set up this way, so that you can define your
  own variations without compiling commands you won't use, and other command
  packs can utilize these commands if you _are_ using them.
* TLDIT_COMMAND_HISTORY_CAPACITY - The maximum number of commands that will be
  stored in the command history. This defaults to 128.
  Only used if you defined TLDIT_IMPLEMENT_COMMANDS. 
  
Provided Commands:
* tld_terminal_begin_interactive_session
* tld_terminal_single_command
******************************************************************************/
#include "4tld_user_interface.h"

#ifndef TLD_HOME_DIRECTORY
#define TLD_HOME_DIRECTORY
static void
tld_get_home_directory(Application_Links *app, String *dest) {
    dest->size = directory_get_hot(app, dest->str, dest->memory_size);
}
#endif

// Pretty prints a File_List to the specified buffer in two columns, with directories listed first.
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

// Split a command line into two parts, execute built-in commands,
// or hand the full line of to exec_system_command.
// Returns `true` if the interactive session should end after the command.
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
        tld_get_home_directory(app, dir);
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
    } else if (match_sc(ident, "quit")) {
        exec_command(app, exit_4coder);
        return true;
    } else {
        exec_system_command(app, view, buffer_id, dir->str, dir->size, expand_str(original_command),
                            CLI_OverlapWithConflict | CLI_CursorAtEnd);
        Buffer_Summary buffer = get_buffer(app, buffer_id.id, AccessAll);
        buffer_replace_range(app, &buffer, 0, 0, literal("\n\n"));
    }
    
    return false;
}

#ifdef TLDFM_IMPLEMENT_COMMANDS

#define tld_iterm_switch_buffer_interactive exec_command(app, tld_switch_buffer_fuzzy)
#define tld_iterm_open_file_interactive(app, view, work_dir) __tld_open_file_fuzzy_impl(app, view, work_dir)

#else

#define tld_iterm_switch_buffer_interactive exec_command(app, cmdid_interactive_switch_buffer)
#define tld_iterm_open_file_interactive(...) exec_command(app, cmdid_interactive_open)

#endif

// The custom query that combines auto-completion of file names and command history.
static bool32
tld_iterm_query_user_command(Application_Links *app,
                             Buffer_Identifier buffer, View_Summary *view,
                             Query_Bar *cmd_bar, Query_Bar *dir_bar,
                             tld_StringHistory *history,
                             File_List *file_list)
{
    int32_t cmd_history_index = history->size - 1;
    
    while (true) {
        User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
        
        tld_query_complete_filenames(app, &in, '\t', &cmd_bar->string, file_list, dir_bar->prompt);
        tld_query_traverse_history(in, key_up, key_down, &cmd_bar->string, history, &cmd_history_index);
        
        if (in.abort) return false;
        
        bool good_character = false;
        if (key_is_unmodified(&in.key) && in.key.character != 0) {
            good_character = true;
        }
        
        if (in.type == UserInputKey) {
            if (in.key.keycode == '\n') {
                tld_string_history_push(history, cmd_bar->string);
                cmd_history_index = history->size - 1;
                return true;
            } else if (in.key.keycode == 'o' && in.key.modifiers[MDFR_ALT]) {
                end_query_bar(app, cmd_bar, 0);
                end_query_bar(app, dir_bar, 0);
                
                if (tld_iterm_open_file_interactive(app, view, dir_bar->prompt)) {
                    return false;
                }
                
                start_query_bar(app, cmd_bar, 0);
                start_query_bar(app, dir_bar, 0);
            } else if (in.key.keycode == 'q' && in.key.modifiers[MDFR_ALT]) {
                exec_command(app, kill_buffer);
                return false;
            } else if (in.key.keycode == 'p' && in.key.modifiers[MDFR_ALT]) {
                end_query_bar(app, cmd_bar, 0);
                end_query_bar(app, dir_bar, 0);
                
                tld_iterm_switch_buffer_interactive;
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

#ifndef TLDIT_COMMAND_HISTORY_CAPACITY
#define TLDIT_COMMAND_HISTORY_CAPACITY 128
#endif

static String tld_iterm_command_history_space[TLDIT_COMMAND_HISTORY_CAPACITY];
static tld_StringHistory tld_iterm_command_history = {0};
static char tld_iterm_working_directory_space[1024] = {0};
static String tld_iterm_working_directory = {0};

// Prepare the global state for a new terminal session.
static void
tld_iterm_init_session(Application_Links *app,
                       Buffer_Identifier *buffer, View_Summary *view,
                       Query_Bar *cmd_bar, String cmd_space,
                       Query_Bar *dir_bar,
                       File_List *file_list)
{
    if (tld_iterm_command_history.strings == 0) {
        tld_iterm_command_history.strings = tld_iterm_command_history_space;
        tld_iterm_command_history.capacity = TLDIT_COMMAND_HISTORY_CAPACITY;
    }
    
    if (tld_iterm_working_directory.str == 0) {
        tld_iterm_working_directory = make_fixed_width_string(tld_iterm_working_directory_space);
        tld_get_home_directory(app, &tld_iterm_working_directory);
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

// The looped version of the command. Continue entering commands until the user hits ESC.
CUSTOM_COMMAND_SIG(tld_terminal_begin_interactive_session) {
    View_Summary view;
    Buffer_Identifier buffer_id;
    Query_Bar cmd_bar, dir_bar;
    File_List file_list;
    
    char cmd_space[1024];
    String cmd = make_fixed_width_string(cmd_space);
    
    tld_iterm_init_session(app, &buffer_id, &view, &cmd_bar, cmd, &dir_bar, &file_list);
    
    bool should_exit;
    do {
        if (tld_iterm_query_user_command(app, buffer_id, &view, &cmd_bar, &dir_bar, &tld_iterm_command_history, &file_list)) {
            should_exit = tld_iterm_handle_command(app, &view, buffer_id, cmd_bar.string, &dir_bar.prompt, &file_list);
        } else {
            should_exit = true;
        }
        cmd_bar.string.size = 0;
    } while (!should_exit);
    
    tld_iterm_working_directory = dir_bar.prompt;
}

// Query a single command and execute it.
CUSTOM_COMMAND_SIG(tld_terminal_single_command) {
    View_Summary view;
    Buffer_Identifier buffer_id;
    Query_Bar cmd_bar, dir_bar;
    File_List file_list;
    
    char cmd_space[1024];
    String cmd = make_fixed_width_string(cmd_space);
    
    tld_iterm_init_session(app, &buffer_id, &view, &cmd_bar, cmd, &dir_bar, &file_list);
    if (tld_iterm_query_user_command(app, buffer_id, &view, &cmd_bar, &dir_bar, &tld_iterm_command_history, &file_list)) {
        tld_iterm_handle_command(app, &view, buffer_id, cmd_bar.string, &dir_bar.prompt, &file_list);
    }
    tld_iterm_working_directory = dir_bar.prompt;
}

#endif