/******************************************************************************
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
*******************************************************************************
LICENSE

This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.
*******************************************************************************
This file is the basic build target, and only defines commands that assume that
more than one of my command packs is being used, as well as my custom hooks
(all of which do the the same).

I did not spend any time making this file reusable at all; consider it example
code. If you do want my full configuration, use this in its entirety, or copy
out the individual commands you want.
******************************************************************************/

#include "4coder_default_include.cpp"

#include "4tld_user_interface.h"
#include "4tld_custom_commands.cpp"

#define TLD_SEARCH_COMMANDS
#define TLDFR_EXPERIMENT
#include "4tld_find_and_replace.cpp"

#include "4tld_interactive_terminal.cpp"
#include "4tld_file_manager.cpp"
#include "4tld_hex_viewer.cpp"
#include "4tld_project_management.cpp"

#include "4tld_vcs_git.cpp"

#include "4tld_modal_bindings.cpp"

TLDIT_BUILTIN_COMMAND_SIG(tld_terminal_cmd_goto_files) {
    exec_command(app, tld_files_show_hot_dir);
    return true;
}

CUSTOM_COMMAND_SIG(tld_files_begin_iterm_session) {
    exec_command(app, tld_files_close_buffer);
    exec_command(app, tld_iterm_begin_session);
}

CUSTOM_COMMAND_SIG(tld_project_show_files) {
    View_Summary view = get_active_view(app, AccessAll);
    
    String hot_dir;
    String file_name = {0};
    if (tld_current_project.project_file.str) {
        hot_dir = path_of_directory(tld_current_project.project_file);
        file_name = front_of_directory(tld_current_project.project_file);
    } else {
        Buffer_Summary file_buffer = get_buffer(app, view.buffer_id, AccessAll);
        if (file_buffer.file_name) {
            String file_name_ = make_string(file_buffer.file_name, file_buffer.file_name_len);
            
            hot_dir = path_of_directory(file_name_);
            file_name = front_of_directory(file_name_);
        } else {
            char hot_dir_space[1024];
            hot_dir = make_fixed_width_string(hot_dir_space);
            hot_dir.size = directory_get_hot(app, hot_dir.str, hot_dir.memory_size);
        }
    }
    
    directory_set_hot(app, expand_str(hot_dir));
    view_set_setting(app, &view, ViewSetting_ShowFileBar, false);
    
    Buffer_Summary buffer = get_buffer_by_name(app, literal("*files*"), AccessAll);
    if (!buffer.exists) {
        buffer = create_buffer(app, literal("*files*"), BufferCreate_AlwaysNew);
        buffer_set_setting(app, &buffer, BufferSetting_Unimportant, 1);
        buffer_set_setting(app, &buffer, BufferSetting_MapID, tld_files_buffer_mapid);
    }
    
    view_set_buffer(app, &view, buffer.buffer_id, 0);
    
    tld_files_state = tld_print_directory(app, &buffer, hot_dir, file_name);
    view_set_highlight(app, &view, tld_files_state.cells[tld_files_state.selected_index].min,
                       tld_files_state.cells[tld_files_state.selected_index].max, true);
}

CUSTOM_COMMAND_SIG(tld_files_hex_view_selected) {
    if (tld_files_state.cells == 0) return;
    if (tld_files_state.selected_index < tld_files_state.directory_count) return;
    
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    char hot_dir_space[1024];
    String hot_dir make_fixed_width_string(hot_dir_space);
    hot_dir.size = directory_get_hot(app, hot_dir.str, hot_dir.memory_size);
    
    Range r = tld_files_state.cells[tld_files_state.selected_index];
    if (hot_dir.memory_size - hot_dir.size > r.max - r.min) {
        if (buffer_read_range(app, &buffer, r.min, r.max, hot_dir.str + hot_dir.size)) {
            hot_dir.size += r.max - r.min;
            Buffer_Summary src_buffer = create_buffer(app, expand_str(hot_dir), 0);
            if (src_buffer.exists) {
                free(tld_files_state.cells);
                tld_files_state = {0};
                
                kill_buffer(app, buffer_identifier(buffer.buffer_id),
                            view.view_id, BufferKill_AlwaysKill);
                
                view_set_setting(app, &view, ViewSetting_ShowFileBar, 1);
                view_set_highlight(app, &view, 0, 0, 0);
                
                tld_HexViewState *state = tld_hex_view_state_make(app, 0, src_buffer.buffer_id);
                if (state) {
                    int32_t new_cursor_pos = 0;
                    int32_t new_mark_pos = 0;
                    tld_hex_view_print(app, state, true, &new_cursor_pos, &new_mark_pos);
                    
                    view_set_buffer(app, &view, state->hex_buffer_id, 0);
                    view_set_cursor(app, &view, seek_pos(new_cursor_pos), 0);
                    view_set_mark(app, &view, seek_pos(new_mark_pos));
                } else {
                    view_set_buffer(app, &view, src_buffer.buffer_id, 0);
                }
            }
        }
    }
}

