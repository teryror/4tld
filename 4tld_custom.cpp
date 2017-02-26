/******************************************************************************
File: 4tld_custom.cpp
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
******************************************************************************/

#include "4coder_default_include.cpp"
#include "4tld_user_interface.h"

#define TLDFR_IMPLEMENT_COMMMANDS
#include "4tld_find_and_replace.cpp"

#define TLDPM_IMPLEMENT_COMMANDS
#include "4tld_project_management.cpp"

#define TLDFM_IMPLEMENT_COMMANDS
#include "4tld_fuzzy_match.cpp"

#define TLDIT_IMPLEMENT_COMMANDS
#include "4tld_interactive_terminal.cpp"

#define TLD_COLORS_WHITE     0xE0E0E0
#define TLD_COLORS_GRAY      0x909090
#define TLD_COLORS_DARK_GRAY 0x303030
#define TLD_COLORS_BLACK     0x181818

#define TLD_COLORS_YELLOW 0xE7C547
#define TLD_COLORS_BLUE   0x71DDFF
#define TLD_COLORS_GREEN  0xC0FFDD
#define TLD_COLORS_RED    0xCD0000

#define TLD_THEME_MODE_EDIT      TLD_COLORS_YELLOW
#define TLD_THEME_MODE_EDIT_MARK TLD_COLORS_GREEN
#define TLD_THEME_MODE_CMD       TLD_COLORS_RED

static bool edit_mode;
CUSTOM_COMMAND_SIG(enter_edit_mode) {
    edit_mode = true;
    
    Theme_Color colors[] = {
        {Stag_Cursor,        TLD_THEME_MODE_EDIT_MARK},
        {Stag_Bar,           TLD_THEME_MODE_EDIT},
        {Stag_Margin_Active, TLD_THEME_MODE_EDIT},
        {Stag_Mark,          TLD_THEME_MODE_EDIT_MARK}
    };
    
    int count = ArrayCount(colors);
    set_theme_colors(app, colors, count);
}

CUSTOM_COMMAND_SIG(smart_save) {
    View_Summary view = get_active_view(app, AccessProtected);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessProtected);
    
    if (buffer.exists) {
        String ext = file_extension(make_string(buffer.file_name, buffer.file_name_len));
        if (match_sc(ext, "4proj")) {
            exec_command(app, cmdid_save);
            tld_current_project =
                tld_project_reload_from_buffer(app, buffer.buffer_id,
                                               &tld_current_project_memory);
            return;
        }
        
        if (buffer.is_lexed) {
            exec_command(app, auto_tab_whole_file);
        }
        
        if (buffer.buffer_name[0] == '*') {
            exec_command(app, save_as);
        } else {
            exec_command(app, cmdid_save);
        }
    }
}

CUSTOM_COMMAND_SIG(seek_back_to_indent) {
    exec_command(app, seek_beginning_of_line);
    
    View_Summary view = get_active_view(app, AccessProtected);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessProtected);
    
    move_past_lead_whitespace(app, &view, &buffer);
}

CUSTOM_COMMAND_SIG(toggle_or_create_panel) {
    int view_count = 0;
    
    for (View_Summary view = get_view_first(app, AccessAll);
         view.exists; get_view_next(app, &view, AccessAll)) {
        ++view_count;
    }
    
    if (view_count <= 1) {
        exec_command(app, open_panel_vsplit);
        exec_command(app, hide_scrollbar);
    } else {
        exec_command(app, change_active_panel);
    }
}

CUSTOM_COMMAND_SIG(maximize_panel) {
    exec_command(app, change_active_panel);
    exec_command(app, close_panel);
}

CUSTOM_COMMAND_SIG(write_individual_character) {
    User_Input in = get_user_input(app, EventOnAnyKey, EventOnButton);
    if (in.abort || in.key.keycode == key_esc || in.key.keycode == 0) {
        return;
    }
    
    write_string(app, make_string(&in.key.keycode, 1));
}

CUSTOM_COMMAND_SIG(replace_character) {
    User_Input in = get_user_input(app, EventOnAnyKey, EventOnButton);
    if (in.abort || in.key.keycode == key_esc || in.key.keycode == 0) {
        return;
    }
    
    exec_command(app, delete_char);
    write_string(app, make_string(&in.key.keycode, 1));
}

CUSTOM_COMMAND_SIG(replace_range) {
    exec_command(app, delete_range);
    exec_command(app, enter_edit_mode);
}

CUSTOM_COMMAND_SIG(open_line) {
    exec_command(app, seek_end_of_line);
    write_string(app, make_lit_string("\n"));
    exec_command(app, auto_tab_line_at_cursor);
}

