/******************************************************************************
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
*******************************************************************************
LICENSE

This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.
******************************************************************************/

// A ring buffer of strings
struct tld_StringHistory {
    String *strings;
    int32_t size;
    int32_t offset;
    int32_t capacity;
};

// Push a new string into the history and delete the oldest one if necessary.
// 
// NOTE: This does _all_ the memory management for the strings.
// Do not copy string pointers out of the history unlsess you are certain
// nothing will be pushed during the copy's lifetime.
static void
tld_string_history_push(tld_StringHistory *history, String value) {
    int32_t offset = (history->size + history->offset) % history->capacity;
    
    if (history->strings[offset].str) {
        free(history->strings[offset].str);
    }
    
    history->strings[offset] = {0};
    history->strings[offset].str = (char *) malloc(value.size);
    history->strings[offset].memory_size = value.size;
    copy_partial_ss(&history->strings[offset], value);
    
    if (history->size < history->capacity) {
        ++history->size;
    } else {
        history->offset += 1;
        history->offset %= history->capacity;
    }
}

#ifndef TLDF_COMMAND_HISTORY_CAPACITY
#define TLDF_COMMAND_HISTORY_CAPACITY 128
#endif

static String tld_file_term_command_history_space[TLDF_COMMAND_HISTORY_CAPACITY];
static tld_StringHistory tld_file_term_command_history = {0};

static void
tld_file_query_traverse_history(User_Input in,
                                uint32_t keycode_prev,
                                uint32_t keycode_next,
                                String *bar_string,
                                tld_StringHistory *history,
                                int32_t *history_index)
{
    if (in.abort) return;
    if (history->size <= 0) return;
    
    if (in.type == UserInputKey) {
        if (in.key.keycode == keycode_prev) {
            int32_t _index = (*history_index + history->offset) % history->capacity;
            bar_string->size = 0;
            append_partial_ss(bar_string, history->strings[_index]);
            
            if (*history_index > 0) {
                *history_index -= 1;
            }
        } else if (in.key.keycode == keycode_next) {
            bar_string->size = 0;
            if (*history_index + 1 < history->size) {
                *history_index += 1;
                
                int32_t _index = (*history_index + history->offset) % history->capacity;
                append_partial_ss(bar_string, history->strings[_index]);
            }
        }
    }
}

static void
tld_file_query_complete_filename(Application_Links *app,
                                 User_Input *in,
                                 uint32_t keycode,
                                 String *bar_string,
                                 String working_directory)
{
    if (in->abort) return;
    
    if (in->type == UserInputKey && in->key.keycode == keycode) {
        Range incomplete_string = {0};
        incomplete_string.max = bar_string->size;
        incomplete_string.min = incomplete_string.max - 1;
        
        bool other_directory = false;
        
        while (incomplete_string.min >= 0 &&
               bar_string->str[incomplete_string.min] != ' ')
        {
            if (bar_string->str[incomplete_string.min] == '/' ||
                bar_string->str[incomplete_string.min] == '\\') {
                other_directory = true;
            }
            --incomplete_string.min;
        }
        ++incomplete_string.min;
        
        String prefix = *bar_string;
        prefix.str += incomplete_string.min;
        prefix.size = incomplete_string.max - incomplete_string.min;
        prefix.memory_size = bar_string->memory_size - prefix.size;
        
        if (other_directory) {
            // NOTE: directory_cd is still broken in this 4coder build...
            directory_cd(app, working_directory.str, &working_directory.size,
                         working_directory.memory_size, expand_str(path_of_directory(prefix)));
            prefix = front_of_directory(prefix);
        }
        
        File_List files = get_file_list(app, expand_str(working_directory));
        
        int32_t file_index = -1;
        while (in->type == UserInputKey && in->key.keycode == keycode) {
            if (in->key.modifiers[MDFR_SHIFT]) {
                if (file_index < 0) {
                    file_index = files.count;
                }
                
                for (--file_index; file_index >= 0; --file_index) {
                    if (match_part_insensitive_cs(files.infos[file_index].filename, prefix)) {
                        break;
                    }
                }
            } else {
                for (++file_index; file_index < (int32_t)files.count; ++file_index) {
                    if (match_part_insensitive_cs(files.infos[file_index].filename, prefix)) {
                        break;
                    }
                }
            }
            
            bar_string->size = incomplete_string.max;
            if (file_index >= 0 && file_index < (int32_t)files.count) {
                append_ss(bar_string,
                          make_string(files.infos[file_index].filename + prefix.size,
                                      files.infos[file_index].filename_len - prefix.size));
            } else if (file_index >= (int32_t)files.count) {
                file_index = -1;
            } else if (file_index < 0) {
                file_index = files.count;
            } else {
                Assert(false); // NOTE: Unreachable
            }
            
            *in = get_user_input(app, EventOnAnyKey, EventOnEsc);
        }
        
        free_file_list(app, files);
    }
}

