/******************************************************************************
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
*******************************************************************************
LICENSE

This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.
*******************************************************************************
This file hosts the small commands that depend on nothing but 4tld_user_interface.h.

Preprocessor Variables:
* TLDUI_CMD_NAME_CAPACITY is the maximum number of named commands that can be
  registered for use with tld_execute_arbitrary_command_fuzzy.
  
Provided Commands:
* tld_panels_switch_or_create
* tld_switch_buffer_fuzzy
* tld_open_line
* tld_open_line_upwards
* tld_select_token
* tld_seek_beginning_of_scope
* tld_seek_end_of_scope
* tld_seek_character_right
* tld_seek_character_left
* tld_execute_arbitrary_command_fuzzy
******************************************************************************/

#include "4tld_user_interface.h"

CUSTOM_COMMAND_SIG(tld_panels_switch_or_create) {
    int view_count = 0;
    
    for (View_Summary view = get_view_first(app, AccessAll);
         view.exists; get_view_next(app, &view, AccessAll))
    {
        ++view_count;
    }
    
    if (view_count <= 1) {
        exec_command(app, open_panel_vsplit);
    } else {
        exec_command(app, change_active_panel);
    }
}

CUSTOM_COMMAND_SIG(tld_switch_buffer_fuzzy) {
    tldui_string buffer_list[128];
    int32_t buffer_count = 0;
    
    for (Buffer_Summary buffer = get_buffer_first(app, AccessAll);
         buffer.exists && buffer_count < 128;
         get_buffer_next(app, &buffer, AccessAll))
    {
        buffer_list[buffer_count].value = make_string(buffer.buffer_name, buffer.buffer_name_len);
        buffer_list[buffer_count].matched_pattern_prefix_length = 0;
        
        ++buffer_count;
    }
    
    Query_Bar search_bar;
    char search_bar_space[TLDUI_MAX_PATTERN_SIZE];
    search_bar.prompt = make_lit_string("Go to Buffer: ");
    search_bar.string = make_fixed_width_string(search_bar_space);
    start_query_bar(app, &search_bar, 0);
    
    int32_t buffer_name_index = tldui_query_fuzzy_list(app, &search_bar, buffer_list, buffer_count);
    if (buffer_name_index >= 0) {
        String buffer_name = buffer_list[buffer_name_index].value;
        View_Summary view = get_active_view(app, AccessAll);
        Buffer_Summary buffer = get_buffer_by_name(app, expand_str(buffer_name), AccessAll);
        view_set_buffer(app, &view, buffer.buffer_id, 0);
    }
}

// 
// Editing Commands
// 

CUSTOM_COMMAND_SIG(tld_open_line) {
    exec_command(app, seek_end_of_line);
    write_string(app, make_lit_string("\n"));
    exec_command(app, auto_tab_line_at_cursor);
}

CUSTOM_COMMAND_SIG(tld_open_line_upwards) {
    exec_command(app, seek_beginning_of_line);
    write_string(app, make_lit_string("\n"));
    exec_command(app, move_up);
    exec_command(app, auto_tab_line_at_cursor);
}

CUSTOM_COMMAND_SIG(tld_move_line_up) {
    View_Summary view = get_active_view(app, AccessOpen);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen);
    
    int start = seek_line_beginning(app, &buffer, view.cursor.pos);
    int end = seek_line_end(app, &buffer, view.cursor.pos);
    
    if (start <= 0) {
        return;
    }
    
    if (end < buffer.size) {
        end += 1;
    }
    
    char line_text[4096];
    if (end - start <= sizeof(line_text)) {
        buffer_read_range(app, &buffer, start, end, line_text);
        buffer_replace_range(app, &buffer, start, end, 0, 0);
        
        exec_command(app, move_up);
        write_string(app, make_string(line_text, end - start));
        exec_command(app, move_up);
    }
}

CUSTOM_COMMAND_SIG(tld_move_line_down) {
    View_Summary view = get_active_view(app, AccessOpen);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen);
    
    int start = seek_line_beginning(app, &buffer, view.cursor.pos);
    int end = seek_line_end(app, &buffer, view.cursor.pos);
    
    if (start < 0) {
        start = 0;
    }
    
    if (end < buffer.size) {
        end += 1;
    }
    
    char line_text[4096];
    if (end - start <= sizeof(line_text)) {
        buffer_read_range(app, &buffer, start, end, line_text);
        buffer_replace_range(app, &buffer, start, end, 0, 0);
        
        exec_command(app, move_down);
        write_string(app, make_string(line_text, end - start));
        exec_command(app, move_up);
    }
}