CUSTOM_COMMAND_SIG(move_line_down) {
    View_Summary view = get_active_view(app, AccessProtected);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessProtected);
    
    int start = seek_line_beginning(app, &buffer, view.cursor.pos);
    int end = seek_line_end(app, &buffer, view.cursor.pos);
    
    char line_text[1024];
    
    if (start < 0) {
        start = 0;
    }
    if (end < buffer.size) {
        end += 1;
    }
    
    if (end - start <= sizeof(line_text)) {
        buffer_read_range(app, &buffer, start, end, line_text);
        buffer_replace_range(app, &buffer, start, end, 0, 0);
        
        exec_command(app, move_down);
        write_string(app, make_string(line_text, end - start));
        exec_command(app, move_up);
    }
}

CUSTOM_COMMAND_SIG(open_line_upwards) {
    exec_command(app, seek_beginning_of_line);
    write_string(app, make_lit_string("\n"));
    exec_command(app, move_up);
    exec_command(app, auto_tab_line_at_cursor);
}

CUSTOM_COMMAND_SIG(move_line_up) {
    View_Summary view = get_active_view(app, AccessProtected);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessProtected);
    
    int start = seek_line_beginning(app, &buffer, view.cursor.pos);
    int end = seek_line_end(app, &buffer, view.cursor.pos);
    
    char line_text[1024];
    
    if (start < 0) {
        start = 0;
    }
    if (end < buffer.size) {
        end += 1;
    }
    
    if (end - start <= sizeof(line_text)) {
        buffer_read_range(app, &buffer, start, end, line_text);
        buffer_replace_range(app, &buffer, start, end, 0, 0);
        
        exec_command(app, move_up);
        write_string(app, make_string(line_text, end - start));
        exec_command(app, move_up);
    }
}

CUSTOM_COMMAND_SIG(seek_end_of_scope) {
    View_Summary view = get_active_view(app, AccessProtected);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessProtected);
    int pos = view.cursor.pos + 1;
    
    char chunk[1024];
    int chunk_size = sizeof(chunk);
    Stream_Chunk stream = {0};
    
    if (init_stream_chunk(&stream, app, &buffer, pos, chunk, chunk_size)) {
        int still_looping = 1;
        int level = 1;
        
        do {
            for (; pos < stream.end; ++pos) {
                if (stream.data[pos] == '}') {
                    --level;
                    if (level == 0) {
                        goto double_break;
                    }
                } else if (stream.data[pos] == '{') {
                    ++level;
                }
            }
            
            still_looping = forward_stream_chunk(&stream);
        } while (still_looping);
        
        double_break:;
        
        if (pos > buffer.size) {
            pos = buffer.size;
        }
    }
    
    view_set_cursor(app, &view, seek_pos(pos), true);
}

CUSTOM_COMMAND_SIG(seek_beginning_of_scope) {
    View_Summary view = get_active_view(app, AccessProtected);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessProtected);
    int pos = view.cursor.pos - 1;
    
    char chunk[1024];
    int chunk_size = sizeof(chunk);
    Stream_Chunk stream = {0};
    
    if (init_stream_chunk(&stream, app, &buffer, pos, chunk, chunk_size)) {
        int still_looping = 1;
        int level = 1;
        
        do {
            for (; pos >= stream.start; --pos) {
                if (stream.data[pos] == '{') {
                    --level;
                    if (level == 0) {
                        goto double_break;
                    }
                } else if (stream.data[pos] == '}') {
                    ++level;
                }
            }
            
            still_looping = backward_stream_chunk(&stream);
        } while (still_looping);
        
        double_break:;
        
        if (pos < 0) {
            pos = 0;
        }
    }
    
    view_set_cursor(app, &view, seek_pos(pos), true);
}

CUSTOM_COMMAND_SIG(seek_character_right) {
    User_Input in = get_user_input(app, EventOnAnyKey, EventOnButton);
    if (in.abort || in.key.keycode == key_esc || in.key.keycode == 0) {
        return;
    }
    
    View_Summary view = get_active_view(app, AccessProtected);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessProtected);
    int pos = view.cursor.pos + 1;
    
    char chunk[1024];
    int chunk_size = sizeof(chunk);
    Stream_Chunk stream = {0};
    
    if (init_stream_chunk(&stream, app, &buffer, pos, chunk, chunk_size)) {
        int still_looping = 1;
        do {
            for (; pos < stream.end; ++pos) {
                if (stream.data[pos] == in.key.keycode) {
                    goto double_break;
                }
            }
            still_looping = forward_stream_chunk(&stream);
        } while (still_looping);
        
        double_break:;
        
        if (pos > buffer.size) {
            pos = buffer.size;
        }
    }
    
    view_set_cursor(app, &view, seek_pos(pos), true);
}

