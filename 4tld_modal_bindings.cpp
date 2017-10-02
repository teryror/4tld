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
// NOTE: Forward declare these, the actual bindings happen in 4tld_custom.cpp
// 

#define BIMODAL_COMMAND(name, modal_command, command)                 \
CUSTOM_COMMAND_SIG(name) {                                            \
    if (__tld_cmd_mode_enabled) { exec_command(app, modal_command); } \
    else exec_command(app, command);                                  \
}

#define MODAL_COMMAND(name, modal_command) BIMODAL_COMMAND(name, modal_command, write_character)

// This is useful for binding modal keys that only function in one mode (i.e. temporary bindings)
CUSTOM_COMMAND_SIG(nop) { }

CUSTOM_COMMAND_SIG(modal_escape);
CUSTOM_COMMAND_SIG(modal_space);

CUSTOM_COMMAND_SIG(modal_backtick);
CUSTOM_COMMAND_SIG(modal_1);
CUSTOM_COMMAND_SIG(modal_2);
CUSTOM_COMMAND_SIG(modal_3);
CUSTOM_COMMAND_SIG(modal_4);
CUSTOM_COMMAND_SIG(modal_5);
CUSTOM_COMMAND_SIG(modal_6);
CUSTOM_COMMAND_SIG(modal_7);
CUSTOM_COMMAND_SIG(modal_8);
CUSTOM_COMMAND_SIG(modal_9);
CUSTOM_COMMAND_SIG(modal_0);
CUSTOM_COMMAND_SIG(modal_dash);
CUSTOM_COMMAND_SIG(modal_equals);

CUSTOM_COMMAND_SIG(modal_tilde);
CUSTOM_COMMAND_SIG(modal_exclamation_mark);
CUSTOM_COMMAND_SIG(modal_at);
CUSTOM_COMMAND_SIG(modal_pound);
CUSTOM_COMMAND_SIG(modal_dollar);
CUSTOM_COMMAND_SIG(modal_percent);
CUSTOM_COMMAND_SIG(modal_caret);
CUSTOM_COMMAND_SIG(modal_and);
CUSTOM_COMMAND_SIG(modal_asterisk);
CUSTOM_COMMAND_SIG(modal_open_paren);
CUSTOM_COMMAND_SIG(modal_close_paren);
CUSTOM_COMMAND_SIG(modal_lodash);
CUSTOM_COMMAND_SIG(modal_plus);

CUSTOM_COMMAND_SIG(modal_a);
CUSTOM_COMMAND_SIG(modal_b);
CUSTOM_COMMAND_SIG(modal_c);
CUSTOM_COMMAND_SIG(modal_d);
CUSTOM_COMMAND_SIG(modal_e);
CUSTOM_COMMAND_SIG(modal_f);
CUSTOM_COMMAND_SIG(modal_g);
CUSTOM_COMMAND_SIG(modal_h);
CUSTOM_COMMAND_SIG(modal_i);
CUSTOM_COMMAND_SIG(modal_j);
CUSTOM_COMMAND_SIG(modal_k);
CUSTOM_COMMAND_SIG(modal_l);
CUSTOM_COMMAND_SIG(modal_m);
CUSTOM_COMMAND_SIG(modal_n);
CUSTOM_COMMAND_SIG(modal_o);
CUSTOM_COMMAND_SIG(modal_p);
CUSTOM_COMMAND_SIG(modal_q);
CUSTOM_COMMAND_SIG(modal_r);
CUSTOM_COMMAND_SIG(modal_s);
CUSTOM_COMMAND_SIG(modal_t);
CUSTOM_COMMAND_SIG(modal_u);
CUSTOM_COMMAND_SIG(modal_v);
CUSTOM_COMMAND_SIG(modal_w);
CUSTOM_COMMAND_SIG(modal_x);
CUSTOM_COMMAND_SIG(modal_y);
CUSTOM_COMMAND_SIG(modal_z);
CUSTOM_COMMAND_SIG(modal_return);

CUSTOM_COMMAND_SIG(modal_A);
CUSTOM_COMMAND_SIG(modal_B);
CUSTOM_COMMAND_SIG(modal_C);
CUSTOM_COMMAND_SIG(modal_D);
CUSTOM_COMMAND_SIG(modal_E);
CUSTOM_COMMAND_SIG(modal_F);
CUSTOM_COMMAND_SIG(modal_G);
CUSTOM_COMMAND_SIG(modal_H);
CUSTOM_COMMAND_SIG(modal_I);
CUSTOM_COMMAND_SIG(modal_J);
CUSTOM_COMMAND_SIG(modal_K);
CUSTOM_COMMAND_SIG(modal_H);
CUSTOM_COMMAND_SIG(modal_I);
CUSTOM_COMMAND_SIG(modal_L);
CUSTOM_COMMAND_SIG(modal_M);
CUSTOM_COMMAND_SIG(modal_N);
CUSTOM_COMMAND_SIG(modal_O);
CUSTOM_COMMAND_SIG(modal_P);
CUSTOM_COMMAND_SIG(modal_Q);
CUSTOM_COMMAND_SIG(modal_R);
CUSTOM_COMMAND_SIG(modal_S);
CUSTOM_COMMAND_SIG(modal_T);
CUSTOM_COMMAND_SIG(modal_U);
CUSTOM_COMMAND_SIG(modal_V);
CUSTOM_COMMAND_SIG(modal_W);
CUSTOM_COMMAND_SIG(modal_X);
CUSTOM_COMMAND_SIG(modal_Y);
CUSTOM_COMMAND_SIG(modal_Z);

CUSTOM_COMMAND_SIG(modal_tab);
CUSTOM_COMMAND_SIG(modal_open_bracket);
CUSTOM_COMMAND_SIG(modal_close_bracket);
CUSTOM_COMMAND_SIG(modal_semicolon);
CUSTOM_COMMAND_SIG(modal_single_quote);
CUSTOM_COMMAND_SIG(modal_backslash);
CUSTOM_COMMAND_SIG(modal_comma);
CUSTOM_COMMAND_SIG(modal_period);
CUSTOM_COMMAND_SIG(modal_slash);

CUSTOM_COMMAND_SIG(modal_open_brace);
CUSTOM_COMMAND_SIG(modal_close_brace);
CUSTOM_COMMAND_SIG(modal_colon);
CUSTOM_COMMAND_SIG(modal_double_quote);
CUSTOM_COMMAND_SIG(modal_pipe);
CUSTOM_COMMAND_SIG(modal_open_angle);
CUSTOM_COMMAND_SIG(modal_close_angle);
CUSTOM_COMMAND_SIG(modal_question_mark);

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