/******************************************************************************
File: 4tld_find_and_replace.cpp
Author: Tristan Dannenberg
Notice: No warranty is offered or imlied; use this code at your own risk.
*******************************************************************************
TODO(Polish): This is pretty nice, but there are some minor problems with it,
that may get annoying after a while, and should be fixed before packaging and
sharing this code:
- I would like the ability to paste into the query bar from the clipboard.
  Should be easy to implement within tld_requery_user_string, but I would like
  all query bars to behave the same, so maybe I should look into helping Allen
  rewrite the existing code there, so that everyone can benefit from it, and I
  don't have to be careful in replacing it after every update.
- I would like to be able to search for special characters, but
  tld_requery_user_string does not allow entering them. We can already copy
  them with interactive_find_and_replace_selection, but the query_bar doesn't
  render them right anyway. Having a (visible!) cursor would also be great.
- We're potentially leaving optimizations on the table by not utilizing the
  existing functionality in 4coder_search.cpp -- this isn't documented very
  well, and it seems to be working fine as is, but I should look into that
  before sharing the code.
- Some more nice-to-have features:
  - toggle case sensitivity (consider: emacs style, where search-keys
    containing capitals are case sensitive, others are not)
  - regular expressions, or at least some kind of placeholders
    If RegEx, consider a smart replace that can utilize info from the match to
    generate a replacement from another regex (there's probably some cool CS
    theory stuff for that)
  - others?
******************************************************************************/
#include "4tld_user_interface.h"

void
tld_interactive_find_and_replace(Application_Links *app,
                                 Query_Bar *find_bar,
                                 Query_Bar *replace_bar,
                                 View_Summary *view,
                                 bool show_hints)
{
    int32_t query_bar_count = 3;
    
    if (show_hints) {
        query_bar_count += 2;
        
        Query_Bar hints2;
        hints2.prompt = make_lit_string("F: Find previous -- R: Replace all -- A: search All buffers");
        hints2.string = make_lit_string("");
        start_query_bar(app, &hints2, 0);
        
        Query_Bar hints1;
        hints1.prompt = make_lit_string("f: Find next     -- r: Replace");
        hints1.string = make_lit_string("");
        start_query_bar(app, &hints1, 0);
    }
    
    replace_bar->prompt = make_lit_string("[M-r] Replace with: ");
    start_query_bar(app, replace_bar, 0);
    
    find_bar->prompt = make_lit_string("[M-f] Find what: ");
    start_query_bar(app, find_bar, 0);
    
    Buffer_Summary buffer = get_buffer(app, view->buffer_id, AccessAll);
    if (!buffer.exists) return;
    
    // TODO(On Update): This is done to make the text in the first couple lines visible
    // If something to this effect ever makes it into default 4coder, remove this hack!
    for (int i = 0; i < query_bar_count; ++i) {
        buffer_replace_range(app, &buffer, 0, 0, literal("\n"));
    }
    
    if (find_bar->string.size == 0) {
        tld_requery_user_string(app, find_bar);
    }
    
    Range match = make_range(0, 0);
    
    int pos = view->cursor.pos;
    bool search_backwards = false;
    int new_pos;
    
    User_Input in;
    while (true) {
        in = get_user_input(app, EventOnAnyKey, EventOnButton);
        if (in.abort || in.key.keycode == key_esc || in.key.keycode == 0) {
            goto end_search;
        }
        if (find_bar->string.size == 0 &&
            in.key.keycode != 'f' &&
            in.key.modifiers[MDFR_ALT_INDEX] == 0)
        {
            continue;
        }
        
        switch (in.key.keycode) {
            case 'f': next_match: {
                if (in.key.modifiers[MDFR_ALT_INDEX]) {
                    find_bar->string.size = 0;
                    tld_requery_user_string(app, find_bar);
                    continue;
                } else {
                    search_backwards = false;
                    if (match.max - match.min > 0) {
                        pos = match.max + 1;
                        if (pos >= buffer.size) {
                            pos = query_bar_count;
                        }
                    }
                    
                    buffer_seek_string_forward(app, &buffer, pos, 0, find_bar->string.str, find_bar->string.size, &new_pos);
                    if (new_pos >= buffer.size) {
                        buffer_seek_string_forward(app, &buffer, query_bar_count, 0, find_bar->string.str, find_bar->string.size, &new_pos);
                        
                        if (new_pos >= buffer.size) {
                            match.min = 0;
                            match.max = 0;
                            break;
                        }
                    }
                    
                    match.min = new_pos;
                    match.max = match.min + find_bar->string.size;
                }
            } break;
            case 'F': previous_match: {
                search_backwards = true;
                if (match.max - match.min > 0) {
                    pos = match.min - 1;
                    if (pos < query_bar_count) {
                        pos = buffer.size;
                    }
                }
                
                buffer_seek_string_backward(app, &buffer, pos, query_bar_count, find_bar->string.str, find_bar->string.size, &new_pos);
                if (new_pos < query_bar_count) {
                    buffer_seek_string_backward(app, &buffer, buffer.size - find_bar->string.size, query_bar_count, find_bar->string.str, find_bar->string.size, &new_pos);
                    if (new_pos < query_bar_count) {
                        match.min = 0;
                        match.max = 0;
                        break;
                    }
                }
                
                match.min = new_pos;
                match.max = match.min + find_bar->string.size;
            } break;
            case 'r': {
                if (buffer.lock_flags & AccessProtected) {
                    continue;
                }
                
                if (in.key.modifiers[MDFR_ALT_INDEX]) {
                    replace_bar->string.size = 0;
                    tld_requery_user_string(app, replace_bar);
                    continue;
                } else if (match.max - match.min > 0) {
                    buffer_replace_range(app, &buffer, match.min, match.max, replace_bar->string.str, replace_bar->string.size);
                    buffer = get_buffer(app, view->buffer_id, AccessAll);
                    
                    if (search_backwards) {
                        goto previous_match;
                    } else {
                        goto next_match;
                    }
                }
            } break;
            case 'R': {
                if (buffer.lock_flags & AccessProtected) {
                    continue;
                }
                
                new_pos = query_bar_count;
                while (new_pos < buffer.size) {
                    buffer_seek_string_forward(app, &buffer, new_pos, 0, find_bar->string.str, find_bar->string.size, &new_pos);
                    if (new_pos < buffer.size) {
                        buffer_replace_range(app, &buffer, new_pos, new_pos + replace_bar->string.size, replace_bar->string.str, replace_bar->string.size);
                        buffer = get_buffer(app, view->buffer_id, AccessAll);
                    }
                }
                
                match.min = 0;
                match.max = 0;
            } break;
            case 'A': {
                generic_search_all_buffers(app, &global_general, &global_part, find_bar->string,
                                           SearchFlag_MatchWholeWord);
                goto end_search;
            } break;
        }
        
        if (match.max - match.min > 0) {
            view_set_highlight(app, view, match.min, match.max, true);
            exec_command(app, center_view);
        } else {
            view_set_highlight(app, view, 0, 0, false);
        }
    }
    
    end_search: {
        view_set_highlight(app, view, 0, 0, false);
        if (match.max - match.min > 0) {
            view_set_mark(app, view, seek_pos(match.min));
            view_set_cursor(app, view, seek_pos(match.max), false);
        }
        
        buffer_replace_range(app, &buffer, 0, query_bar_count, literal(""));
    }
}

