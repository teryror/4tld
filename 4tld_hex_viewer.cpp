/******************************************************************************
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
*******************************************************************************
LICENSE

This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.
*******************************************************************************
This file implements a hex-editor-like view on top of a 4coder buffer.

If you use the provided commands, you need to call tld_hex_view_bind_map(2) in
your get_bindings function to setup the command map for *hex* buffers.

Provided Commands:
* tld_hex_view_current_buffer
* tld_hex_view_goto
* tld_hex_view_move_up
* tld_hex_view_move_left
* tld_hex_view_move_down
* tld_hex_view_move_right
* tld_hex_view_move_to_beginning_of_line
* tld_hex_view_move_to_end_of_line
* tld_hex_view_exit

Note that only the first command in this list is intended for use in a
non-*hex* buffer.
******************************************************************************/

// TODO: Actual hex _editing_ commands

struct tld_HexViewState {
    bool32 slot_used;
    Buffer_ID src_buffer_id;
    Buffer_ID hex_buffer_id;
    uint32_t window_start_offset;
    uint32_t current_offset;
};

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef TLD_HEX_VIEW_WINDOW_SIZE
#define TLD_HEX_VIEW_WINDOW_SIZE (48 * 16)
#endif

// Hex printing functions

static bool32
tld_hex_view_print_byte(String * out, uint8_t byte) {
    if (out->memory_size - out->size < 2) return false;
    const char nibbles[] = "0123456789abcdef";
    
    out->str[out->size + 0] = nibbles[byte >> 4];
    out->str[out->size + 1] = nibbles[byte & 0xF];
    
    out->size += 2;
    return true;
}

static bool32
tld_hex_view_print_word(String * out, uint32_t word) {
    if (out->memory_size - out->size < 8) return false;
    const char nibbles[] = "0123456789abcdef";
    
    out->str[out->size + 0] = nibbles[(word >> 28) & 0xF];
    out->str[out->size + 1] = nibbles[(word >> 24) & 0xF];
    out->str[out->size + 2] = nibbles[(word >> 20) & 0xF];
    out->str[out->size + 3] = nibbles[(word >> 16) & 0xF];
    out->str[out->size + 4] = nibbles[(word >> 12) & 0xF];
    out->str[out->size + 5] = nibbles[(word >>  8) & 0xF];
    out->str[out->size + 6] = nibbles[(word >>  4) & 0xF];
    out->str[out->size + 7] = nibbles[(word >>  0) & 0xF];
    
    out->size += 8;
    return true;
}