CUSTOM_COMMAND_SIG(tld_write_space_reflow_comment) {
    View_Summary view = get_active_view(app, AccessOpen);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessProtected);
    
    if (buffer.is_lexed) {
        if (view.cursor.character >= 80) {
            int32_t beginning_of_line = -1;
            buffer_seek_string_backward(app, &buffer, view.cursor.pos - 1, 0, literal("\n"), &beginning_of_line);
            
            if (beginning_of_line >= 0) {
                buffer_seek_string_forward(app, &buffer, beginning_of_line, view.cursor.pos, literal("//"), &beginning_of_line);
                
                if (beginning_of_line < view.cursor.pos) {
                    write_string(app, make_lit_string("\n// "));
                    exec_command(app, auto_tab_line_at_cursor);
                    return;
                }
            }
        }
    }
    
    exec_command(app, write_character);
}

// 
// Navigation Commands
// 

CUSTOM_COMMAND_SIG(tld_select_token) {
    // TODO: This feels pretty cool, but is probably useless. See if this can be improved
    View_Summary view = get_active_view(app, AccessProtected);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessProtected);
    
    Cpp_Get_Token_Result get_result = {0};
    bool32 success = buffer_get_token_index(app, &buffer, view.cursor.pos, &get_result);
    
    if (success && !get_result.in_whitespace) {
        view_set_mark(app, &view, seek_pos(get_result.token_start));
        view_set_cursor(app, &view, seek_pos(get_result.token_end), true);
    } else {
        view_set_mark(app, &view, seek_pos(view.cursor.pos));
    }
}

CUSTOM_COMMAND_SIG(tld_seek_beginning_of_scope) {
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    int32_t pos = view.cursor.pos - 1;
    if (pos < 0) return;
    
    Temp_Memory temp = begin_temp_memory(&global_part);
    
    Cpp_Token_Array all_tokens = buffer_get_all_tokens(app, &global_part, &buffer);
    Cpp_Get_Token_Result get_result = {0};
    bool32 success = buffer_get_token_index(app, &buffer, pos, &get_result);
    
    if (success) {
        int32_t nesting = 0;
        
        int32_t i = get_result.token_index;
        for (; i >= 0; --i) {
            if (all_tokens.tokens[i].type == CPP_TOKEN_BRACE_CLOSE) {
                ++nesting;
            } else if (all_tokens.tokens[i].type == CPP_TOKEN_BRACE_OPEN) {
                if (nesting <= 0)
                    break;
                --nesting;
            }
        }
        
        if (i < 0) i = 0;
        
        view_set_cursor(app, &view, seek_pos(all_tokens.tokens[i].start), 0);
    }
    
    end_temp_memory(temp);
}

CUSTOM_COMMAND_SIG(tld_seek_end_of_scope) {
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    int32_t pos = view.cursor.pos;
    if (pos >= buffer.size) return;
    
    Temp_Memory temp = begin_temp_memory(&global_part);
    
    Cpp_Token_Array all_tokens = buffer_get_all_tokens(app, &global_part, &buffer);
    Cpp_Get_Token_Result get_result = {0};
    bool32 success = buffer_get_token_index(app, &buffer, pos, &get_result);
    
    if (success) {
        int32_t nesting = 1;
        
        int32_t i = get_result.token_index + 1;
        for (; i < all_tokens.count; ++i) {
            if (all_tokens.tokens[i].type == CPP_TOKEN_BRACE_OPEN) {
                ++nesting;
            } else if (all_tokens.tokens[i].type == CPP_TOKEN_BRACE_CLOSE) {
                --nesting;
                if (nesting <= 0)
                    break;
            }
        }
        
        if (i >= all_tokens.count) {
            pos = buffer.size;
        } else {
            pos = all_tokens.tokens[i].start;
        }
        view_set_cursor(app, &view, seek_pos(pos), 0);
    }
    
    end_temp_memory(temp);
}

CUSTOM_COMMAND_SIG(tld_seek_indent_left) {
    exec_command(app, seek_beginning_of_line);
    
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    move_past_lead_whitespace(app, &view, &buffer);
}

CUSTOM_COMMAND_SIG(tld_seek_character_right) {
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    Assert(buffer.exists);
    
    int32_t pos = view.cursor.pos;
    User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
    if (in.abort) return;
    
    uint8_t character[4];
    uint32_t length = to_writable_character(in, character);
    
    if (length != 0 && key_is_unmodified(&in.key)) {
        buffer_seek_string_forward(app, &buffer, pos + 1, 0, (char *) &character, length, &pos);
        view_set_cursor(app, &view, seek_pos(pos), true);
    }
}

