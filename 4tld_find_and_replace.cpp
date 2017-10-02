/******************************************************************************
Author: Tristan Dannenberg
Notice: No warranty is offered or imlied; use this code at your own risk.
*******************************************************************************
LICENSE
This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.
******************************************************************************/

static inline void
tldfr_update_highlight(Application_Links *app, View_Summary *view, Buffer_Summary *buffer,
                       bool32 backwards, Search_Match *match, bool32 search_attempt,
                       Query_Bar *error_bar)
{
    if (match->found_match) {
        view_set_highlight(app, view, match->start, match->end, 1);
    } else {
        if (backwards) {
            view_set_highlight(app, view, 0, 0, 1);
        } else {
            view_set_highlight(app, view, buffer->size, buffer->size, 1);
        }
        
        if (search_attempt) {
            error_bar->prompt = make_lit_string("No match found! ");
            error_bar->string = make_lit_string("Press any key to continue.");
            
            start_query_bar(app, error_bar, 0);
        }
    }
}

static void
tldfr_list_all_matches(Application_Links *app, Buffer_Summary *list_buffer, Partition *part,
                       Search_Set *set, Search_Iter *iter)
{
    buffer_replace_range(app, list_buffer, 0, list_buffer->size, 0, 0);
    
    Search_Match match = search_next_match(app, set, iter);
    if (!match.found_match) {
        tldui_print_text(app, list_buffer, literal("No matches found!\n"));
        return;
    }
    
    tldui_print_text(app, list_buffer,
                     match.buffer.buffer_name,
                     match.buffer.buffer_name_len);
    tldui_print_text(app, list_buffer, literal(":\n"));
    
    Partition line_part = partition_sub_part(part, (4 << 10));
    
    Buffer_ID current_buffer_id = match.buffer.buffer_id;
    int32_t last_line = 0;
    
    do {
        Partial_Cursor word_pos = {0};
        if (match.found_match && buffer_compute_cursor(
            app, &match.buffer, seek_pos(match.start), &word_pos))
        {
            if (match.buffer.buffer_id != current_buffer_id) {
                tldui_print_text(app, list_buffer, literal("\n"));
                if (match.buffer.file_name) {
                    tldui_print_text(app, list_buffer,
                                     match.buffer.file_name,
                                     match.buffer.file_name_len);
                } else {
                    tldui_print_text(app, list_buffer,
                                     match.buffer.buffer_name,
                                     match.buffer.buffer_name_len);
                }
                tldui_print_text(app, list_buffer, literal(":\n"));
                
                current_buffer_id = match.buffer.buffer_id;
                last_line = 0;
            }
            
            // TODO: Print variable number of context lines
            if (word_pos.line != last_line) {
                last_line = word_pos.line;
                
                // TODO: Audit the way we manage memory here, this is a pretty spotty cut'n'paste...
                int32_t line_num_len = int_to_str_size(word_pos.line);
                Temp_Memory line_temp = begin_temp_memory(&line_part);
                String line_str = {0};
                
                read_line(app, &line_part, &match.buffer, word_pos.line, &line_str);
                line_str = skip_chop_whitespace(line_str);
                
                int32_t str_len = line_num_len + 2 + line_str.size + 1;
                char *spare = push_array(part, char, str_len);
                Assert(spare);
                
                String out_line = make_string_cap(spare, 0, str_len);
                append_int_to_str(&out_line, word_pos.line);
                append_s_char(&out_line, ':');
                append_s_char(&out_line, ' ');
                append_ss(&out_line, line_str);
                append_s_char(&out_line, '\n');
                tldui_print_text(app, list_buffer, expand_str(out_line));
                
                end_temp_memory(line_temp);
            }
            
            // TODO: Build up a set of ranges that can be used for UI
            // TODO: Use buffer markers to make search results persistently useful
        }
        
        match = search_next_match(app, set, iter);
    } while (match.found_match);
}