// Clear the hex_buffer, and fill it with the hex view of TLD_HEX_VIEW_WINDOW_SIZE
// bytes around the specified offset.
// Optionally, write the buffer position of the byte at the specified offset into
// the cursor, and the buffer position of its ASCII representation into mark.
static void
tld_hex_view_print(Application_Links *app,
                   tld_HexViewState *state,
                   bool show_ascii,
                   int32_t *cursor, int32_t *mark)
{
    Buffer_Summary hex_buffer = get_buffer(app, state->hex_buffer_id, AccessAll);
    Buffer_Summary src_buffer = get_buffer(app, state->src_buffer_id, AccessAll);
    
    char line_space[81];
    String line = make_fixed_width_string(line_space);
    
    buffer_replace_range(app, &hex_buffer, 0, hex_buffer.size, 0, 0);
    
    uint8_t window[TLD_HEX_VIEW_WINDOW_SIZE];
    
    if (src_buffer.size > 0 && state->current_offset >= (uint32_t) src_buffer.size) {
        state->current_offset = src_buffer.size - 1;
    }
    
    if (state->current_offset < 16) {
        state->window_start_offset = 0;
    } else if ((state->current_offset & 0xFFFFFFF0) == state->window_start_offset) {
        state->window_start_offset = (state->current_offset & 0xFFFFFFF0) - 16;
    } else if ((sizeof(window) - 32) <=
               (state->current_offset & 0xFFFFFFF0) - state->window_start_offset)
    {
        state->window_start_offset = (state->current_offset & 0xFFFFFF0) - (sizeof(window) - 32);
    }
    uint32_t window_start_offset = state->window_start_offset;
    
    uint32_t window_end_offset;
    if (src_buffer.size - window_start_offset < sizeof(window)) {
        window_end_offset = src_buffer.size;
    } else {
        window_end_offset = window_start_offset + sizeof(window);
    }
    
    Assert(buffer_read_range(app, &src_buffer,
                             (int32_t)(window_start_offset & 0x7FFFFFFF),
                             (int32_t)(window_end_offset   & 0x7FFFFFFF),
                             (char  *)(&window)));
    
    uint32_t window_size = window_end_offset - window_start_offset;
    
    for (uint32_t i = 0; i < window_size; i += 16) {
        line.size = 0;
        
        append_s_char(&line, ' ');
        append_s_char(&line, ' ');
        tld_hex_view_print_word(&line, window_start_offset + i);
        append_s_char(&line, ' ');
        append_s_char(&line, ' ');
        
        for (uint32_t j = i; j < min(i + 8, window_size); ++j) {
            if (cursor && window_start_offset + j == state->current_offset) {
                *cursor = hex_buffer.size + line.size;
            }
            
            tld_hex_view_print_byte(&line, window[j]);
            append_s_char(&line, ' ');
        }
        append_s_char(&line, ' ');
        for (uint32_t j = i + 8; j < min(i + 16, window_size); ++j) {
            if (cursor && window_start_offset + j == state->current_offset) {
                *cursor = hex_buffer.size + line.size;
            }
            
            tld_hex_view_print_byte(&line, window[j]);
            append_s_char(&line, ' ');
        }
        if (show_ascii) {
            append_padding(&line, ' ', 62);
            
            for (uint32_t j = i; j < min(i + 16, window_size); ++j) {
                if (mark && window_start_offset + j == state->current_offset) {
                    *mark = hex_buffer.size + line.size;
                }
                
                if (window[j] < 0x20) {
                    append_s_char(&line, '\\');
                } else if (window[j] >= 0x80) {
                    append_s_char(&line, '.');
                } else {
                    append_s_char(&line, (char) window[j]);
                }
            }
        }
        
        append_padding(&line, ' ', 80);
        Assert(append_s_char(&line, '\n'));
        
        buffer_replace_range(app, &hex_buffer,
                             hex_buffer.size,
                             hex_buffer.size,
                             expand_str(line));
    }
}

// Custom allocator for tld_HexViewStates

static int32_t tld_hex_view_mapid = 0;

#ifndef TLD_HEX_VIEW_STATE_POOL_SIZE
#define TLD_HEX_VIEW_STATE_POOL_SIZE 16
#endif

static tld_HexViewState tld_hex_view_state_pool[TLD_HEX_VIEW_STATE_POOL_SIZE] = {0};

static tld_HexViewState *
tld_hex_view_state_make(Application_Links *app, uint32_t offset, Buffer_ID src_buffer_id) {
    for (int i = 0; i < TLD_HEX_VIEW_STATE_POOL_SIZE; ++i) {
        if (!tld_hex_view_state_pool[i].slot_used) {
            Buffer_Summary src_buffer = get_buffer(app, src_buffer_id, AccessAll);
            
            char buffer_name_space[1024];
            String buffer_name = make_fixed_width_string(buffer_name_space);
            append_ss(&buffer_name, make_lit_string("*hex* "));
            append_ss(&buffer_name, make_string(src_buffer.buffer_name, src_buffer.buffer_name_len));
            
            Buffer_Summary hex_buffer = create_buffer(app, expand_str(buffer_name), BufferCreate_AlwaysNew);
            buffer_set_setting(app, &hex_buffer, BufferSetting_MapID, tld_hex_view_mapid);
            buffer_set_setting(app, &hex_buffer, BufferSetting_ReadOnly, 1);
            buffer_set_setting(app, &hex_buffer, BufferSetting_Unimportant, 1);
            
            tld_hex_view_state_pool[i].slot_used = true;
            tld_hex_view_state_pool[i].src_buffer_id = src_buffer_id;
            tld_hex_view_state_pool[i].hex_buffer_id = hex_buffer.buffer_id;
            tld_hex_view_state_pool[i].current_offset = offset;
            tld_hex_view_state_pool[i].window_start_offset = (offset & 0xFFFFFFF0) - TLD_HEX_VIEW_WINDOW_SIZE / 2;
            
            return &tld_hex_view_state_pool[i];
        }
    }
    
    return 0;
}