CUSTOM_COMMAND_SIG(seek_character_left) {
    User_Input in = get_user_input(app, EventOnAnyKey, EventOnButton);
    if (in.abort || in.key.keycode == key_esc || in.key.keycode == 0) {
        return;
    }
    
    View_Summary view = get_active_view(app, AccessProtected);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessProtected);
    int pos = view.cursor.pos - 1;
    
    char chunk[1024];
    int chunk_size = sizeof(chunk);
    Stream_Chunk stream = {0};
    
    if (init_stream_chunk(&stream, app, &buffer, pos, chunk, chunk_size)) {
        int still_looping = 1;
        do {
            for (; pos >= stream.start; --pos) {
                if (stream.data[pos] == in.key.keycode) {
                    goto double_break;
                }
            }
            still_looping = backward_stream_chunk(&stream);
        } while (still_looping);
        
        double_break:;
        
        if (pos < 0) {
            pos = 0;
        }
    }
    
    view_set_cursor(app, &view, seek_pos(pos), true);
}

CUSTOM_COMMAND_SIG(write_multiple_characters) {
    Query_Bar hints;
    hints.prompt = make_lit_string("/: Block Comment -- t: TODO -- n: NOTE -- r: REGION");
    hints.string = make_lit_string("");
    start_query_bar(app, &hints, 0);
    
    Query_Bar hints2;
    hints2.prompt = make_lit_string("0: Zero Struct -- [: Scope -- a: Assert -- i: Include");
    hints2.string = make_lit_string("");
    start_query_bar(app, &hints2, 0);
    
    User_Input in = get_user_input(app, EventOnAnyKey, EventOnButton);
    if (in.abort || in.key.keycode == key_esc || in.key.keycode == 0) {
        return;
    }
    
    View_Summary view = get_active_view(app, AccessOpen);
    int pos = view.cursor.pos;
    
    switch (in.key.keycode) {
        case '/': {
            write_string(app, make_lit_string("/* \n */"));
            exec_command(app, auto_tab_line_at_cursor);
            view_set_cursor(app, &view, seek_pos(pos + 3), true);
        } break;
        case 't': {
            write_string(app, make_lit_string("// TODO: "));
        } break;
        case 'n': {
            write_string(app, make_lit_string("// NOTE: "));
        } break;
        case 'r': {
            write_string(app, make_lit_string("// REGION() "));
            view_set_cursor(app, &view, seek_pos(pos + 10), true);
        } break;
        case '0': {
            write_string(app, make_lit_string(" = {0};"));
        } break;
        case '[': {
            write_string(app, make_lit_string("{\n"));
            exec_command(app, auto_tab_line_at_cursor);
            
            view = get_active_view(app, AccessOpen);
            pos = view.cursor.pos;
            
            write_string(app, make_lit_string("\n}"));
            exec_command(app, auto_tab_line_at_cursor);
            view_set_cursor(app, &view, seek_pos(pos), true);
        } break;
        case 'a': {
            write_string(app, make_lit_string("Assert();"));
            view_set_cursor(app, &view, seek_pos(pos + 7), true);
        } break;
        case 'i': {
            write_string(app, make_lit_string("#include \"\""));
            view_set_cursor(app, &view, seek_pos(pos + 10), true);
        } break;
    }
    
    exec_command(app, enter_edit_mode);
}

CUSTOM_COMMAND_SIG(write_or_goto_position){
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    if (buffer.lock_flags & AccessProtected) {
        exec_command(app, goto_jump_at_cursor);
        lock_jump_buffer(buffer);
    }
    else {
        exec_command(app, write_and_auto_tab);
    }
}

/*

// NOTE: Hackish workaround for the lack of the new_file_hook
// TODO: Is this fixed in 4-0-16?
static bool opened_file_is_new = false;
CUSTOM_COMMAND_SIG(new_file) {
    opened_file_is_new = true;
    exec_command(app, cmdid_interactive_new);
}
CUSTOM_COMMAND_SIG(open_file) {
    opened_file_is_new = false;
    exec_command(app, cmdid_interactive_open);
}

*/

