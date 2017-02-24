/******************************************************************************
File: 4tld_find_and_replace.cpp
Author: Tristan Dannenberg
Notice: No warranty is offered or imlied; use this code at your own risk.
******************************************************************************/
#include "4tld_user_interface.h"

struct tld_Search {
    String find_what;
    String replace_with;
    bool backwards;
    bool case_sensitive;
};

static void
tld_find_and_replace_interactive(Application_Links *app, tld_Search *search_state, int32_t min, int32_t max) {
    View_Summary target_view = get_active_view(app, AccessAll);
    Buffer_Summary target_buffer = get_buffer(app, target_view.buffer_id, AccessAll);
    
    if (min > max) {
        int32_t tmp = min;
        min = max;
        max = tmp;
    }
    
    if (min == 0 && max == 0) {
        max = target_buffer.size;
    }
    
    View_Summary search_view = open_view(app, &target_view, ViewSplit_Top);
    view_set_split_proportion(app, &search_view, 0.15f);
    view_set_setting(app, &search_view, ViewSetting_ShowScrollbar, 0);
    
    Buffer_Summary search_buffer = get_buffer_by_name(app, literal("*search*"), AccessAll);
    view_set_buffer(app, &search_view, search_buffer.buffer_id, 0);
    
    Query_Bar find_bar = {0};
    find_bar.prompt = make_lit_string("[M-f]    Find What: ");
    find_bar.string = search_state->find_what;
    
    Query_Bar replace_bar = {0};
    replace_bar.prompt = make_lit_string("[M-r] Replace With: ");
    replace_bar.string = search_state->replace_with;
    
    String enabled = make_lit_string("[X]");
    String disabled = make_lit_string("[ ]");
    
    Query_Bar case_bar = {0};
    case_bar.prompt = make_lit_string("[c] Case Sensitive: ");
    case_bar.string = search_state->case_sensitive ? enabled : disabled;
    
    Query_Bar hint_bar = {0};
    hint_bar.prompt = make_lit_string("i/j: find prev./next match -- r: replace -- R: replace all -- a: list all matches -- A: search all buffers");
    
    start_query_bar(app, &hint_bar, 0);
    start_query_bar(app, &case_bar, 0);
    start_query_bar(app, &replace_bar, 0);
    start_query_bar(app, &find_bar, 0);
    
    int mode;
    if (search_state->find_what.size) {
        mode = 2;
        set_active_view(app, &target_view);
    } else {
        mode = 0;
    }
    
    bool goto_next_match = false;
    
    Range match = make_range(0, 0);
    int pos = target_view.cursor.pos;
    int new_pos;
    
    User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
    while (true) {
        if (in.abort) {
            search_state->find_what.size = 0;
            search_state->replace_with.size = 0;
            search_state->case_sensitive = false;
            search_state->backwards = false;
            view_set_highlight(app, &target_view, 0, 0, false);
            
            close_view(app, &search_view);
            return;
        } else if (in.type != UserInputKey) {
            in = get_user_input(app, EventOnAnyKey, EventOnEsc);
            continue;
        }
        
        switch (mode) {
            case 0: {
                if (in.key.keycode == '\t') {
                    mode = 1;
                } else if (in.key.keycode == '\n') {
                    set_active_view(app, &target_view);
                    mode = 2;
                    goto_next_match = true;
                } else if (in.key.keycode == key_back) {
                    if (find_bar.string.size) {
                        find_bar.string.size -= 1;
                    }
                } else if (in.key.keycode == key_del) {
                    find_bar.string.size = 0;
                } else if (key_is_unmodified(&in.key) && in.key.character) {
                    append_s_char(&find_bar.string, in.key.character);
                }
            } break;
            case 1: {
                if (in.key.keycode == '\t') {
                    mode = 0;
                } else if (in.key.keycode == '\n') {
                    set_active_view(app, &target_view);
                    mode = 2;
                    goto_next_match = true;;
                } else if (in.key.keycode == key_back) {
                    if (replace_bar.string.size) {
                        replace_bar.string.size -= 1;
                    }
                } else if (in.key.keycode == key_del) {
                    replace_bar.string.size = 0;
                } else if (key_is_unmodified(&in.key) && in.key.character) {
                    append_s_char(&replace_bar.string, in.key.character);
                }
            } break;
            case 2: {
                if (in.key.keycode == 'f') {
                    if (key_is_unmodified(&in.key)) {
                        goto_next_match = true;
                    } else if (in.key.modifiers[MDFR_ALT]) {
                        set_active_view(app, &search_view);
                        mode = 0;
                    }
                } else if (in.key.keycode == 'c') {
                    search_state->case_sensitive = !search_state->case_sensitive;
                    case_bar.string = search_state->case_sensitive ? enabled : disabled;
                } else if (in.key.keycode == 'r') {
                    if (key_is_unmodified(&in.key)) {
                        if (target_buffer.lock_flags & AccessProtected) {
                            break;
                        }
                        
                        if (match.max - match.min > 0) {
                            buffer_replace_range(app, &target_buffer, match.min, match.max, expand_str(replace_bar.string));
                            goto_next_match = true;
                        }
                    } else if (in.key.modifiers[MDFR_ALT]) {
                        set_active_view(app, &search_view);
                        mode = 1;
                    }
                } else if (in.key.keycode == 'R') {
                    if (target_buffer.lock_flags & AccessProtected) {
                        break;
                    }
                    
                    new_pos = min;
                    while (new_pos < max) {
                        if (search_state->case_sensitive) {
                            buffer_seek_string_forward(app, &target_buffer, new_pos, max - replace_bar.string.size, expand_str(find_bar.string), &new_pos);
                        } else {
                            buffer_seek_string_insensitive_forward(app, &target_buffer, new_pos, max - replace_bar.string.size, expand_str(find_bar.string), &new_pos);
                        }
                        
                        if (new_pos < max) {
                            buffer_replace_range(app, &target_buffer, new_pos, new_pos + replace_bar.string.size, expand_str(replace_bar.string));
                        }
                    }
                    
                    match.min = 0;
                    match.max = 0;
                } else if (in.key.keycode == 'i') {
                    search_state->backwards = true;
                    goto_next_match = true;
                } else if (in.key.keycode == 'k') {
                    search_state->backwards = false;
                    goto_next_match = true;
                } else if (in.key.keycode == ' ') {
                    search_state->find_what = find_bar.string;
                    search_state->replace_with = replace_bar.string;
                    view_set_highlight(app, &target_view, 0, 0, false);
                    
                    close_view(app, &search_view);
                    return;
                } else if (in.key.keycode == 'a') {
                    // TODO: List all matches in current buffer only
                } else if (in.key.keycode == 'A') {
                    search_state->find_what = find_bar.string;
                    search_state->replace_with = replace_bar.string;
                    search_state->case_sensitive = false;
                    search_state->backwards = false;
                    set_active_view(app, &search_view);
                    
                    end_query_bar(app, &hint_bar, 0);
                    end_query_bar(app, &case_bar, 0);
                    end_query_bar(app, &replace_bar, 0);
                    end_query_bar(app, &find_bar, 0);
                    
                    int32_t flags = SearchFlag_MatchWholeWord;
                    if (!search_state->case_sensitive) {
                        flags |= SearchFlag_CaseInsensitive;
                    }
                    generic_search_all_buffers(app, &global_general, &global_part,
                                               find_bar.string, flags);
                    
                    view_set_highlight(app, &target_view, 0, 0, false);
                    close_view(app, &target_view);
                    return;
                }
            } break;
            default: {
                // NOTE: Unreachable
                Assert(false);
            }
        }
        
        if (goto_next_match) {
            goto_next_match = false;
            if (search_state->backwards) {
                if (match.max - match.min > 0) {
                    pos = match.min - 1;
                    if (pos < min) {
                        pos = max;
                    }
                }
                
                if (search_state->case_sensitive) {
                    buffer_seek_string_backward(app, &target_buffer, pos, min, expand_str(find_bar.string), &new_pos);
                } else {
                    buffer_seek_string_insensitive_backward(app, &target_buffer, pos, min, expand_str(find_bar.string), &new_pos);
                }
                if (new_pos < min) {
                    if (search_state->case_sensitive) {
                        buffer_seek_string_backward(app, &target_buffer, max - find_bar.string.size, min, expand_str(find_bar.string), &new_pos);
                    } else {
                        buffer_seek_string_insensitive_backward(app, &target_buffer, max - find_bar.string.size, min, expand_str(find_bar.string), &new_pos);
                    }
                    
                    if (new_pos < min) {
                        match.min = 0;
                        match.max = 0;
                        goto retire_iteration;
                    }
                }
                
                match.min = new_pos;
                match.max = match.min + find_bar.string.size;
            } else {
                if (match.max - match.min > 0) {
                    pos = match.max + 1;
                    if (pos >= max - find_bar.string.size) {
                        pos = min;
                    }
                }
                
                if (search_state->case_sensitive) {
                    buffer_seek_string_forward(app, &target_buffer, pos, max - find_bar.string.size, expand_str(find_bar.string), &new_pos);
                } else {
                    buffer_seek_string_insensitive_forward(app, &target_buffer, pos, max - find_bar.string.size, expand_str(find_bar.string), &new_pos);
                }
                if (new_pos >= max - find_bar.string.size) {
                    if (search_state->case_sensitive) {
                        buffer_seek_string_forward(app, &target_buffer, min, max - find_bar.string.size, expand_str(find_bar.string), &new_pos);
                    } else {
                        buffer_seek_string_insensitive_forward(app, &target_buffer, min, max - find_bar.string.size, expand_str(find_bar.string), &new_pos);
                    }
                    
                    if (new_pos >= max - find_bar.string.size) {
                        match.min = 0;
                        match.max = 0;
                        goto retire_iteration;
                    }
                }
                
                match.min = new_pos;
                match.max = match.min + find_bar.string.size;
            }
            
            retire_iteration:
            
            if (match.max - match.min > 0) {
                view_set_highlight(app, &target_view, match.min, match.max, true);
                exec_command(app, center_view);
            } else {
                view_set_highlight(app, &target_view, 0, 0, false);
            }
        }
        
        in = get_user_input(app, EventOnAnyKey, EventOnEsc);
    }
}