enum tld_Keymaps {
    // TODO: Because of the bug in buffer_set_setting, we can't use just any values for our mapids. Simplify this when upgrading to a 4coder build that fixes this.
    keymap_modal = 0,
    // keymap_hex_view = 0x01000001,
    keymap_file_manager_defaults = 2,
    keymap_file_manager = 0x01000001,
    
    keymap_global = mapid_global,
};

START_HOOK_SIG(tld_start_hook) {
    load_themes_folder(app);
    
    change_font(app, literal("Inconsolata"), true);
    exec_command(app, tld_enter_cmd_mode); // NOTE(Tristan) This will also set the theme for us
    
    View_Summary view = get_active_view(app, AccessAll);
    new_view_settings(app, &view);
    
    if (file_count == 0) {
        exec_command(app, tld_files_show_hot_dir);
    } else {
        Buffer_Identifier left = buffer_identifier(expand_str(front_of_directory(make_string_slowly(files[0]))));
        Buffer_ID left_id = buffer_identifier_to_id(app , left);
        view_set_buffer(app, &view, left_id, 0);
        
        if (file_count > 1) {
            open_panel_vsplit(app);
            View_Summary right_view = get_active_view(app, AccessAll);
            
            Buffer_Identifier right = buffer_identifier(expand_str(front_of_directory(make_string_slowly(files[1]))));
            Buffer_ID right_id = buffer_identifier_to_id(app, right);
            view_set_buffer(app, &right_view, right_id, 0);
            
            set_active_view(app, &view);
        }
    }
    
    tld_push_default_command_names();
    
    tld_push_named_command(tld_find_and_replace_selection,
                           literal("Find and Replace: selection"));
    tld_push_named_command(tld_find_and_replace_in_range,
                           literal("Find and Replace: in range"));
    tld_push_named_command(tld_hex_view_current_buffer,
                           literal("Hex Viewer: show current buffer"));
    tld_push_named_command(tld_iterm_begin_session,
                           literal("Terminal: begin session"));
    tld_push_named_command(tld_files_show_hot_dir,
                           literal("File Manager: show hot directory"));
    
    tld_push_named_command(tld_project_show_files,
                           literal("Project: show files"));
    tld_push_named_command(tld_project_open_all_code,
                           literal("Project: open all code"));
    tld_push_named_command(tld_reload_project,
                           literal("Project: reload"));
    tld_push_named_command(tld_build_project,
                           literal("Project: build"));
    tld_push_named_command(tld_test_project,
                           literal("Project: test"));
    tld_push_named_command(tld_change_build_config,
                           literal("Project: change build config"));
    tld_push_named_command(tld_change_test_config,
                           literal("Project: change test config"));
    
    tldit_add_builtin_command(literal("cd"), tldit_builtin_cd);
    tldit_add_builtin_command(literal("quit"), tldit_builtin_quit);
    tldit_add_builtin_command(literal("files"), tld_terminal_cmd_goto_files);
    
    return 0;
}

OPEN_FILE_HOOK_SIG(tld_open_file_hook) {
    if (!global_part.base) {
        init_memory(app);
    }
    
    Buffer_Summary buffer = get_buffer(app, buffer_id, AccessAll);
    
    bool32 treat_as_code = false;
    Parse_Context_ID parse_context_id = 0;
    
    bool32 wrap_lines = true;
    
    if (buffer.file_name != 0 && buffer.size < (16 << 20)) {
        String name = make_string(buffer.file_name, buffer.file_name_len);
        String ext = file_extension(name);
        
        if (match(ext, "cs")) {
            if (parse_context_language_cs == 0) {
                init_language_cs(app);
            }
            
            treat_as_code = true;
            parse_context_id = parse_context_language_cs;
        } else if (match(ext, "java")) {
            if (parse_context_language_java == 0) {
                init_language_java(app);
            }
            
            treat_as_code = true;
            parse_context_id = parse_context_language_java;
        } else if (match(ext, "rs")) {
            if (parse_context_language_rust == 0) {
                init_language_rust(app);
            }
            
            treat_as_code = true;
            parse_context_id = parse_context_language_rust;
        } else if (match(ext, "4proj") || match(ext, "cpp") || match(ext, "h") ||
                   match(ext, "c") || match(ext, "hpp"))
        {
            if (match(ext, "4proj")) {
                tld_project_read_from_file(app, &tld_current_project,
                                           buffer.file_name,
                                           buffer.file_name_len);
                exec_command(app, tld_project_open_all_code);
            }
            
            if (parse_context_language_cpp == 0) {
                init_language_cpp(app);
            }
            
            treat_as_code = true;
            parse_context_id = parse_context_language_cpp;
        }
    }
    
    if (treat_as_code || buffer.file_name == 0) {
        wrap_lines = false;
    }
    
    buffer_set_setting(app, &buffer, BufferSetting_WrapPosition, default_wrap_width);
    buffer_set_setting(app, &buffer, BufferSetting_MinimumBaseWrapPosition, default_min_base_width);
    buffer_set_setting(app, &buffer, BufferSetting_MapID, keymap_modal);
    buffer_set_setting(app, &buffer, BufferSetting_ParserContext, parse_context_id);
    
    buffer_set_setting(app, &buffer, BufferSetting_WrapLine, wrap_lines);
    buffer_set_setting(app, &buffer, BufferSetting_Lex, treat_as_code);
    
    return 0;
}