CUSTOM_COMMAND_SIG(tld_seek_character_left) {
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    Assert(buffer.exists);
    
    int32_t pos = view.cursor.pos;
    User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
    if (in.abort) return;
    
    uint8_t character[4];
    uint32_t length = to_writable_character(in, character);
    
    if (length != 0 && key_is_unmodified(&in.key)) {
        buffer_seek_string_backward(app, &buffer, pos - 1, 0, (char *) &character, length, &pos);
        view_set_cursor(app, &view, seek_pos(pos), true);
    }
}

// 
// Arbitrary Command Execution
// 

typedef Custom_Command_Function * tld_custom_command_function_pointer;
static tldui_string *tld_command_names = 0;
static uint32_t tld_command_names_count = 0;
static tld_custom_command_function_pointer *tld_command_functions = 0;

CUSTOM_COMMAND_SIG(tld_execute_arbitrary_command_fuzzy) {
    if (tld_command_functions == 0) return;
    
    char search_bar_space[TLDUI_MAX_PATTERN_SIZE];
    Query_Bar search_bar = {0};
    search_bar.prompt = make_lit_string("Command: ");
    search_bar.string = make_fixed_width_string(search_bar_space);
    
    start_query_bar(app, &search_bar, 0);
    int32_t command_index = tldui_query_fuzzy_list(app, &search_bar,
                                                   tld_command_names,
                                                   tld_command_names_count);
    end_query_bar(app, &search_bar, 0);
    
    if (command_index >= 0) {
        exec_command(app, tld_command_functions[command_index]);
    }
}

#ifndef TLD_CMD_NAME_CAPACITY
#define TLD_CMD_NAME_CAPACITY 128
#endif

static inline bool32
tld_push_named_command(Custom_Command_Function *cmd, char *cmd_name, int32_t cmd_name_len) {
    if (tld_command_names == 0) {
        tld_command_names = (tldui_string *) malloc(
            sizeof(tldui_string) * TLD_CMD_NAME_CAPACITY);
    }
    
    if (tld_command_functions == 0) {
        tld_command_functions = (tld_custom_command_function_pointer *) malloc(
            sizeof(tld_custom_command_function_pointer) * TLD_CMD_NAME_CAPACITY);
    }
    
    if (tld_command_names_count < TLD_CMD_NAME_CAPACITY) {
        tld_command_names[tld_command_names_count] = {0};
        tld_command_names[tld_command_names_count].value = make_string(cmd_name, cmd_name_len);
        
        tld_command_functions[tld_command_names_count] = cmd;
        
        ++tld_command_names_count;
        return true;
    }
    
    return false;
}

static inline void
tld_push_default_command_names() {
    tld_push_named_command(exit_4coder, literal("System: exit 4coder"));
    tld_push_named_command(toggle_mouse, literal("System: toggle mouse suppression"));
    
    tld_push_named_command(to_uppercase, literal("Fancy Editing: range to uppercase"));
    tld_push_named_command(to_lowercase, literal("Fancy Editing: range to lowercase"));
    tld_push_named_command(if0_off, literal("Fancy Editing: surround with if 0"));
    tld_push_named_command(auto_tab_range, literal("Fancy Editing: autotab range"));
    tld_push_named_command(auto_tab_whole_file,
                           literal("Fancy Editing: autotab whole file"));
    
    tld_push_named_command(show_scrollbar, literal("View: show scrollbar"));
    tld_push_named_command(hide_scrollbar, literal("View: hide scrollbar"));
    tld_push_named_command(open_panel_vsplit, literal("View: split vertically"));
    tld_push_named_command(open_panel_hsplit, literal("View: split horizontally"));
    tld_push_named_command(close_panel, literal("View: close"));
    
    tld_push_named_command(kill_buffer, literal("Buffer: close"));
    tld_push_named_command(eol_nixify, literal("Buffer: nixify line endings"));
    tld_push_named_command(eol_dosify, literal("Buffer: dosify line endings"));
    tld_push_named_command(toggle_line_wrap, literal("Buffer: toggle line wrap"));
    tld_push_named_command(toggle_virtual_whitespace,
                           literal("Buffer: toggle virtual whitespace"));
    
    tld_push_named_command(open_long_braces, literal("Write Snippet: scope"));
    tld_push_named_command(write_todo, literal("Write Snippet: TODO"));
    tld_push_named_command(write_note, literal("Write Snippet: NOTE"));
    tld_push_named_command(write_block, literal("Write Snippet: block comment"));
    tld_push_named_command(write_zero_struct, literal("Write Snippet: zero struct"));
    
    tld_push_named_command(open_file_in_quotes, literal("Files: open filename in quotes"));
    tld_push_named_command(open_matching_file_cpp, literal("Files: open matching cpp/h file"));
}
