/******************************************************************************
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
*******************************************************************************
LICENSE

This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.
*******************************************************************************
This file hosts the user interface code shared between the command packs.
Note that, because of the nature of this file, most command packs #include it,
making this a strict dependency no matter which one you're using.

Preprocessor Variables:
* TLD_USER_INTERFACE_H is the include guard
* TLDUI_MAX_PATTERN_SIZE is the maximum length the fuzzy matcher supports. This
  defaults to 64, which is the highest valid value. Using other values is not
  recommended, but possible.
* TLDUI_TRANSPOSE_PATTERNS is not defined by default. If this macro is defined,
  tldui_query_fuzzy_list will try a number of transpositions of each string,
  yielding larger result sets and resistance to typos in the pattern, at the
  cost of a noticeable performance drop when querying large lists.
******************************************************************************/
#ifndef TLD_USER_INTERFACE_H
#define TLD_USER_INTERFACE_H

static inline Buffer_Summary
tldui_get_empty_buffer_by_name(Application_Links *app,
                               char * buffer_name, int32_t buffer_name_len,
                               bool32 unimportant, bool32 read_only,
                               Access_Flag access)
{
    Buffer_Summary result = get_buffer_by_name(app, buffer_name, buffer_name_len, access);
    if (!result.exists) {
        result = create_buffer(app, buffer_name, buffer_name_len, BufferCreate_AlwaysNew);
    } else {
        buffer_send_end_signal(app, &result);
        buffer_replace_range(app, &result, 0, result.size, 0, 0);
    }
    
    Assert(result.exists);
    buffer_set_setting(app, &result, BufferSetting_Unimportant, unimportant);
    buffer_set_setting(app, &result, BufferSetting_ReadOnly, read_only);
    
    return result;
}

static inline View_Summary
tldui_display_buffer(Application_Links *app, Buffer_ID buffer, bool prefer_inactive_view) 
{
    int view_count = 0;
    for (View_Summary v = get_view_first(app, AccessAll);
         v.exists; get_view_next(app, &v, AccessAll))
    {
        if (v.buffer_id == buffer) {
            set_active_view(app, &v);
            return v;
        } else {
            view_count += 1;
        }
    }
    
    View_Summary result = get_active_view(app, AccessAll);
    if (view_count > 1) {
        if (prefer_inactive_view) {
            get_view_next(app, &result, AccessAll);
            if (!result.exists) {
                result = get_view_first(app, AccessAll);
            }
            
            set_active_view(app, &result);
        }
    } else if (prefer_inactive_view) {
        open_view(app, &result, ViewSplit_Right);
        view_set_setting(app, &result, ViewSetting_ShowScrollbar, false);
    }
    
    view_set_buffer(app, &result, buffer, 0);
    return result;
}

static inline View_Summary
tldui_display_buffer_temp(Application_Links *app, View_Summary * base_view,
                          float split_proportion, View_Split_Position split_direction,
                          bool show_file_bar, Buffer_ID buffer)
{
    View_Summary result = open_view(app, base_view, split_direction);
    
    view_set_split_proportion(app, &result, split_proportion);
    
    view_set_setting(app, &result, ViewSetting_ShowScrollbar, false);
    view_set_setting(app, &result, ViewSetting_ShowFileBar, show_file_bar);
    
    view_set_buffer(app, &result, buffer, 0);
    return result;
}

// 
// Panel Management
// 