// TODO: This is pretty nice, but there are some minor improvements we could make:
// * Locate a corresponding file without allocating memory
// * Look for different build files depending on the platform
// * Provide multiple file names to the recursive search,
//   so that the README and TODO options become more powerful.
CUSTOM_COMMAND_SIG(interactive_find_file) {
    Query_Bar hints;
    hints.prompt = make_lit_string("b: Build File -- c: Corresponding Code File -- r: README -- t: TODO -- Space: Other");
    hints.string = make_lit_string("");
    start_query_bar(app, &hints, 0);
    
    User_Input in = get_user_input(app, EventOnAnyKey, EventOnButton);
    if (in.abort || in.key.keycode == key_esc || in.key.keycode == 0) {
        return;
    }
    
    String file_name_to_find = {0};
    
    switch (in.key.keycode) {
        case 'b': {
            file_name_to_find = make_lit_string("build.bat");
        } break;
        case 'c': {
            View_Summary view = get_active_view(app, AccessAll);
            Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
            
            Buffer_Summary new_buffer = {0};
            if (get_cpp_matching_file(app, buffer, &new_buffer)) {
                view_set_buffer(app, &view, new_buffer.buffer_id, 0);
            } else {
                hints.prompt = make_lit_string("Error: File not found!");
                in = get_user_input(app, EventOnAnyKey, EventOnButton);
            }
            
            return;
        } break;
        case 'r': {
            file_name_to_find = make_lit_string("README.md");
        } break;
        case 't': {
            file_name_to_find = make_lit_string("TODO.md");
        } break;
        case ' ': {
            Query_Bar file_name;
            char space[1024];
            
            file_name.prompt = make_lit_string("File Name: ");
            file_name.string = make_fixed_width_string(space);
            
            if (!query_user_string(app, &file_name)) return;
            
            file_name_to_find = file_name.string;
            end_query_bar(app, &file_name, 0);
        } break;
    }
    
    if (file_name_to_find.str) {
        View_Summary view = get_active_view(app, AccessAll);
        
        int keep_going = 1;
        int old_size;
        // TODO: We probably shouldn't need this kind of app->memory fuckery either
        String dir = make_string(app->memory, 0, app->memory_size);
        dir.size = directory_get_hot(app, dir.str, dir.memory_size);
        
        while (keep_going) {
            old_size = dir.size;
            append(&dir, file_name_to_find);
            
            if (file_exists(app, dir.str, dir.size)) {
                view_open_file(app, &view, expand_str(dir), false);
                return;
            }
            
            dir.size = old_size;
            if (directory_cd(app, dir.str, &dir.size, dir.memory_size,
                             literal("..")) == 0) {
                keep_going = 0;
            }
        }
        
        tld_show_error("File not found!");
    }
}

CUSTOM_COMMAND_SIG(git_quick_save) {
    String working_directory;
    
    if (tld_current_project.working_directory.str) {
        working_directory = tld_current_project.working_directory;
    } else {
        working_directory = make_fixed_width_string(hot_directory_space);
        working_directory.size = directory_get_hot(app, working_directory.str, working_directory.memory_size);
    }
    
    Query_Bar hot_dir_hint;
    hot_dir_hint.prompt = working_directory;
    hot_dir_hint.string = make_lit_string("");
    
    char git_command_buffer[68];
    String git_command = make_fixed_width_string(git_command_buffer);
    append_sc(&git_command, "git commit -am ");
    git_command_buffer[15] = '"';
    
    Query_Bar message_bar;
    message_bar.prompt = git_command;
    message_bar.string.str = &git_command_buffer[16];
    message_bar.string.size = 0;
    message_bar.string.memory_size = sizeof(git_command_buffer) - 18;
    
    start_query_bar(app, &message_bar, 0);
    start_query_bar(app, &hot_dir_hint, 0);
    if (tld_requery_user_string(app, &message_bar)) {
        int size = message_bar.prompt.size + 1 + message_bar.string.size;
        git_command_buffer[size] = '"';
        git_command_buffer[size + 1] = 0;
        
        View_Summary view;
        Buffer_Summary buffer;
        tld_display_buffer_by_name(app, make_lit_string("*terminal*"), &buffer, &view, false, AccessAll);
        
        Buffer_Identifier buffer_id = {0};
        buffer_id.id = buffer.buffer_id;
        
        exec_system_command(app, &view, buffer_id, working_directory.str, working_directory.size, git_command.str, size + 1, CLI_OverlapWithConflict | CLI_CursorAtEnd);
    }
}

// NOTE: This is a stub that forces me to use the edit mode
CUSTOM_COMMAND_SIG(no_op) { }

CUSTOM_COMMAND_SIG(enter_command_mode) {
    edit_mode = false;
    
    Theme_Color colors[] = {
        {Stag_Cursor,        TLD_THEME_MODE_CMD},
        {Stag_Bar,           TLD_THEME_MODE_CMD},
        {Stag_Margin_Active, TLD_THEME_MODE_CMD},
        {Stag_Mark,          TLD_THEME_MODE_CMD},
    };
    int count = ArrayCount(colors);
    set_theme_colors(app, colors, count);
}

#define DefineBimodalKey(binding_name, edit_code, character_code) \
CUSTOM_COMMAND_SIG(binding_name) {                                \
    if (edit_mode) {                                              \
        exec_command(app, character_code);                       \
    } else {                                                      \
        exec_command(app, edit_code);                             \
    }                                                             \
}
#define DefineModalKey(binding_name, edit_code)                   \
DefineBimodalKey(binding_name, edit_code, write_character)

DefineModalKey(modal_space, enter_edit_mode);

DefineModalKey(modal_backtick, tld_terminal_single_command);
DefineModalKey(modal_1, no_op);
DefineModalKey(modal_2, no_op);
DefineModalKey(modal_3, no_op);
DefineModalKey(modal_4, no_op);
DefineModalKey(modal_5, no_op);
DefineModalKey(modal_6, no_op);
DefineModalKey(modal_7, no_op);
DefineModalKey(modal_8, no_op);
DefineModalKey(modal_9, no_op);
DefineModalKey(modal_0, no_op);
DefineModalKey(modal_dash, no_op);
DefineModalKey(modal_equals, no_op);