static void
search_range_init(Search_Range *out, Buffer_Summary *buffer,
                  bool32 match_word, bool32 match_case)
{
    uint32_t match_flags = 0;
    if (!match_word) match_flags |= SearchFlag_MatchSubstring;
    if (!match_case) match_flags |= SearchFlag_CaseInsensitive;
    
    out->type = SearchRange_FrontToBack;
    out->flags = match_flags;
    out->buffer = buffer->buffer_id;
    out->start = 0;
    out->size = buffer->size;
}

enum tldfr_search_state {
    TldSearchState_Iteration,
    TldSearchState_SearchKeyInput,
    TldSearchState_ReplaceInput,
};

struct tldfr_search {
    String find_string;
    String replace_string;
    
    bool32 match_word;
    bool32 match_case;
    bool32 wrap_around;
    bool32 backwards;
};

struct tldfr_ui_state {
    Range find_string_box;
    Range repl_string_box;
    
    Range match_word_checkbox;
    Range match_case_checkbox;
    Range wrap_around_checkbox;
};


static inline tldfr_ui_state
tldfr_print_search_ui(Application_Links *app, Buffer_Summary *buffer, tldfr_search *search)
{
    tldfr_ui_state result = {0};
    
    tldui_print_text(app, buffer, literal("(f)    Find what : "));
    result.find_string_box = tldui_print_dynamic_text(app, buffer, search->find_string);
    
    tldui_print_text(app, buffer, literal("\n(h) Replace with : "));
    result.repl_string_box = tldui_print_dynamic_text(app, buffer, search->replace_string);
    
    tldui_print_text(app, buffer, literal("\n\n "));
    result.match_word_checkbox = tldui_print_checkbox(
        app, buffer, literal("Match (w)hole word only"), search->match_word);
    
    tldui_print_text(app, buffer, literal("\n "));
    result.match_case_checkbox = tldui_print_checkbox(
        app, buffer, literal("Match (c)ase"), search->match_case);
    
    tldui_print_text(app, buffer, literal("\n "));
    result.wrap_around_checkbox = tldui_print_checkbox(
        app, buffer, literal("Wra(p) around"), search->wrap_around);
    
    tldui_print_text(app, buffer, literal("\n\n(i) Search Up     | (I) Goto first match  | (r) Replace      | (a) List matches\n"));
    tldui_print_text(app, buffer, literal("(k) Search Down   | (K) Goto last match   | (R) Replace all  | (A) List matches in all buffers\n"));
    
    return result;
}

// TODO: Make this respect search->match_word
static Search_Match
tld_find_string(Application_Links *app,
                Buffer_Summary *buffer,
                int32_t *search_pos,
                tldfr_search *search)
{
    Search_Match result = {0};
    if (search->find_string.size == 0) return result;
    
    void (*tld_seek_string)(Application_Links*, Buffer_Summary*,
                            int32_t, int32_t, char*, int32_t, int32_t*);
    
    if (search->backwards) {
        if (search->match_case) {
            tld_seek_string = buffer_seek_string_backward;
        } else {
            tld_seek_string = buffer_seek_string_insensitive_backward;
        }
        
        tld_seek_string(app, buffer, *search_pos, 0, expand_str(search->find_string), search_pos);
        if ((*search_pos) < 0) {
            if (search->wrap_around) {
                tld_seek_string(
                    app, buffer, buffer->size - search->find_string.size, 0, expand_str(search->find_string), search_pos);
                if ((*search_pos) < 0) {
                    return result;
                }
            } else {
                return result;
            }
        }
    } else {
        if (search->match_case) {
            tld_seek_string = buffer_seek_string_forward;
        } else {
            tld_seek_string = buffer_seek_string_insensitive_forward;
        }
        
        tld_seek_string(app, buffer, *search_pos, buffer->size, expand_str(search->find_string), search_pos);
        if ((*search_pos) >= buffer->size) {
            if (search->wrap_around) {
                tld_seek_string(app, buffer, 0, buffer->size, expand_str(search->find_string), search_pos);
                if ((*search_pos) >= buffer->size) {
                    return result;
                }
            } else {
                return result;
            }
        }
    }
    
    result.start = *search_pos;
    result.end = result.start + search->find_string.size;
    result.found_match = true;
    
    return result;
}