void tld_display_buffer_by_name(Application_Links *app, String buffer_name,
                                Buffer_Summary *buffer, View_Summary *view,
                                bool prefer_inactive_view, Access_Flag access)
{
    Buffer_Summary buffer_;
    if (!buffer) buffer = &buffer_;
    
    View_Summary view_;
    if (!view) view = &view_;
    
    *buffer = get_buffer_by_name(app, expand_str(buffer_name), access);
    if (!buffer->exists) {
        Buffer_Creation_Data data = {0};
        
        begin_buffer_creation(app, &data, 0);
        buffer_creation_name(app, &data, expand_str(buffer_name), 0);
        *buffer = end_buffer_creation(app, &data);
        
        buffer_set_setting(app, buffer, BufferSetting_Unimportant, true);
        buffer_set_setting(app, buffer, BufferSetting_ReadOnly, true);
    }
    
    int view_count = 0;
    for (View_Summary v = get_view_first(app, AccessAll);
         v.exists; get_view_next(app, &v, AccessAll))
    {
        if (v.buffer_id == buffer->buffer_id) {
            *view = v;
            set_active_view(app, view);
            return;
        }
        
        ++view_count;
    }
    
    *view = get_active_view(app, AccessAll);
    if (view_count > 1) {
        if (prefer_inactive_view) {
            get_view_next(app, view, AccessAll);
            if (!view->exists) {
                *view = get_view_first(app, AccessAll);
            }
            
            set_active_view(app, view);
        }
    } else if (prefer_inactive_view) {
        *view = open_view(app, view, ViewSplit_Right);
        view_set_setting(app, view, ViewSetting_ShowScrollbar, false);
    }
    
    view_set_buffer(app, view, buffer->buffer_id, 0);
}

// 
// Query Bar Utilities
// 

static inline void
tldui_append_s_clipboad(Application_Links * app, String * text) {
    char * append_pos = text->str + text->size;
    int32_t append_mem = text->memory_size - text->size;
    int32_t append_size = clipboard_index(app, 0, 0, append_pos, append_mem);
    
    if (append_size < append_mem) {
        text->size += append_size;
    }
}

static inline void
tldui_append_s_input(String * text, User_Input in) {
    uint8_t character[4];
    uint32_t length = to_writable_character(in, character);
    if (length != 0 && text->memory_size - text->size > (int32_t)length) {
        append_ss(text, make_string(character, length));
    }
}

static void
tld_handle_query_bar_input(Application_Links *app,
                           Query_Bar *bar,
                           User_Input input)
{
    if (input.key.keycode == key_back) {
        backspace_utf8(&bar->string);
    } else if (input.key.keycode == key_del) {
        bar->string.size = 0;
    } else if (input.key.keycode == 'v' && input.key.modifiers[MDFR_ALT]) {
        tldui_append_s_clipboad(app, &bar->string);
    } else if (key_is_unmodified(&input.key)) {
        tldui_append_s_input(&bar->string, input);
    }
}

// 
// UI Printing
// 

static inline Range
tldui_print_text(Application_Links *app, Buffer_Summary *buffer, char *text, int32_t len) {
    Range result = make_range(buffer->size, buffer->size + len);
    buffer_replace_range(app, buffer, buffer->size, buffer->size, text, len);
    return result;
}

static inline Range
tldui_update_dynamic_text(Application_Links *app, Buffer_Summary *buffer,
                          String text, Range text_loc)
{
    Range result = text_loc;
    static const char padding[] = "                                "; // 32 Spaces
    buffer_replace_range(app, buffer,
                         text_loc.min, text_loc.min + text.memory_size,
                         text.str, text.size);
    result.max = text_loc.min + text.size;
    
    int32_t remaining = text.memory_size - text.size;
    buffer_replace_range(app, buffer, result.max, result.max,
                         (char *) padding, remaining % 32);
    
    for (int i = 0; i < remaining / 32; ++i) {
        buffer_replace_range(app, buffer, result.max, result.max,
                             (char *) padding, 32);
    }
    
    return result;
}

static inline Range
tldui_print_dynamic_text(Application_Links *app, Buffer_Summary *buffer, String text) {
    Assert(text.memory_size);
    return tldui_update_dynamic_text(app, buffer, text, make_range(buffer->size, buffer->size));
}

enum tldui_update_result {
    tldui_update_cancel,
    tldui_update_accept,
    tldui_update_next,
    
    tldui_update_result_count
};