static char tld_search_find_space[512];
static char tld_search_replace_space[512];
static tld_Search tld_search_state = {0};

CUSTOM_COMMAND_SIG(tld_find_and_replace) {
    if (tld_search_state.find_what.str == 0 ||
        tld_search_state.replace_with.str == 0)
    {
        tld_search_state.find_what = make_fixed_width_string(tld_search_find_space);
        tld_search_state.replace_with = make_fixed_width_string(tld_search_replace_space);
    }
    
    tld_find_and_replace_interactive(app, &tld_search_state, 0, 0);
}

CUSTOM_COMMAND_SIG(tld_find_and_replace_selection) {
    tld_search_state = {0};
    tld_search_state.find_what = make_fixed_width_string(tld_search_find_space);
    tld_search_state.replace_with = make_fixed_width_string(tld_search_replace_space);
    
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    int32_t cursor = view.cursor.pos;
    int32_t mark = view.mark.pos;
    
    Range range = {0};
    range.min = (cursor < mark) ? cursor : mark;
    range.max = (cursor > mark) ? cursor : mark;
    
    if (range.max - range.min > ArrayCount(tld_search_find_space)) {
        range.max = range.min + ArrayCount(tld_search_find_space);
    }
    
    buffer_read_range(app, &buffer, range.min, range.max, tld_search_find_space);
    tld_search_state.find_what.size = range.max - range.min;
    
    tld_find_and_replace_interactive(app, &tld_search_state, 0, 0);
}

CUSTOM_COMMAND_SIG(tld_find_and_replace_in_range) {
    if (tld_search_state.find_what.str == 0 ||
        tld_search_state.replace_with.str == 0)
    {
        tld_search_state.find_what = make_fixed_width_string(tld_search_find_space);
        tld_search_state.replace_with = make_fixed_width_string(tld_search_replace_space);
    }
    
    View_Summary view = get_active_view(app, AccessAll);
    tld_find_and_replace_interactive(app, &tld_search_state, view.cursor.pos, view.mark.pos);
}