DefineModalKey(modal_tilde, tld_terminal_begin_interactive_session);
DefineModalKey(modal_exclamation_mark, no_op);
DefineModalKey(modal_at, no_op);
DefineModalKey(modal_pound, no_op);
DefineModalKey(modal_dollar, no_op);
DefineModalKey(modal_percent, no_op);
DefineModalKey(modal_caret, no_op);
DefineModalKey(modal_and, no_op);
DefineModalKey(modal_asterisk, no_op);
DefineModalKey(modal_open_paren, no_op);
DefineModalKey(modal_close_paren, no_op);
DefineModalKey(modal_lodash, no_op);
DefineModalKey(modal_plus, no_op);

DefineModalKey(modal_a, no_op);
DefineModalKey(modal_b, no_op);
DefineModalKey(modal_c, no_op);
DefineModalKey(modal_d, delete_char);
DefineModalKey(modal_e, no_op);
DefineModalKey(modal_f, seek_character_right);
DefineModalKey(modal_g, no_op);
DefineModalKey(modal_h, seek_back_to_indent);
DefineModalKey(modal_i, move_up);
DefineModalKey(modal_j, move_left);
DefineModalKey(modal_k, move_down);
DefineModalKey(modal_l, move_right);
DefineModalKey(modal_m, write_multiple_characters);
DefineModalKey(modal_n, write_individual_character);
DefineModalKey(modal_o, open_line);
DefineModalKey(modal_p, no_op);
DefineModalKey(modal_q, no_op);
DefineModalKey(modal_r, replace_character);
DefineModalKey(modal_s, no_op);
DefineModalKey(modal_t, no_op);
DefineModalKey(modal_u, open_line_upwards);
DefineModalKey(modal_v, no_op);
DefineModalKey(modal_w, no_op);
DefineModalKey(modal_x, no_op);
DefineModalKey(modal_y, no_op);
DefineModalKey(modal_z, no_op);

DefineModalKey(modal_A, no_op);
DefineModalKey(modal_B, no_op);
DefineModalKey(modal_C, no_op);
DefineModalKey(modal_D, delete_word);
DefineModalKey(modal_E, no_op);
DefineModalKey(modal_F, seek_character_left);
DefineModalKey(modal_G, no_op);
DefineModalKey(modal_H, no_op);
DefineModalKey(modal_I, seek_whitespace_up);
DefineModalKey(modal_J, seek_alphanumeric_left);
DefineModalKey(modal_K, seek_whitespace_down);
DefineModalKey(modal_L, seek_alphanumeric_right);
DefineModalKey(modal_M, no_op);
DefineModalKey(modal_N, no_op);
DefineModalKey(modal_O, move_line_down);
DefineModalKey(modal_P, no_op);
DefineModalKey(modal_Q, no_op);
DefineModalKey(modal_R, no_op);
DefineModalKey(modal_S, no_op);
DefineModalKey(modal_T, no_op);
DefineModalKey(modal_U, move_line_up);
DefineModalKey(modal_V, no_op);
DefineModalKey(modal_W, no_op);
DefineModalKey(modal_X, no_op);
DefineModalKey(modal_Y, no_op);
DefineModalKey(modal_Z, no_op);

DefineModalKey(modal_tab, toggle_or_create_panel);
DefineModalKey(modal_open_bracket, seek_beginning_of_scope);
DefineModalKey(modal_close_bracket, seek_end_of_scope);
DefineModalKey(modal_semicolon, seek_end_of_line);
DefineModalKey(modal_single_quote, no_op);
DefineModalKey(modal_backslash, no_op);
DefineModalKey(modal_comma, cursor_mark_swap);
DefineModalKey(modal_period, set_mark);
DefineModalKey(modal_slash, no_op);

DefineModalKey(modal_open_brace, no_op);
DefineBimodalKey(modal_close_brace, no_op, write_and_auto_tab);
DefineModalKey(modal_colon, no_op);
DefineModalKey(modal_double_quote, no_op);
DefineModalKey(modal_pipe, no_op);
DefineModalKey(modal_open_angle, no_op);
DefineModalKey(modal_close_angle, no_op);
DefineModalKey(modal_question_mark, no_op);// tld_find_and_replace_new);