static void
tldfr_interactive_search(Application_Links *app,
                         View_Summary *ui_view, Buffer_Summary *ui_buffer,
                         View_Summary *target_view, Buffer_Summary *target_buffer,
                         tldfr_search *search, tldfr_search_state initial_state,
                         int32_t * search_pos)
{
    tldfr_search_state state = initial_state;
    
    Query_Bar error_bar = {0};
    tldfr_ui_state ui = tldfr_print_search_ui(app, ui_buffer, search);
    
    Search_Match match = {0};
    
    int32_t search_pos_internal = target_view->cursor.pos;
    if (search_pos == 0) {
        search_pos = &search_pos_internal;
    }
    
    bool attempted_search = false;
    
    while (true) {
        if (state == TldSearchState_Iteration) {
            view_set_mark(app, ui_view, seek_pos(ui_buffer->size));
            view_set_cursor(app, ui_view, seek_pos(ui_buffer->size), false);
        } else if (state == TldSearchState_SearchKeyInput) {
            view_set_mark(app, ui_view, seek_pos(ui.find_string_box.min));
            view_set_cursor(app, ui_view, seek_pos(ui.find_string_box.max), false);
        } else if (state == TldSearchState_ReplaceInput) {
            view_set_mark(app, ui_view, seek_pos(ui.repl_string_box.min));
            view_set_cursor(app, ui_view, seek_pos(ui.repl_string_box.max), false);
        }
        
        // TODO: Add flags for mouse input once that does not crash anymore
        User_Input in = get_user_input(
            app, EventOnAnyKey,
            EventOnEsc);
        
        if (error_bar.string.str) {
            end_query_bar(app, &error_bar, 0);
            error_bar = {0};
        }
        
        if (in.abort) {
            close_view(app, ui_view);
            view_set_highlight(app, target_view, 0, 0, 0);
            break;
        } else if (in.type == UserInputMouse) {
            if (in.mouse.release_l && !in.mouse.out_of_window) {
                float rx = 0, ry = 0;
                if (global_point_to_view_point(target_view, in.mouse.x, in.mouse.y, &rx, &ry)) {
                    Full_Cursor click_pos = {0};
                    if (view_compute_cursor(app, ui_view,
                                            seek_xy(rx, ry, false, true),
                                            &click_pos))
                    {
                        int32_t pos = click_pos.pos;
                        if (pos >= ui.find_string_box.min && pos <= ui.find_string_box.max) {
                            state = TldSearchState_SearchKeyInput;
                        } else if (pos >= ui.find_string_box.min && pos <= ui.find_string_box.max) {
                            state = TldSearchState_ReplaceInput;
                        } else if (pos >= ui.match_case_checkbox.min &&
                                   pos <= ui.match_case_checkbox.max)
                        {
                            tldui_toggle_checkbox(
                                app, ui_buffer, ui.match_case_checkbox, &search->match_case);
                        } else if (pos >= ui.match_word_checkbox.min &&
                                   pos <= ui.match_word_checkbox.max)
                        {
                            tldui_toggle_checkbox(
                                app, ui_buffer, ui.match_word_checkbox, &search->match_word);
                        } else if (pos >= ui.wrap_around_checkbox.min &&
                                   pos <= ui.wrap_around_checkbox.max)
                        {
                            tldui_toggle_checkbox(
                                app, ui_buffer, ui.wrap_around_checkbox, &search->wrap_around);
                        }
                    }
                }
            }
            
            continue;
        } else if (in.type != UserInputKey) {
            continue;
        }
        
        switch (state) {
            case TldSearchState_Iteration: {
                if (in.key.keycode == 'f') {
                    state = TldSearchState_SearchKeyInput;
                } else if (in.key.keycode == 'h') {
                    state = TldSearchState_ReplaceInput;
                } else if (in.key.keycode == 'c') {
                    tldui_toggle_checkbox(app, ui_buffer,
                                          ui.match_case_checkbox,
                                          &search->match_case);
                } else if (in.key.keycode == 'w') {
                    tldui_toggle_checkbox(app, ui_buffer,
                                          ui.match_word_checkbox,
                                          &search->match_word);
                } else if (in.key.keycode == 'p') {
                    tldui_toggle_checkbox(app, ui_buffer,
                                          ui.wrap_around_checkbox,
                                          &search->wrap_around);
                } else if (in.key.keycode == 'i') {
                    *search_pos -= 1;
                    search->backwards = true;
                    match = tld_find_string(app, target_buffer, search_pos, search);
                    attempted_search = true;
                } else if (in.key.keycode == 'I') {
                    *search_pos = 0;
                    search->backwards = false;
                    match = tld_find_string(app, target_buffer, search_pos, search);
                    attempted_search = true;
                } else if (in.key.keycode == 'k') {
                    *search_pos += 1;
                    search->backwards = false;
                    match = tld_find_string(app, target_buffer, search_pos, search);
                    attempted_search = true;
                } else if (in.key.keycode == 'K') {
                    *search_pos = target_buffer->size - search->find_string.size;
                    search->backwards = true;
                    match = tld_find_string(app, target_buffer, search_pos, search);
                    attempted_search = true;
                } else if (in.key.keycode == 'r' && match.found_match) {
                    buffer_replace_range(app, target_buffer, match.start, match.end,
                                         expand_str(search->replace_string));
                    if (search->backwards) {
                        *search_pos -= search->find_string.size;
                        if (*search_pos < 0) {
                            match = {0};
                        }
                    } else {
                        *search_pos += search->replace_string.size;
                        if (*search_pos >= target_buffer->size - search->find_string.size) {
                            match = {0};
                        }
                    }
                    
                    if (match.found_match) {
                        match = tld_find_string(app, target_buffer, search_pos, search);
                        attempted_search = true;
                    }
                } else if (in.key.keycode == 'R' && search->find_string.size) {
                    *search_pos = target_buffer->size - search->find_string.size;
                    search->backwards = true;
                    while (*search_pos >= 0) {
                        match = tld_find_string(app, target_buffer, search_pos, search);
                        if (match.found_match) {
                            buffer_replace_range(app, target_buffer, match.start, match.end,
                                                 expand_str(search->replace_string));
                        } else {
                            break;
                        }
                        
                        *search_pos -= search->find_string.size;
                    }
                    
                    match = {0};
                } else if (in.key.keycode == 'a') {
                    Search_Iter iter = {0};
                    search_iter_init(&global_general, &iter, search->find_string.size);
                    copy_ss(&iter.word, search->find_string);
                    
                    Search_Set set = {0};
                    search_set_init(&global_general, &set, 1);
                    
                    search_range_init(&set.ranges[0], target_buffer,
                                      search->match_word, search->match_case);
                    set.count = 1;
                    kill_buffer(app, buffer_identifier(ui_buffer->buffer_id),
                                0, BufferKill_AlwaysKill);
                    close_view(app, ui_view);
                    *ui_buffer = tldui_get_empty_buffer_by_name(
                        app, literal("*search-results*"), true, true, AccessAll);
                    tldfr_list_all_matches(app, ui_buffer, &global_part, &set, &iter);
                    tldui_display_buffer(app, ui_buffer->buffer_id, true);
                    return;
                } else if (in.key.keycode == 'A') {
                    Search_Iter iter = {0};
                    search_iter_init(&global_general, &iter, search->find_string.size);
                    copy_ss(&iter.word, search->find_string);
                    
                    Search_Set set = {0};
                    search_set_init(&global_general, &set, get_buffer_count(app));
                    
                    search_range_init(&set.ranges[0], target_buffer,
                                      search->match_word, search->match_case);
                    int32_t j = 1;
                    for (Buffer_Summary next_buffer = get_buffer_first(app, AccessAll);
                         next_buffer.exists; get_buffer_next(app, &next_buffer, AccessAll))
                    {
                        if (next_buffer.buffer_name[0] == '*' ||
                            next_buffer.buffer_id == target_buffer->buffer_id)
                        {
                            continue;
                        }
                        
                        search_range_init(&set.ranges[j], &next_buffer,
                                          search->match_word, search->match_case);
                        j += 1;
                    }
                    
                    set.count = j;
                    kill_buffer(app, buffer_identifier(ui_buffer->buffer_id),
                                0, BufferKill_AlwaysKill);
                    close_view(app, ui_view);
                    *ui_buffer = tldui_get_empty_buffer_by_name(
                        app, literal("*search-results*"), true, true, AccessAll);
                    tldfr_list_all_matches(app, ui_buffer, &global_part, &set, &iter);
                    tldui_display_buffer(app, ui_buffer->buffer_id, true);
                    return;
                }
            } break;
            case TldSearchState_SearchKeyInput: {
                if (in.key.keycode == key_back) {
                    backspace_utf8(&search->find_string);
                } else if (in.key.keycode == key_del) {
                    search->find_string.size = 0;
                } else if (in.key.keycode == 'v' && in.key.modifiers[MDFR_ALT]) {
                    tldui_append_s_clipboad(app, &search->find_string);
                } else if (in.key.keycode == '\n') {
                    state = TldSearchState_Iteration;
                    continue;
                } else if (in.key.keycode == '\t') {
                    state = TldSearchState_ReplaceInput;
                    continue;
                } else if (key_is_unmodified(&in.key)) {
                    tldui_append_s_input(&search->find_string, in);
                }
                
                attempted_search = true;
                tldui_update_dynamic_text(
                    app, ui_buffer, search->find_string, ui.find_string_box);
                
                if (search->find_string.size) {
                    match = tld_find_string(app, target_buffer, search_pos, search);
                } else {
                    match.end = match.start;
                }
            } break;
            case TldSearchState_ReplaceInput: {
                if (in.key.keycode == key_back) {
                    backspace_utf8(&search->replace_string);
                } else if (in.key.keycode == key_del) {
                    search->replace_string.size = 0;
                } else if (in.key.keycode == 'v' && in.key.modifiers[MDFR_ALT]) {
                    tldui_append_s_clipboad(app, &search->replace_string);
                } else if (in.key.keycode == '\n') {
                    state = TldSearchState_Iteration;
                    continue;
                } else if (in.key.keycode == '\t') {
                    state = TldSearchState_SearchKeyInput;
                    continue;
                } else if (key_is_unmodified(&in.key)) {
                    tldui_append_s_input(&search->replace_string, in);
                }
                
                tldui_update_dynamic_text(
                    app, ui_buffer, search->replace_string, ui.repl_string_box);
            } break;
            default: {
                Assert(false); // Unreachable
            }
        }
        
        tldfr_update_highlight(
            app, target_view, target_buffer,
            search->backwards, &match,
            attempted_search, &error_bar);
    }
}

