/******************************************************************************
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
*******************************************************************************
LICENSE

This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.
*******************************************************************************
This file provides a modal keymap, as well as a couple of commands that make
editing in that setup more comfortable. A number of other commands are assumed
to exist here, some of them defined by other 4tld packages.

Since this file is highly specific to my personal preferences, no macro-based
configuration switches are used.

Provided Commands:
* tld_exit_cmd_mode
* tld_enter_cmd_mode
* tld_replace_range
* tld_write_individual_character
* tld_replace_individual_character
******************************************************************************/

static bool32 __tld_cmd_mode_enabled = 0;
static String __tld_modal_themes[2] = {0};

void __tld_enter_mode(Application_Links * app, bool32 new_mode) {
    __tld_cmd_mode_enabled = new_mode;
    
    String theme = __tld_modal_themes[new_mode];
    change_theme(app, theme.str, theme.size);
}

CUSTOM_COMMAND_SIG(tld_exit_cmd_mode) {
    __tld_enter_mode(app, 0);
}

CUSTOM_COMMAND_SIG(tld_enter_cmd_mode) {
    __tld_enter_mode(app, 1);
}

// 
// Simple Edits in CMD Mode
// 

CUSTOM_COMMAND_SIG(tld_replace_range) {
    exec_command(app, delete_range);
    exec_command(app, tld_exit_cmd_mode);
}

CUSTOM_COMMAND_SIG(tld_write_individual_character) {
    View_Summary view = get_active_view(app, AccessOpen);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen);
    if (!buffer.exists) return;
    
    int32_t pos = view.cursor.pos;
    User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
    if (in.abort) return;
    
    uint8_t character[4];
    uint32_t length = to_writable_character(in, character);
    if (length != 0 && key_is_unmodified(&in.key)) {
        buffer_replace_range(app, &buffer, pos, pos, (char *) &character, length);
        view_set_cursor(app, &view, seek_pos(pos + length), true);
    }
}

CUSTOM_COMMAND_SIG(tld_replace_individual_character) {
    View_Summary view = get_active_view(app, AccessOpen);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen);
    if (!buffer.exists) return;
    
    int32_t start = view.cursor.pos;
    
    Full_Cursor cursor;
    view_compute_cursor(app, &view, seek_character_pos(view.cursor.character_pos + 1), &cursor);
    int32_t end = cursor.pos;
    
    User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
    if (in.abort) return;
    
    uint8_t character[4];
    uint32_t length = to_writable_character(in, character);
    if (length != 0 && key_is_unmodified(&in.key)) {
        buffer_replace_range(app, &buffer, start, end, (char *) &character, length);
        view_set_cursor(app, &view, seek_pos(start + length), true);
    }
}

// 
// Bindings
// 

#define BIMODAL_COMMAND(name, modal_command, command)                 \
CUSTOM_COMMAND_SIG(name) {                                            \
    if (__tld_cmd_mode_enabled) { exec_command(app, modal_command); } \
    else exec_command(app, command);                                  \
}

#define MODAL_COMMAND(name, modal_command) BIMODAL_COMMAND(name, modal_command, write_character)

// This is useful for binding modal keys that only function in one mode (i.e. temporary bindings)
CUSTOM_COMMAND_SIG(nop) { }

// TODO: Forward declare these instead, and move the actual bindings into 4tld_custom.cpp

BIMODAL_COMMAND(modal_escape, tld_execute_arbitrary_command_fuzzy, tld_enter_cmd_mode)
BIMODAL_COMMAND(modal_space, tld_exit_cmd_mode, tld_write_space_reflow_comment)