static tld_HexViewState *
tld_hex_view_state_find(Buffer_ID hex_buffer_id) {
    for (int i = 0; i < TLD_HEX_VIEW_STATE_POOL_SIZE; ++i) {
        if (tld_hex_view_state_pool[i].slot_used &&
            tld_hex_view_state_pool[i].hex_buffer_id == hex_buffer_id)
        {
            return &tld_hex_view_state_pool[i];
        }
    }
    
    return 0;
}

static void
tld_hex_view_state_destroy(Application_Links *app, View_Summary *view, tld_HexViewState *state) {
    kill_buffer(app, buffer_identifier(state->hex_buffer_id), view->view_id, BufferKill_AlwaysKill);
    *state = {0};
}

// Create a *hex* buffer showing the currently viewed region in the active view
// and display it.
CUSTOM_COMMAND_SIG(tld_hex_view_current_buffer) {
    View_Summary view = get_active_view(app, AccessAll);
    
    int32_t new_cursor_pos = 0;
    int32_t new_mark_pos   = 0;
    
    tld_HexViewState *state = tld_hex_view_state_make(app, view.cursor.pos, view.buffer_id);
    
    if (state) {
        tld_hex_view_print(app, state, true, &new_cursor_pos, &new_mark_pos);
        
        view_set_buffer(app, &view, state->hex_buffer_id, 0);
        view_set_cursor(app, &view, seek_pos(new_cursor_pos), 0);
        view_set_mark(app, &view, seek_pos(new_mark_pos));
    }
}

// Query the user for a byte offset and update the current *hex* buffer to
// show the region around it.
CUSTOM_COMMAND_SIG(tld_hex_view_goto) {
    View_Summary view = get_active_view(app, AccessAll);
    
    char offset_space[256];
    Query_Bar offset_bar = {0};
    offset_bar.prompt = make_lit_string("Goto Offset: ");
    offset_bar.string = make_fixed_width_string(offset_space);
    
    if (query_user_number(app, &offset_bar)) {
        tld_HexViewState *state = tld_hex_view_state_find(view.buffer_id);
        if (state) {
            int32_t new_cursor_pos = 0;
            int32_t new_mark_pos   = 0;
            
            state->current_offset = str_to_int_s(offset_bar.string);
            state->window_start_offset = (state->current_offset & 0xFFFFFFF0) - 
                (TLD_HEX_VIEW_WINDOW_SIZE / 2);
            
            tld_hex_view_print(app, state, true, &new_cursor_pos, &new_mark_pos);
            
            view_set_cursor(app, &view, seek_pos(new_cursor_pos), 0);
            view_set_mark(app, &view, seek_pos(new_mark_pos));
        }
    }
}

// Move both the cursor and mark in the current *hex* buffer by one byte.
CUSTOM_COMMAND_SIG(tld_hex_view_move_left) {
    View_Summary view = get_active_view(app, AccessAll);
    tld_HexViewState *state = tld_hex_view_state_find(view.buffer_id);
    
    if (state && state->current_offset) {
        int32_t new_cursor_pos = 0;
        int32_t new_mark_pos   = 0;
        
        state->current_offset -= 1;
        
        tld_hex_view_print(app, state, true, &new_cursor_pos, &new_mark_pos);
        
        view_set_cursor(app, &view, seek_pos(new_cursor_pos), 0);
        view_set_mark(app, &view, seek_pos(new_mark_pos));
    }
}

// Move both the cursor and mark in the current *hex* buffer by one byte.
CUSTOM_COMMAND_SIG(tld_hex_view_move_right) {
    View_Summary view = get_active_view(app, AccessAll);
    tld_HexViewState *state = tld_hex_view_state_find(view.buffer_id);
    
    if (state) {
        int32_t new_cursor_pos = 0;
        int32_t new_mark_pos   = 0;
        
        state->current_offset += 1;
        
        tld_hex_view_print(app, state, true, &new_cursor_pos, &new_mark_pos);
        
        view_set_cursor(app, &view, seek_pos(new_cursor_pos), 0);
        view_set_mark(app, &view, seek_pos(new_mark_pos));
    }
}