#ifdef TLDFR_IMPLEMENT_COMMMANDS
#undef TLDFR_IMPLEMENT_COMMMANDS

char tld_find_bar_space[512];
char tld_replace_bar_space[512];

CUSTOM_COMMAND_SIG(tld_interactive_find_and_replace_new) {
    View_Summary view = get_active_view(app, AccessAll);
    
    Query_Bar find_bar;
    find_bar.string = make_fixed_width_string(tld_find_bar_space);
    
    Query_Bar replace_bar;
    replace_bar.string = make_fixed_width_string(tld_replace_bar_space);
    
    tld_interactive_find_and_replace(app, &find_bar, &replace_bar, &view, true);
}

CUSTOM_COMMAND_SIG(tld_interactive_find_and_replace_continued) {
    View_Summary view = get_active_view(app, AccessAll);
    
    Query_Bar find_bar;
    find_bar.string = make_string_slowly(tld_find_bar_space);
    find_bar.string.memory_size = sizeof(tld_find_bar_space);
    
    Query_Bar replace_bar;
    replace_bar.string = make_string_slowly(tld_replace_bar_space);
    replace_bar.string.memory_size = sizeof(tld_replace_bar_space);
    
    tld_interactive_find_and_replace(app, &find_bar, &replace_bar, &view, true);
}

CUSTOM_COMMAND_SIG(tld_interactive_find_and_replace_selection) {
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    Query_Bar find_bar;
    find_bar.string.str = tld_find_bar_space;
    
    Query_Bar replace_bar;
    replace_bar.string = make_string_slowly(tld_replace_bar_space);
    replace_bar.string.memory_size = sizeof(tld_replace_bar_space);
    
    int cursor_pos = view.cursor.pos;
    int mark_pos = view.mark.pos;
    
    Range selection = make_range(0, 0);
    
    if (cursor_pos < mark_pos) {
        selection.min = cursor_pos;
        selection.max = mark_pos;
    } else if (cursor_pos > mark_pos) {
        selection.min = mark_pos;
        selection.max = cursor_pos;
    }
    
    if (selection.min == selection.max ||
        selection.max - selection.min >= sizeof(tld_find_bar_space) - 1)
    {
        find_bar.string = make_string_slowly(tld_find_bar_space);
    } else {
        buffer_read_range(app, &buffer, selection.min, selection.max, tld_find_bar_space);
        find_bar.string.size = selection.max - selection.min;
        terminate_with_null(&find_bar.string);
    }
    
    find_bar.string.memory_size = sizeof(tld_find_bar_space);
    tld_interactive_find_and_replace(app, &find_bar, &replace_bar, &view, true);
}

#endif