HOOK_SIG(global_settings) {
    init_memory(app);
    
    tld_push_default_command_names();
    
    change_theme(app, literal("stb"));
    change_font(app, literal("Inconsolata"), true);
    Theme_Color colors[] = {
        {Stag_Default, TLD_COLORS_WHITE},
        
        {Stag_Back,              TLD_COLORS_BLACK},
        {Stag_Pop1,              TLD_COLORS_BLUE},
        {Stag_Pop2,              TLD_COLORS_GREEN},
        {Stag_Margin,            TLD_COLORS_DARK_GRAY},
        {Stag_Margin_Hover,      TLD_COLORS_DARK_GRAY},
        {Stag_At_Cursor,         TLD_COLORS_BLACK},
        {Stag_Comment,           TLD_COLORS_YELLOW},
        {Stag_Keyword,           TLD_COLORS_WHITE},
        {Stag_Str_Constant,      TLD_COLORS_GRAY},
        {Stag_Char_Constant,     TLD_COLORS_WHITE},
        {Stag_Int_Constant,      TLD_COLORS_WHITE},
        {Stag_Float_Constant,    TLD_COLORS_WHITE},
        {Stag_Bool_Constant,     TLD_COLORS_WHITE},
        {Stag_Preproc,           TLD_COLORS_GREEN},
        {Stag_Include,           TLD_COLORS_GRAY},
        {Stag_Highlight_Junk,    TLD_COLORS_BLACK},
        {Stag_Special_Character, TLD_COLORS_RED},
        {Stag_Highlight,         TLD_COLORS_BLUE},
        {Stag_At_Highlight,      TLD_COLORS_BLACK},
    };
    
    int count = ArrayCount(colors);
    set_theme_colors(app, colors, count);
    
    exec_command(app, enter_command_mode);
    exec_command(app, change_active_panel);
    exec_command(app, hide_scrollbar);
    
    return 0;
}

static void
get_file_settings(Application_Links *app, Buffer_Summary *buffer) {
    if (buffer->file_name && buffer->size < (16 << 20)) {
        String ext = file_extension(make_string(buffer->file_name, buffer->file_name_len));
        if (match_ss(ext, make_lit_string("cpp")) ||
            match_ss(ext, make_lit_string("hpp")) ||
            match_ss(ext, make_lit_string("c")) ||
            match_ss(ext, make_lit_string("h")))
        {
            buffer_set_setting(app, buffer, BufferSetting_Lex, true);
            
            buffer_set_setting(app, buffer, BufferSetting_WrapPosition, 900);
            buffer_set_setting(app, buffer, BufferSetting_MinimumBaseWrapPosition, 480);
            
            buffer_set_setting(app, buffer, BufferSetting_WrapLine, true);
            buffer_set_setting(app, buffer, BufferSetting_VirtualWhitespace, true);
        } else if (match_ss(ext, make_lit_string("rs")) ||
                   match_ss(ext, make_lit_string("java")) ||
                   match_ss(ext, make_lit_string("cs"))) {
            buffer_set_setting(app, buffer, BufferSetting_Lex, true);
            
            buffer_set_setting(app, buffer, BufferSetting_WrapLine, false);
            buffer_set_setting(app, buffer, BufferSetting_VirtualWhitespace, false);
        } else if (match_ss(ext, make_lit_string("xml"))) {
            buffer_set_setting(app, buffer, BufferSetting_Lex, false);
            
            buffer_set_setting(app, buffer, BufferSetting_WrapLine, false);
            buffer_set_setting(app, buffer, BufferSetting_VirtualWhitespace, false);
        } else if (match_ss(ext, make_lit_string("4proj"))) {
            tld_current_project = tld_project_load_from_buffer(app, buffer->buffer_id, &tld_current_project_memory);
            tld_project_open_source_files(app, &tld_current_project, &tld_current_project_memory);
        } else {
            buffer_set_setting(app, buffer, BufferSetting_Lex, false);
            
            buffer_set_setting(app, buffer, BufferSetting_WrapLine, true);
            buffer_set_setting(app, buffer, BufferSetting_WrapPosition, 950);
            buffer_set_setting(app, buffer, BufferSetting_VirtualWhitespace, false);
        }
    }
}

OPEN_FILE_HOOK_SIG(new_file_settings) {
    Buffer_Summary buffer = get_buffer(app, buffer_id, AccessProtected | AccessHidden);
    get_file_settings(app, &buffer);
    
    int32_t buffer_lexed = 0;
    if (buffer_get_setting(app, &buffer, BufferSetting_Lex, &buffer_lexed) && buffer_lexed) {
        buffer_replace_range(app, &buffer, 0, 0, literal("\nNotice: No warranty is offered or implied; use this code at your own risk.\n******************************************************************************/\n\n"));
        buffer_replace_range(app, &buffer, 0, 0, literal("\nAuthor: Tristan Dannenberg"));
        buffer_replace_range(app, &buffer, 0, 0, buffer.buffer_name, buffer.buffer_name_len);
        buffer_replace_range(app, &buffer, 0, 0, literal("/******************************************************************************\nFile: "));
    }
    
    return 0;
}

OPEN_FILE_HOOK_SIG(file_settings) {
    Buffer_Summary buffer = get_buffer(app, buffer_id, AccessProtected | AccessHidden);
    get_file_settings(app, &buffer);
    return 0;
}