static inline tldui_update_result
tldui_query_dynamic_text(Application_Links *app, View_Summary *view, Buffer_Summary *buffer,
                         String * text, Range * text_loc)
{
    view_set_highlight(app, view, 0, 0, 0);
    
    while (true) {
        view_set_mark(
            app, view, seek_pos(text_loc->min));
        view_set_cursor(
            app, view, seek_pos(text_loc->max), false);
        User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
        
        if (in.abort) {
            return tldui_update_cancel;
        } else if (in.type != UserInputKey) {
            continue;
        }
        
        if (in.key.keycode == key_back) {
            backspace_utf8(text);
        } else if (in.key.keycode == key_del) {
            text->size = 0;
        } else if (in.key.keycode == 'v' && in.key.modifiers[MDFR_ALT]) {
            tldui_append_s_clipboad(app, text);
        } else if (in.key.keycode == '\n') {
            return tldui_update_accept;
        } else if (in.key.keycode == '\t') {
            return tldui_update_next;
        } else if (key_is_unmodified(&in.key)) {
            tldui_append_s_input(text, in);
        }
        
        *text_loc = tldui_update_dynamic_text(app, buffer, *text, *text_loc);
    }
}

static inline Range
tldui_print_checkbox(Application_Links *app, Buffer_Summary *buffer,
                     char * label, int32_t label_length, bool32 state)
{
    Range result = make_range(buffer->size, buffer->size);
    
    if (state) {
        buffer_replace_range(app, buffer, buffer->size, buffer->size, literal("[x] "));
    } else {
        buffer_replace_range(app, buffer, buffer->size, buffer->size, literal("[ ] "));
    }
    
    buffer_replace_range(app, buffer, buffer->size, buffer->size, label, label_length);
    result.max += label_length + 4;
    
    return result;
}

static inline void
tldui_toggle_checkbox(Application_Links *app, Buffer_Summary *buffer,
                      Range checkbox, bool32 *state)
{
    *state = !(*state);
    if (*state) {
        buffer_replace_range(app, buffer, checkbox.min, checkbox.min + 3, literal("[x]"));
    } else {
        buffer_replace_range(app, buffer, checkbox.min, checkbox.min + 3, literal("[ ]"));
    }
}

struct tldui_table_printer {
    Buffer_Summary *target;
    int32_t column_width;
    int32_t current_column;
    int32_t column_count;
};

static tldui_table_printer
tldui_make_table(Buffer_Summary * target_buffer, uint32_t column_width, int32_t column_count) {
    tldui_table_printer result;
    
    result.target = target_buffer;
    result.column_width = column_width;
    result.column_count = column_count;
    result.current_column = 0;
    
    return result;
}

static Range
tldui_print_table_cell(Application_Links *app, tldui_table_printer *printer, String text) {
    Assert(printer->column_width <= 40);
    const char spaces[41] = "                                        ";
    
    int32_t max_width = printer->column_count * printer->column_width;
    int32_t projected_width = printer->current_column * printer->column_width + text.size;
    
    if (printer->current_column > 0 && projected_width > max_width) {
        buffer_replace_range(app, printer->target, printer->target->size,
                             printer->target->size, literal("\n"));
        printer->current_column = 0;
    }
    
    Range result;
    result.min = printer->target->size;
    result.max = result.min + text.size;
    
    buffer_replace_range(app, printer->target, printer->target->size,
                         printer->target->size, expand_str(text));
    buffer_replace_range(app, printer->target, printer->target->size,
                         printer->target->size, (char *) &spaces,
                         printer->column_width - (text.size % printer->column_width));
    
    printer->current_column += 1 + text.size / printer->column_width;
    return result;
}

// 
// Fuzzy String Matching
// 

#ifndef TLDUI_MAX_PATTERN_SIZE
#define TLDUI_MAX_PATTERN_SIZE 64
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

static inline b32_4tech
tld_char_is_separator(char a) {
    return (a == ' ' || a == '_' || a == '-' ||
            a == '.' || a == '/' || a == '\\');
}

static inline b32_4tech
tld_fuzzy_match_char(char a, char b) {
    return ((char_to_lower(a) == char_to_lower(b)) ||
            (a == ' ' && tld_char_is_separator(b)));
}