MODAL_COMMAND(modal_backtick, nop)
MODAL_COMMAND(modal_1, nop)
MODAL_COMMAND(modal_2, nop)
MODAL_COMMAND(modal_3, nop)
MODAL_COMMAND(modal_4, nop)
MODAL_COMMAND(modal_5, nop)
MODAL_COMMAND(modal_6, nop)
MODAL_COMMAND(modal_7, nop)
MODAL_COMMAND(modal_8, nop)
MODAL_COMMAND(modal_9, nop)
MODAL_COMMAND(modal_0, nop)
MODAL_COMMAND(modal_dash, nop)
MODAL_COMMAND(modal_equals, nop)

MODAL_COMMAND(modal_tilde, nop)
MODAL_COMMAND(modal_exclamation_mark, nop)
MODAL_COMMAND(modal_at, nop)
MODAL_COMMAND(modal_pound, nop)
MODAL_COMMAND(modal_dollar, nop)
MODAL_COMMAND(modal_percent, nop)
MODAL_COMMAND(modal_caret, nop)
MODAL_COMMAND(modal_and, nop)
MODAL_COMMAND(modal_asterisk, nop)
MODAL_COMMAND(modal_open_paren, nop)
MODAL_COMMAND(modal_close_paren, nop)
MODAL_COMMAND(modal_lodash, nop)
MODAL_COMMAND(modal_plus, nop)

MODAL_COMMAND(modal_a, nop)
MODAL_COMMAND(modal_b, nop)
MODAL_COMMAND(modal_c, duplicate_line)
MODAL_COMMAND(modal_d, delete_char)
MODAL_COMMAND(modal_e, nop)
MODAL_COMMAND(modal_f, tld_seek_character_right)
MODAL_COMMAND(modal_g, nop)
MODAL_COMMAND(modal_h, tld_seek_indent_left)
MODAL_COMMAND(modal_i, move_up)
MODAL_COMMAND(modal_j, move_left)
MODAL_COMMAND(modal_k, move_down)
MODAL_COMMAND(modal_l, move_right)
MODAL_COMMAND(modal_m, nop)
MODAL_COMMAND(modal_n, tld_write_individual_character)
MODAL_COMMAND(modal_o, tld_open_line)
MODAL_COMMAND(modal_p, nop)
MODAL_COMMAND(modal_q, nop)
MODAL_COMMAND(modal_r, tld_replace_individual_character)
MODAL_COMMAND(modal_s, nop)
MODAL_COMMAND(modal_t, nop)
MODAL_COMMAND(modal_u, tld_open_line_upwards)
MODAL_COMMAND(modal_v, nop)
MODAL_COMMAND(modal_w, tld_select_token)
MODAL_COMMAND(modal_x, delete_line)
MODAL_COMMAND(modal_y, nop)
MODAL_COMMAND(modal_z, nop)
BIMODAL_COMMAND(modal_return, nop, write_and_auto_tab)

MODAL_COMMAND(modal_A, nop)
MODAL_COMMAND(modal_B, nop)
MODAL_COMMAND(modal_C, nop)
MODAL_COMMAND(modal_D, delete_word)
MODAL_COMMAND(modal_E, nop)
MODAL_COMMAND(modal_F, tld_seek_character_left)
MODAL_COMMAND(modal_G, nop)
MODAL_COMMAND(modal_H, nop)
MODAL_COMMAND(modal_I, seek_whitespace_up)
MODAL_COMMAND(modal_J, seek_alphanumeric_left)
MODAL_COMMAND(modal_K, seek_whitespace_down)
MODAL_COMMAND(modal_L, seek_alphanumeric_right)
MODAL_COMMAND(modal_M, nop)
MODAL_COMMAND(modal_N, nop)
MODAL_COMMAND(modal_O, tld_move_line_down)
MODAL_COMMAND(modal_P, nop)
MODAL_COMMAND(modal_Q, nop)
MODAL_COMMAND(modal_R, nop)
MODAL_COMMAND(modal_S, nop)
MODAL_COMMAND(modal_T, nop)
MODAL_COMMAND(modal_U, tld_move_line_up)
MODAL_COMMAND(modal_V, nop)
MODAL_COMMAND(modal_W, nop)
MODAL_COMMAND(modal_X, nop)
MODAL_COMMAND(modal_Y, nop)
MODAL_COMMAND(modal_Z, nop)