CUSTOM_COMMAND_SIG(tld_find_and_replace) {
    View_Summary target_view = get_active_view(app, AccessAll);
    Buffer_Summary target_buffer = get_buffer(app, target_view.buffer_id, AccessAll);
    
    int32_t start_pos = target_view.cursor.pos;
    
    Buffer_Summary ui_buffer = tldui_get_empty_buffer_by_name(
        app, literal("*search*"), true, true, AccessAll);
    View_Summary ui_view = tldui_display_buffer_temp(
        app, &target_view, 0.25f, ViewSplit_Bottom, false, ui_buffer.buffer_id);
    view_set_highlight(app, &ui_view, 0, 0, 1);
    set_active_view(app, &target_view);
    
    char find_string_space[512];
    char repl_string_space[512];
    
    tldfr_search search = {0};
    search.find_string = make_fixed_width_string(find_string_space);
    search.replace_string = make_fixed_width_string(repl_string_space);
    search.wrap_around = true;
    
    tldfr_interactive_search(app, &ui_view, &ui_buffer,
                             &target_view, &target_buffer, &search,
                             TldSearchState_SearchKeyInput, &start_pos);
}

// TODO: Rethink how commands should be structured
// I like the idea of having Alt-F go to either the *search-results* or the *search* buffer,
// depending on how the last search was exited, though that additional state may be unwanted.
// Maybe we'll leave the *search-results* view open, but have our switch_or_create_view command
// recognize it? (Maybe that just needs an overhaul as well and go into tldui afterwards?)

CUSTOM_COMMAND_SIG(tld_find_and_replace_selection) {}
CUSTOM_COMMAND_SIG(tld_find_and_replace_in_range) {}