static bool
tld_file_query_term_command(Application_Links *app, View_Summary *view, String working_directory, String *command) {
    char cmd_space[1024];
    bool result;
    
    if (tld_file_term_command_history.strings == 0) {
        tld_file_term_command_history.strings = tld_file_term_command_history_space;
        tld_file_term_command_history.capacity = ArrayCount(tld_file_term_command_history_space);
    }
    
    Query_Bar cmd_bar = {0};
    cmd_bar.prompt = make_lit_string("> ");
    cmd_bar.string = make_fixed_width_string(cmd_space);
    if (command->size) {
        append_ss(&cmd_bar.string, *command);
    }
    
    Query_Bar dir_bar = {0};
    dir_bar.prompt = working_directory;
    
    start_query_bar(app, &cmd_bar, 0);
    start_query_bar(app, &dir_bar, 0);
    
    int32_t cmd_history_index = tld_file_term_command_history.size - 1;
    while (true) {
        User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
        if (in.type != UserInputKey) continue;
        
        tld_file_query_complete_filename(app, &in, '\t', &cmd_bar.string, dir_bar.prompt);
        tld_file_query_traverse_history(in, key_up, key_down,
                                        &cmd_bar.string,
                                        &tld_file_term_command_history,
                                        &cmd_history_index);
        if (in.abort) { result = false; break; }
        
        if (in.key.keycode == '\n') {
            tld_string_history_push(&tld_file_term_command_history, cmd_bar.string);
            cmd_history_index = tld_file_term_command_history.size - 1;
            
            result = true;
            break;
        } else tld_handle_query_bar_input(app, &cmd_bar, in);
    }
    
    end_query_bar(app, &cmd_bar, 0);
    end_query_bar(app, &dir_bar, 0);
    
    if (result) {
        command->size = 0;
        append_ss(command, cmd_bar.string);
    }
    
    return result;
}

#define TLDIT_BUILTIN_COMMAND_SIG(name) \
static bool name(Application_Links *app, String args)

TLDIT_BUILTIN_COMMAND_SIG(tldit_builtin_cd) {
    char hot_dir_space[1024];
    String hot_dir = make_fixed_width_string(hot_dir_space);
    hot_dir.size = directory_get_hot(app, hot_dir.str, hot_dir.memory_size);
    
    // TODO: Replace this with directory_cd(...) once that is fixed
    if (match_sc(args, ".")) {
        return false;
    } else if (match_sc(args, "..")) {
        directory_cd(app, hot_dir.str, &hot_dir.size, hot_dir.memory_size, literal(".."));
    } else {
        append_ss(&hot_dir, args);
        append_sc(&hot_dir, "/");
    }
    
    directory_set_hot(app, expand_str(hot_dir));
    return false;
}

TLDIT_BUILTIN_COMMAND_SIG(tldit_builtin_quit) {
    exec_command(app, exit_4coder);
    return true;
}