// This is where the magic happens
static int32_t
tld_fuzzy_match_ss(String pattern, String val) {
    if (pattern.size == 0) {
        return 1;
    }
    
    // Optimization 1: Skip table rows
    int j = 0;
    while (j < val.size && !tld_fuzzy_match_char(pattern.str[0], val.str[j])) {
        j++;
    }
    
    if (val.size - j < pattern.size) {
        return 0;
    }
    
    int32_t row[TLDUI_MAX_PATTERN_SIZE] = {0}; // Current row of scores table
    int32_t lml[TLDUI_MAX_PATTERN_SIZE] = {0}; // Current row of auxiliary table (match lengths)
    
    int j_lo = j;
    
    for (; j < val.size; ++j) {
        int32_t diag = 1;
        int32_t diag_l = 0;
        
        // Optimization 2: Skip triangular table sections that don't affect the result
        int i_lo = max(j + pattern.size - val.size, 0);
        int i_hi = min(j - j_lo + 1, pattern.size);
        
        if (i_lo > 0) {
            diag = row[i_lo - 1];
            diag_l = lml[i_lo - 1];
        }
        
        for (int i = i_lo; i < i_hi; ++i) {
            int32_t row_old = row[i];
            int32_t lml_old = lml[i];
            
            int32_t match = tld_fuzzy_match_char(pattern.str[i], val.str[j]);
            lml[i] = match * (diag_l + 1);
            
            if (diag > 0 && match) {
                // Sequential match bonus:
                int32_t value = lml[i];
                
                if (j == 0 || (char_is_lower(val.str[j - 1]) && char_is_upper(val.str[j])) ||
                    tld_char_is_separator(val.str[j - 1]))
                {   // Abbreviation bonus:
                    value += 4;
                }
                
                row[i] = max(diag + value, row[i]);
            }
            
            diag = row_old;
            diag_l = lml_old;
        }
    }
    
    return row[pattern.size - 1];
}

// A string that tracks whether it matched patterns of a given length
struct tldui_string {
    String value;
    // The length of the longest prefix of the current pattern this string matched
    uint64_t matched_pattern_prefix_length;
};