// Move the cursor and mark in the current *hex* buffer to the previous 16 byte aligned offset.
CUSTOM_COMMAND_SIG(tld_hex_view_move_to_beginning_of_line) {
    View_Summary view = get_active_view(app, AccessAll);
    tld_HexViewState *state = tld_hex_view_state_find(view.buffer_id);
    
    if (state) {
        int32_t new_cursor_pos = 0;
        int32_t new_mark_pos   = 0;
        
        state->current_offset &= 0xFFFFFFF0;
        
        tld_hex_view_print(app, state, true, &new_cursor_pos, &new_mark_pos);
        
        view_set_cursor(app, &view, seek_pos(new_cursor_pos), 0);
        view_set_mark(app, &view, seek_pos(new_mark_pos));
    }
}

// Move the cursor and mark in the current *hex* buffer to
// the byte before the next 16 byte aligned offset.
CUSTOM_COMMAND_SIG(tld_hex_view_move_to_end_of_line) {
    View_Summary view = get_active_view(app, AccessAll);
    tld_HexViewState *state = tld_hex_view_state_find(view.buffer_id);
    
    if (state) {
        int32_t new_cursor_pos = 0;
        int32_t new_mark_pos   = 0;
        
        state->current_offset |= 0xF;
        
        tld_hex_view_print(app, state, true, &new_cursor_pos, &new_mark_pos);
        
        view_set_cursor(app, &view, seek_pos(new_cursor_pos), 0);
        view_set_mark(app, &view, seek_pos(new_mark_pos));
    }
}

// Move both the cursor and mark in the current *hex* buffer by 16 bytes.
CUSTOM_COMMAND_SIG(tld_hex_view_move_up) {
    View_Summary view = get_active_view(app, AccessAll);
    tld_HexViewState *state = tld_hex_view_state_find(view.buffer_id);
    
    if (state && state->current_offset >= 16) {
        int32_t new_cursor_pos = 0;
        int32_t new_mark_pos   = 0;
        
        state->current_offset -= 16;
        
        tld_hex_view_print(app, state, true, &new_cursor_pos, &new_mark_pos);
        
        view_set_cursor(app, &view, seek_pos(new_cursor_pos), 0);
        view_set_mark(app, &view, seek_pos(new_mark_pos));
    }
}

// Move both the cursor and mark in the current *hex* buffer by 16 bytes.
CUSTOM_COMMAND_SIG(tld_hex_view_move_down) {
    View_Summary view = get_active_view(app, AccessAll);
    tld_HexViewState *state = tld_hex_view_state_find(view.buffer_id);
    
    if (state) {
        int32_t new_cursor_pos = 0;
        int32_t new_mark_pos   = 0;
        
        state->current_offset += 16;
        
        tld_hex_view_print(app, state, true, &new_cursor_pos, &new_mark_pos);
        
        view_set_cursor(app, &view, seek_pos(new_cursor_pos), 0);
        view_set_mark(app, &view, seek_pos(new_mark_pos));
    }
}

// Kill the current *hex* buffer and show its source buffer again.
CUSTOM_COMMAND_SIG(tld_hex_view_exit) {
    View_Summary view = get_active_view(app, AccessAll);
    tld_HexViewState *state = tld_hex_view_state_find(view.buffer_id);
    
    if (state) {
        Buffer_ID src_buffer_id = state->src_buffer_id;
        uint32_t new_pos = state->current_offset;
        
        tld_hex_view_state_destroy(app, &view, state);
        
        view_set_buffer(app, &view, src_buffer_id, 0);
        view_set_cursor(app, &view, seek_pos(new_pos), 0);
    }
}

// Binds the provided *hex* buffer commands to the specified command map,
// and initializes global state using the specified mapid, allowing
// *hex* buffers created in the future to use it.
static void
tld_hex_view_bind_map(Bind_Helper *context, uint32_t mapid) {
    tld_hex_view_mapid = mapid;
    
    begin_map(context, mapid);
    
    bind(context, 'j', MDFR_NONE, tld_hex_view_move_left);
    bind(context, 'l', MDFR_NONE, tld_hex_view_move_right);
    bind(context, 'i', MDFR_NONE, tld_hex_view_move_up);
    bind(context, 'k', MDFR_NONE, tld_hex_view_move_down);
    bind(context, 'h', MDFR_NONE, tld_hex_view_move_to_beginning_of_line);
    bind(context, ';', MDFR_NONE, tld_hex_view_move_to_end_of_line);
    
    bind(context, 'g', MDFR_NONE, tld_hex_view_goto);
    bind(context, key_esc, MDFR_NONE, tld_hex_view_exit);
    
    end_map(context);
}