typedef bool (*tldit_builtin_command_function)(Application_Links *app, String args);

struct tldit_builtin_command {
    String name;
    tldit_builtin_command_function impl;
};

static bool
tld_file_handle_term_command(Application_Links *app,
                             View_Summary *view,
                             Buffer_ID buffer_id,
                             String command,
                             String *working_directory,
                             tldit_builtin_command *builtins,
                             int32_t builtins_count)
{
    String original_command = command;
    
    char *cmd_end = command.str + command.size;
    while (command.str < cmd_end && *command.str == ' ') {
        ++command.str;
    }
    
    String ident = {0};
    ident.str = command.str;
    while (command.str < cmd_end && *command.str != ' ') {
        ++command.str;
    }
    ident.size = (int)(command.str - ident.str);
    
    while (command.str < cmd_end && *command.str == ' ') {
        ++command.str;
    }
    
    tldit_builtin_command_function cmd_impl = 0;
    
    for (int i = 0; i < builtins_count; ++i) {
        if (match_ss(ident, builtins[i].name)) {
            cmd_impl = builtins[i].impl;
            break;
        }
    }
    
    if (cmd_impl) {
        return cmd_impl(app, make_string(command.str, (int32_t)(cmd_end - command.str)));
    } else {
        exec_system_command(app, view, buffer_identifier(buffer_id),
                            working_directory->str,
                            working_directory->size,
                            expand_str(original_command),
                            CLI_OverlapWithConflict | CLI_CursorAtEnd);
        Buffer_Summary buffer = get_buffer(app, buffer_id, AccessAll);
        buffer_replace_range(app, &buffer, 0, 0, literal("\n\n"));
    }
    
    return false;
}

#ifndef TLDIT_BUILTIN_COMMAND_CAP
#define TLDIT_BUILTIN_COMMAND_CAP 16
#endif

static tldit_builtin_command tldit_builtin_commands[TLDIT_BUILTIN_COMMAND_CAP];
static int32_t tldit_builtin_commands_count = 0;

static void
tldit_add_builtin_command(char *cmd_name, int32_t cmd_name_len,
                          tldit_builtin_command_function cmd_impl)
{
    Assert(tldit_builtin_commands_count < TLDIT_BUILTIN_COMMAND_CAP);
    
    tldit_builtin_commands[tldit_builtin_commands_count].name =
        make_string(cmd_name, cmd_name_len);
    tldit_builtin_commands[tldit_builtin_commands_count].impl = cmd_impl;
    
    tldit_builtin_commands_count += 1;
}

CUSTOM_COMMAND_SIG(tld_iterm_begin_session) {
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_ID old_buffer_id = view.buffer_id;
    
    Buffer_Summary term_buffer = get_buffer_by_name(
        app, literal("*iterm*"), AccessAll);
    if (!term_buffer.exists) {
        term_buffer = create_buffer(
            app, literal("*iterm*"), BufferCreate_AlwaysNew);
        buffer_set_setting(app, &term_buffer, BufferSetting_ReadOnly, 1);
    }
    view_set_buffer(app, &view, term_buffer.buffer_id, 0);
    
    char hot_dir_space[1024];
    String hot_dir;
    
    bool done = false;
    do {
        char _command_space[1024];
        String command = make_fixed_width_string(_command_space);
        
        hot_dir = make_fixed_width_string(hot_dir_space);
        hot_dir.size = directory_get_hot(app, hot_dir.str, hot_dir.memory_size);
        
        if (tld_file_query_term_command(app, &view, hot_dir, &command)) {
            done = tld_file_handle_term_command(app, &view,
                                                term_buffer.buffer_id,
                                                command, &hot_dir,
                                                tldit_builtin_commands,
                                                tldit_builtin_commands_count);
        } else {
            break;
        }
    } while (!done);
    
    if (!done) {
        view_set_buffer(app, &view, old_buffer_id, 0);
    }
}