OPEN_FILE_HOOK_SIG(tld_new_file_hook) {
    if (!global_part.base) {
        // NOTE: This hook runs before the START hook,
        // so we may need to initialize the global memory allocators
        // to load projects properly
        init_memory(app);
    }
    
    Buffer_Summary buffer = get_buffer(app, buffer_id, AccessOpen);
    
    if (buffer.file_name != 0 && buffer.size < (16 << 20)) {
        String name = make_string(buffer.file_name, buffer.file_name_len);
        String ext = file_extension(name);
        
        if (match(ext, "cpp") || match(ext, "h") || match(ext, "c") || match(ext, "hpp")) {
            char str[] = "/***************************************\n"
                "Author: Tristan Dannenberg\n"
                "Notice: No warranty is offered or implied; use this code at your own risk.\n"
                "***************************************/\n\n";
            buffer_replace_range(app, &buffer, 0, 0, str, sizeof(str) - 1);
        }
    }
    return 0;
}

OPEN_FILE_HOOK_SIG(tld_save_file_hook) {
    Buffer_Summary buffer = get_buffer(app, buffer_id, AccessOpen);
    
    if (parse_context_language_cpp != 0) {
        Parse_Context_ID parse_context_id = 0;
        buffer_get_setting(app, &buffer, BufferSetting_ParserContext, (int32_t *)&parse_context_id);
        
        if (parse_context_id == parse_context_language_cpp) {
            auto_tab_whole_file_by_summary(app, &buffer);
        }
    }
    
    return 0;
}

void tld_bind_keys(Bind_Helper *context) {
    begin_map(context, mapid_global);
    // TODO: Figure out what we are going to use the global map for
    end_map(context);
    
    tld_bind_modal_map(context, keymap_modal,
                       make_string(literal("stb dark")),
                       make_string(literal("stb dark cmd")));
    // tld_hex_view_bind_map(context, keymap_hex_view);
    tld_files_bind_map(context, keymap_file_manager_defaults);
    tld_files_buffer_mapid = keymap_file_manager;
    
    begin_map(context, keymap_file_manager);
    inherit_map(context, keymap_file_manager_defaults);
    
    bind(context, ' ', MDFR_NONE, tld_files_begin_iterm_session);
    bind(context, 'h', MDFR_NONE, tld_files_hex_view_selected);
    
    end_map(context);
}

extern "C" int32_t get_bindings(void *data, int32_t size) {
    // Do this here, rather than in START_HOOK, because that runs after the
    // files are opened - too late, since our OPEN_FILE_HOOK needs this to be
    // initialized.
    tld_project_register_version_control_system(&tld_version_control_system_git);
    
    Bind_Helper context_local = begin_bind_helper(data, size);
    Bind_Helper *context = &context_local;
    
    tld_bind_keys(context);
    
    set_start_hook(context, tld_start_hook);
    set_open_file_hook(context, tld_open_file_hook);
    set_new_file_hook(context, tld_new_file_hook);
    set_save_file_hook(context, tld_save_file_hook);
    
    // 4coder default hooks:
    set_hook(context, hook_exit, default_exit);
    set_hook(context, hook_view_size_change, default_view_adjust);
    set_command_caller(context, default_command_caller);
    set_input_filter(context, default_suppress_mouse_filter);
    set_scroll_rule(context, smooth_scroll_rule);
    
    int32_t result = end_bind_helper(context);
    return result;
}