MODAL_COMMAND(modal_tab, tld_panels_switch_or_create)
MODAL_COMMAND(modal_open_bracket, tld_seek_beginning_of_scope)
MODAL_COMMAND(modal_close_bracket, tld_seek_end_of_scope)
MODAL_COMMAND(modal_semicolon, seek_end_of_line)
MODAL_COMMAND(modal_single_quote, nop)
MODAL_COMMAND(modal_backslash, nop)
MODAL_COMMAND(modal_comma, cursor_mark_swap)
MODAL_COMMAND(modal_period, set_mark)
MODAL_COMMAND(modal_slash, nop)

MODAL_COMMAND(modal_open_brace, nop)
BIMODAL_COMMAND(modal_close_brace, nop, write_and_auto_tab)
MODAL_COMMAND(modal_colon, nop)
MODAL_COMMAND(modal_double_quote, nop)
MODAL_COMMAND(modal_pipe, nop)
MODAL_COMMAND(modal_open_angle, nop)
MODAL_COMMAND(modal_close_angle, nop)
MODAL_COMMAND(modal_question_mark, nop)

void tld_bind_modal_map(Bind_Helper *context, uint32_t mapid,
                        String insert_mode_theme_name,
                        String cmd_mode_theme_name)
{
    __tld_modal_themes[0] = insert_mode_theme_name;
    __tld_modal_themes[1] = cmd_mode_theme_name;
    
    begin_map(context, mapid);
    
    // Non-modal key combos:
    bind(context, key_f5, MDFR_NONE, tld_project_show_vcs_status);
    bind(context, key_f7, MDFR_NONE, tld_build_project);
    bind(context, key_f8, MDFR_NONE, tld_test_project);
    bind(context, key_mouse_left, MDFR_NONE, click_set_cursor);
    bind(context, key_mouse_right, MDFR_NONE, click_set_mark);
    bind(context, '\t', MDFR_CTRL, auto_tab_line_at_cursor);
    bind(context, ' ', MDFR_CTRL, word_complete);
    bind(context, 'c', MDFR_ALT, copy);
    bind(context, 'd', MDFR_ALT, cmdid_open_debug);
    bind(context, 'e', MDFR_ALT, tld_files_show_directory_of_current_file);
    bind(context, 'f', MDFR_ALT, tld_find_and_replace);
    bind(context, 'g', MDFR_ALT, goto_line);
    bind(context, 'h', MDFR_ALT, tld_find_and_replace_selection);
    bind(context, 'n', MDFR_ALT, interactive_new);
    // TODO: For now, let's see if we can get by without an explicit OPEN command
    // bind(context, 'o', MDFR_ALT, interactive_open);
    bind(context, 'p', MDFR_ALT, tld_switch_buffer_fuzzy);
    bind(context, 'q', MDFR_ALT, kill_buffer);
    bind(context, 'r', MDFR_ALT, tld_replace_range);
    bind(context, 's', MDFR_ALT, save);
    bind(context, 'v', MDFR_ALT, paste_and_indent);
    bind(context, 'w', MDFR_ALT, close_panel);
    bind(context, 'x', MDFR_ALT, cut);
    bind(context, 'y', MDFR_ALT, redo);
    bind(context, 'z', MDFR_ALT, undo);
    bind(context, '\'', MDFR_ALT, open_file_in_quotes);
    bind(context, key_back, MDFR_ALT, delete_range);
    bind(context, key_back, MDFR_SHIFT, backspace_word);
    
    bind(context, ' ', MDFR_NONE, modal_space);
    bind(context, key_esc, MDFR_NONE, modal_escape);
    
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
    bind(context, '\n', MDFR_NONE, modal_return);
    
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