// Query a user for a pattern to search the list with, show the top 7 results
// (or fewer, if not enough strings match the pattern, even with a low score).
// Navigate the result list with the arrow keys, press enter to accept the
// selected string.
// 
// Returns the index of the selected string in the provided string list,
// or -1 if the query was canceled with ESC.
static int32_t
tldui_query_fuzzy_list(Application_Links *app,
                       Query_Bar *search_bar,
                       tldui_string *list,
                       int32_t list_count)
{
    int32_t result_indices[7];
    
    String empty = make_lit_string("");
    Query_Bar result_bars[ArrayCount(result_indices)] = {0};
    for (int i = 0; i < ArrayCount(result_indices); ++i) {
        result_bars[i].string = empty;
        result_bars[i].prompt = empty;
    }
    
    int result_count = 0;
    int result_selected_index = 0;
    bool full_rescan = true;
    int current_min = 0;
    bool search_key_changed = true;
    bool selected_index_changed = true;
    
    while (true) {
        if (search_key_changed) {
            end_query_bar(app, search_bar, 0);
            result_selected_index = 0;
            
            for (int i = result_count - 1; i >= 0; --i) {
                end_query_bar(app, &result_bars[i], 0);
            }
            result_count = 0;
            
            int32_t min_score = 0x7FFFFFFF;
            int32_t min_index = -1;
            
            int32_t result_scores[ArrayCount(result_indices)];
            
            for (int i = 0; i < list_count; ++i) {
                String candidate = list[i].value;
                
                if (full_rescan || list[i].matched_pattern_prefix_length >= current_min) {
                    int32_t score = tld_fuzzy_match_ss(search_bar->string, candidate);
#ifdef TLDUI_TRANSPOSE_PATTERNS
                    for (int j = 1; j < search_bar->string.size; ++j) {
                        char temp = search_bar->string.str[j - 1];
                        search_bar->string.str[j - 1] = search_bar->string.str[j];
                        search_bar->string.str[j] = temp;
                        
                        int32_t score_transpose = tld_fuzzy_match_ss(search_bar->string, candidate) / 4;
                        
                        search_bar->string.str[j] = search_bar.str[j - 1];
                        search_bar->string.str[j - 1] = temp;
                        
                        if (score < score_transpose) {
                            score = score_transpose;
                        }
                    }
#endif
                    
                    if (score > 0) {
                        list[i].matched_pattern_prefix_length = search_bar->string.size;
                        
                        if (result_count < ArrayCount(result_indices)) {
                            result_indices[result_count] = i;
                            result_scores[result_count] = score;
                            
                            if (score > min_score) {
                                min_score = score;
                                min_index = result_count;
                            }
                            
                            ++result_count;
                        } else if (score > result_scores[min_index]) {
                            result_indices[min_index] = i;
                            result_scores[min_index] = score;
                            
                            min_index = 0;
                            min_score = result_scores[0];
                            
                            for (int j = 1; j <= ArrayCount(result_indices); ++j) {
                                if (result_scores[j] < min_score) {
                                    min_score = result_scores[j];
                                    min_index = j;
                                }
                            }
                        }
                    }
                }
            }
            
            for (int i = 0; i < result_count - 1; ++i) {
                int max = i;
                for (int j = i + 1; j < result_count; ++j) {
                    if (result_scores[j] > result_scores[max]) {
                        max = j;
                    }
                }
                
                int32_t s = result_indices[i];
                result_indices[i] = result_indices[max];
                result_indices[max] = s;
                
                int32_t c = result_scores[i];
                result_scores[i] = result_scores[max];
                result_scores[max] = c;
            }
            
            for (int i = result_count - 1; i >= 0; --i) {
                result_bars[i].string = list[result_indices[i]].value;
                start_query_bar(app, &result_bars[i], 0);
            }
            
            start_query_bar(app, search_bar, 0);
        }
        
        if (selected_index_changed || search_key_changed) {
            for (int i = 0; i < result_count; ++i) {
                if (i == result_selected_index) {
                    result_bars[i].prompt = list[result_indices[i]].value;
                    result_bars[i].string = empty;
                } else {
                    result_bars[i].prompt = empty;
                    result_bars[i].string = list[result_indices[i]].value;
                }
            }
        }
        
        User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
        selected_index_changed = false;
        search_key_changed = false;
        full_rescan = false;
        
        if (in.abort) {
            for (int i = 0; i < ArrayCount(result_indices); ++i) {
                end_query_bar(app, &result_bars[i], 0);
            }
            
            return -1;
        }
        
        if (in.type == UserInputKey) {
            if (in.key.keycode == '\n') {
                if (result_count > 0) {
                    if (result_selected_index < 0)
                        result_selected_index = 0;
                    
                    for (int i = 0; i < ArrayCount(result_indices); ++i) {
                        end_query_bar(app, &result_bars[i], 0);
                    }
                    
                    return result_indices[result_selected_index];
                }
            } else if (in.key.keycode == key_back) {
                if (search_bar->string.size > 0) {
                    backspace_utf8(&search_bar->string);
                    search_key_changed = true;
                }
                current_min = search_bar->string.size;
            } else if (in.key.keycode == key_del) {
                search_bar->string.size = 0;
                result_selected_index = 0;
                
                for (int i = result_count - 1; i >= 0; --i) {
                    end_query_bar(app, &result_bars[i], 0);
                }
                
                full_rescan = true;
                current_min = 0;
            } else if (in.key.keycode == key_up) {
                selected_index_changed = true;
                
                --result_selected_index;
                if (result_selected_index < 0) {
                    result_selected_index = result_count - 1;
                }
            } else if (in.key.keycode == key_down) {
                selected_index_changed = true;
                
                ++result_selected_index;
                if (result_selected_index >= result_count) {
                    result_selected_index = 0;
                }
            } else if (key_is_unmodified(&in.key) && in.key.character != 0) {
                uint8_t character[4];
                uint32_t length = to_writable_character(in, character);
                if (length != 0 && search_bar->string.memory_size - search_bar->string.size > (int32_t)length) {
                    append_ss(&search_bar->string, make_string((char *) &character, length));
                    search_key_changed = true;
                    current_min = search_bar->string.size - 1;
                }
            }
        }
    }
}

#endif