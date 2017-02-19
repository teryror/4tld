/******************************************************************************
File: 4tld_interactive_terminal.cpp
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
******************************************************************************/
#include "4tld_user_interface.h"

#ifndef TLD_HOME_DIRECTORY
#define TLD_HOME_DIRECTORY
static void
tld_get_home_directory(Application_Links *app, String *dest) {
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

#define tld_iterm_switch_buffer_command tld_switch_buffer_fuzzy
#define tld_iterm_open_file_interactive(app, view, work_dir) __tld_open_file_fuzzy_impl(app, view, work_dir)

#else

#define tld_iterm_switch_buffer_command cmdid_interactive_switch_buffer
#define tld_iterm_open_file_interactive(...) exec_command(app, cmdid_interactive_open)

#endif

static bool
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
                
                tld_iterm_open_file_interactive(app, view, dir_bar->prompt);
                return false;
            } else if (in.key.keycode == 'q' && in.key.modifiers[MDFR_ALT]) {
                exec_command(app, kill_buffer);
                return false;
            } else if (in.key.keycode == 'p' && in.key.modifiers[MDFR_ALT]) {
                end_query_bar(app, cmd_bar, 0);
                end_query_bar(app, dir_bar, 0);
                
                exec_command(app, tld_iterm_switch_buffer_command);
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