void set_key_maps(Bind_Helper *context) {
    begin_map(context, mapid_global);
    
    bind(context, key_mouse_left, MDFR_NONE, click_set_cursor);
    bind(context, key_mouse_right, MDFR_NONE, click_set_mark);
    bind(context, '\t', MDFR_CTRL, auto_tab_line_at_cursor);
    bind(context, ' ', MDFR_CTRL, word_complete);
    bind(context, 'c', MDFR_ALT, copy);
    bind(context, 'd', MDFR_ALT, cmdid_open_debug);
    bind(context, 'f', MDFR_ALT, tld_find_and_replace);
    bind(context, 'g', MDFR_ALT, goto_line);
    bind(context, 'h', MDFR_ALT, tld_find_and_replace_selection);
    bind(context, 'n', MDFR_ALT, cmdid_interactive_new);
    bind(context, 'o', MDFR_ALT, tld_open_file_fuzzy);
    bind(context, 'p', MDFR_ALT, tld_switch_buffer_fuzzy);
    bind(context, 'q', MDFR_ALT, cmdid_kill_buffer);
    bind(context, 'r', MDFR_ALT, replace_range);
    bind(context, 's', MDFR_ALT, smart_save);
    bind(context, 't', MDFR_ALT, tld_execute_arbitrary_command_fuzzy);
    bind(context, 'v', MDFR_ALT, paste);
    bind(context, 'w', MDFR_ALT, close_panel);
    bind(context, 'x', MDFR_ALT, cut);
    bind(context, 'y', MDFR_ALT, cmdid_redo);
    bind(context, 'z', MDFR_ALT, cmdid_undo);
    bind(context, '\'', MDFR_ALT, open_file_in_quotes);
    bind(context, key_back, MDFR_ALT, delete_range);
    bind(context, key_back, MDFR_SHIFT, backspace_word);
    // bind(context, '`', MDFR_ALT, execute_any_command); // TODO: Port the metaprogram
    
    bind(context, key_f1, MDFR_NONE, interactive_find_file);
    bind(context, key_f2, MDFR_NONE, list_all_functions_current_buffer);
    bind(context, key_f5, MDFR_NONE, git_quick_save);
    // TODO: bind(context, key_f5, MDFR_SHIFT, git_add_current_file);
    bind(context, key_f7, MDFR_NONE, tld_current_project_save_and_build);
    bind(context, key_f7, MDFR_SHIFT, tld_current_project_save_and_change_build_config);
    bind(context, key_f8, MDFR_NONE, tld_current_project_save_build_and_debug);
    bind(context, key_f8, MDFR_SHIFT, tld_current_project_save_build_and_change_debug_config);
    bind(context, key_f11, MDFR_NONE, maximize_panel);
    
    // TODO: Unused key bindings
    bind(context, key_f3, MDFR_NONE, no_op);
    bind(context, key_f4, MDFR_NONE, no_op);
    bind(context, key_f6, MDFR_NONE, no_op);
    bind(context, key_f9, MDFR_NONE, no_op);
    bind(context, key_f10, MDFR_NONE, no_op);
    bind(context, key_f12, MDFR_NONE, no_op);
    
    bind(context, key_esc, MDFR_NONE, enter_command_mode);
    bind(context, ' ', MDFR_NONE, modal_space);
    
    bind(context, '`', MDFR_NONE, modal_backtick);
    bind(context, '1', MDFR_NONE, modal_1);
    bind(context, '2', MDFR_NONE, modal_2);
    bind(context, '3', MDFR_NONE, modal_3);
    bind(context, '4', MDFR_NONE, modal_4);
    bind(context, '5', MDFR_NONE, modal_5);
    bind(context, '6', MDFR_NONE, modal_6);
    bind(context, '7', MDFR_NONE, modal_7);
    bind(context, '8', MDFR_NONE, modal_8);
    bind(context, '9', MDFR_NONE, modal_9);
    bind(context, '0', MDFR_NONE, modal_0);
    bind(context, '-', MDFR_NONE, modal_dash);
    bind(context, '=', MDFR_NONE, modal_equals);
    bind(context, key_back, MDFR_NONE, backspace_char);
    
    bind(context, '~', MDFR_NONE, modal_tilde);
    bind(context, '!', MDFR_NONE, modal_exclamation_mark);
    bind(context, '@', MDFR_NONE, modal_at);
    bind(context, '#', MDFR_NONE, modal_pound);
    bind(context, '$', MDFR_NONE, modal_dollar);
    bind(context, '%', MDFR_NONE, modal_percent);
    bind(context, '^', MDFR_NONE, modal_caret);
    bind(context, '&', MDFR_NONE, modal_and);
    bind(context, '*', MDFR_NONE, modal_asterisk);
    bind(context, '(', MDFR_NONE, modal_open_paren);
    bind(context, ')', MDFR_NONE, modal_close_paren);
    bind(context, '_', MDFR_NONE, modal_lodash);
    bind(context, '+', MDFR_NONE, modal_plus);
    
    bind(context, 'a', MDFR_NONE, modal_a);
    bind(context, 'b', MDFR_NONE, modal_b);
    bind(context, 'c', MDFR_NONE, modal_c);
    bind(context, 'd', MDFR_NONE, modal_d);
    bind(context, 'e', MDFR_NONE, modal_e);
    bind(context, 'f', MDFR_NONE, modal_f);
    bind(context, 'g', MDFR_NONE, modal_g);
    bind(context, 'h', MDFR_NONE, modal_h);
    bind(context, 'i', MDFR_NONE, modal_i);
    bind(context, 'j', MDFR_NONE, modal_j);
    bind(context, 'k', MDFR_NONE, modal_k);
    bind(context, 'l', MDFR_NONE, modal_l);
    bind(context, 'm', MDFR_NONE, modal_m);
    bind(context, 'n', MDFR_NONE, modal_n);
    bind(context, 'o', MDFR_NONE, modal_o);
    bind(context, 'p', MDFR_NONE, modal_p);
    bind(context, 'q', MDFR_NONE, modal_q);
    bind(context, 'r', MDFR_NONE, modal_r);
    bind(context, 's', MDFR_NONE, modal_s);
    bind(context, 't', MDFR_NONE, modal_t);
    bind(context, 'u', MDFR_NONE, modal_u);
    bind(context, 'v', MDFR_NONE, modal_v);
    bind(context, 'w', MDFR_NONE, modal_w);
    bind(context, 'x', MDFR_NONE, modal_x);
    bind(context, 'y', MDFR_NONE, modal_y);
    bind(context, 'z', MDFR_NONE, modal_z);
    bind(context, '\n', MDFR_NONE, write_or_goto_position);
    
    bind(context, 'A', MDFR_NONE, modal_A);
    bind(context, 'B', MDFR_NONE, modal_B);
    bind(context, 'C', MDFR_NONE, modal_C);
    bind(context, 'D', MDFR_NONE, modal_D);
    bind(context, 'E', MDFR_NONE, modal_E);
    bind(context, 'F', MDFR_NONE, modal_F);
    bind(context, 'G', MDFR_NONE, modal_G);
    bind(context, 'H', MDFR_NONE, modal_H);
    bind(context, 'I', MDFR_NONE, modal_I);
    bind(context, 'J', MDFR_NONE, modal_J);
    bind(context, 'K', MDFR_NONE, modal_K);
    bind(context, 'L', MDFR_NONE, modal_L);
    bind(context, 'M', MDFR_NONE, modal_M);
    bind(context, 'N', MDFR_NONE, modal_N);
    bind(context, 'O', MDFR_NONE, modal_O);
    bind(context, 'P', MDFR_NONE, modal_P);
    bind(context, 'Q', MDFR_NONE, modal_Q);
    bind(context, 'R', MDFR_NONE, modal_R);
    bind(context, 'S', MDFR_NONE, modal_S);
    bind(context, 'T', MDFR_NONE, modal_T);
    bind(context, 'U', MDFR_NONE, modal_U);
    bind(context, 'V', MDFR_NONE, modal_V);
    bind(context, 'W', MDFR_NONE, modal_W);
    bind(context, 'X', MDFR_NONE, modal_X);
    bind(context, 'Y', MDFR_NONE, modal_Y);
    bind(context, 'Z', MDFR_NONE, modal_Z);
    
    bind(context, '\t', MDFR_NONE, modal_tab);
    bind(context, '[', MDFR_NONE, modal_open_bracket);
    bind(context, ']', MDFR_NONE, modal_close_bracket);
    bind(context, ';', MDFR_NONE, modal_semicolon);
    bind(context, '\'', MDFR_NONE, modal_single_quote);
    bind(context, '\\', MDFR_NONE, modal_backslash);
    bind(context, ',', MDFR_NONE, modal_comma);
    bind(context, '.', MDFR_NONE, modal_period);
    bind(context, '/', MDFR_NONE, modal_slash);
    
    bind(context, '{', MDFR_NONE, modal_open_brace);
    bind(context, '}', MDFR_NONE, modal_close_brace);
    bind(context, ':', MDFR_NONE, modal_colon);
    bind(context, '"', MDFR_NONE, modal_double_quote);
    bind(context, '|', MDFR_NONE, modal_pipe);
    bind(context, '<', MDFR_NONE, modal_open_angle);
    bind(context, '>', MDFR_NONE, modal_close_angle);
    bind(context, '?', MDFR_NONE, modal_question_mark);
    
    end_map(context);
}

extern "C" int32_t get_bindings(void *data, int32_t size) {
    // NOTE: We'll be using this for the entire run of the program
    // We could free on shutdown, but the OS will clean it up anyway
    tld_project_memory_init();
    
    Bind_Helper context_local = begin_bind_helper(data, size);
    Bind_Helper *context = &context_local;
    
    set_hook(context, hook_start, global_settings);
    
    set_open_file_hook(context, file_settings);
    set_new_file_hook(context, new_file_settings);
    set_command_caller(context, default_command_caller);
    set_scroll_rule(context, smooth_scroll_rule);
    
    set_key_maps(context);
    
    int32_t result = end_bind_helper(context);
    return result;
}