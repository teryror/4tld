/******************************************************************************
File: 4tld_user_interface.h
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
*******************************************************************************
LICENSE

This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.
*******************************************************************************
This file hosts the maximum amount of user interface code shared between
multiple command packs. I'm not a great API designer, so this may not actually
allow you to build similar interfaces faster, this is mostly so that I can
change the behaviour of similar UI elements in one step.

Note that, because of the nature of this file, most drop-in command packs
#include this file, so this is a strict dependency, no matter which one you're
using.
******************************************************************************/
#ifndef FTLD_USER_INTERFACE_H
#define FTLD_USER_INTERFACE_H

// NOTE: This is only needed because directory_cd(6) is broken...
// This should be removed when a 4coder update fixes this bug,
// therefore it was placed in this file, despite not really fitting in
static bool
tld_change_directory(String *dir, String rel_path) {
    bool success = false;
    
    String _dir;
    _dir.str         = (char *) malloc(dir->memory_size);
    _dir.size        = 0;
    _dir.memory_size = dir->memory_size;
    
    append_ss(&_dir, *dir);
    
    while (rel_path.size > 0) {
        String rel_path_element;
        rel_path_element.str = rel_path.str;
        rel_path_element.size = 0;
        
        while (rel_path.str[rel_path_element.size] != '/' &&
               rel_path.str[rel_path_element.size] != '\\' &&
               rel_path_element.size < rel_path.size)
        {
            ++rel_path_element.size;
        }
        
        if (rel_path_element.size == rel_path.size) {
            rel_path.size = 0;
        } else {
            rel_path.str += rel_path_element.size + 1;
            rel_path.size -= rel_path_element.size + 1;
        }
        
        if (match_sc(rel_path_element, "..")) {
            if (_dir.size > 0 && (_dir.str[_dir.size - 1] == '/' ||
                                  _dir.str[_dir.size - 1] == '\\'))
            {
                _dir.size -= 1;
            }
            _dir = path_of_directory(_dir);
        } else if (match_sc(rel_path_element, ".")) {
            // Do nothing -- we're already in the current directory
        } else {
            append_ss(&_dir, rel_path_element);
            append_s_char(&_dir, '/');
        }
    }
    
    /* file_exists() returns false on directories,
     * and I don't want to write OS specific code here,
     * though this does make the cd command kind of annoying...
    if (file_exists(app, expand_str(_dir))) */
    {
        success = true;
        
        dir->size = 0;
        append_ss(dir, _dir);
    }
    
    free(_dir.str);
    return success;
}

// Displays a buffer with the specified name, no matter what.
// Intended for use by commands requiring special buffers.
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
        
        buffer_set_setting(app, buffer, BufferSetting_WrapLine, true);
        buffer_set_setting(app, buffer, BufferSetting_WrapPosition, 950);
        buffer_set_setting(app, buffer, BufferSetting_Unimportant, true);
        buffer_set_setting(app, buffer, BufferSetting_ReadOnly, true);
    }
    
    int view_count = 0;
    for (View_Summary v = get_view_first(app, AccessAll);
         v.exists; get_view_next(app, &v, AccessAll)) {
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
// REGION(Query_Bars)
// 

// Presents the user with up to seven Strings to choose from, with the assumption
// that the selected option (or rather, its index in the array) is persistent.
// The selected option is marked with a star, pressing ESC leaves it untouched.
static bool32
tld_query_persistent_option(Application_Links *app, String hint, String *strings,
                            uint32_t count, uint32_t *selected_index)
{
    Assert(count <= 7);
    
    bool32 result;
    Query_Bar items[7];
    
    uint32_t current_selection = *selected_index;
    
    for (int i = count - 1; i >= 0; --i) {
        items[i].prompt = make_lit_string("  ");
        items[i].string = strings[i];
        start_query_bar(app, &items[i], 0);
    }
    
    Query_Bar hint_bar = {0};
    hint_bar.prompt = hint;
    start_query_bar(app, &hint_bar, 0);
    
    while (true) {
        items[current_selection].prompt = make_lit_string("* ");
        
        User_Input in = get_user_input(app, EventOnAnyKey, EventOnButton);
        if (in.abort || in.key.keycode == key_esc || in.key.keycode == 0) {
            result = false;
            break;
        } else if (in.key.keycode == key_up || in.key.keycode == 'i') {
            items[current_selection].prompt = make_lit_string("  ");
            
            if (current_selection == 0) {
                current_selection = count;
            }
            
            current_selection -= 1;
        } else if (in.key.keycode == key_down || in.key.keycode == 'k') {
            items[current_selection].prompt = make_lit_string("  ");
            current_selection += 1;
            
            if (current_selection >= count) {
                current_selection = 0;
            }
        } else if (in.key.keycode == '\n') {
            result = true;
            break;
        }
    }
    
    for (int i = count - 1; i >= 0; --i) {
        end_query_bar(app, &items[i], 0);
    }
    end_query_bar(app, &hint_bar, 0);
    
    if (result) {
        *selected_index = current_selection;
    }
    
    return result;
}

// A ring buffer of strings
struct tld_StringHistory {
    String* strings;
    int32_t size;
    int32_t offset;
    int32_t capacity;
};

// Pushes a new string into the history and deletes the oldest one if necessary.
// 
// NOTE: This does ALL the memory management, so do not copy string pointers out of
// the history unless you are certain you're not pushing anything else during the
// copy's lifetime.
static void
tld_string_history_push(tld_StringHistory *history, String value) {
    int32_t offset = (history->size + history->offset) % history->capacity;
    
    if (history->strings[offset].str) {
        free(history->strings[offset].str);
    }
    
    history->strings[offset] = {0};
    history->strings[offset].str = (char *) malloc(value.size);
    history->strings[offset].memory_size = value.size;
    copy_partial_ss(&history->strings[offset], value);
    
    if (history->size < history->capacity) {
        ++history->size;
    } else {
        history->offset += 1;
        history->offset %= history->capacity;
    }
}

// Replaces the text in the string with a string from the history.
// This is for use inside a query_user_string type function, NOT a full implementation itself.
static void
tld_query_traverse_history(User_Input in,
                           char keycode_older,
                           char keycode_newer,
                           String *bar_string,
                           tld_StringHistory *history,
                           int32_t *history_index)
{
    if (in.abort) return;
    if (history->size <= 0) return;
    
    if (in.type == UserInputKey) {
        if (in.key.keycode == keycode_older) {
            int32_t _index = (*history_index + history->offset) % history->capacity;
            bar_string->size = 0;
            append_partial_ss(bar_string, history->strings[_index]);
            
            if (*history_index > 0)
                *history_index -= 1;
        } else if (in.key.keycode == keycode_newer) {
            bar_string->size = 0;
            if (*history_index + 1 < history->size) {
                *history_index += 1;
                
                int32_t _index = (*history_index + history->offset) % history->capacity;
                append_partial_ss(bar_string, history->strings[_index]);
            }
        }
    }
}

// Autocompletes filenames of files in the specified work_dir and its subdirectories.
// This is for use inside a query_user_string type function, NOT a full implementation itself.
static void
tld_query_complete_filenames(Application_Links *app, User_Input *in, char keycode, String *bar_string, File_List *file_list, String work_dir)
{
    if (in->abort) return;
    
    if (in->type == UserInputKey && in->key.keycode == keycode) {
        Range incomplete_string = {0};
        incomplete_string.max = bar_string->size;
        incomplete_string.min = incomplete_string.max - 1;
        bool in_work_dir = true;
        
        while (incomplete_string.min >= 0 &&
               bar_string->str[incomplete_string.min] != ' ' &&
               bar_string->str[incomplete_string.min] != '\t')
        {
            if (bar_string->str[incomplete_string.min] == '/' ||
                bar_string->str[incomplete_string.min] == '\\')
            {
                in_work_dir = false;
            }
            --incomplete_string.min;
        }
        ++incomplete_string.min;
        
        String prefix = *bar_string;
        prefix.str += incomplete_string.min;
        prefix.size = incomplete_string.max - incomplete_string.min;
        
        File_List files;
        if (in_work_dir) {
            files = *file_list;
        } else {
            char other_dir_space[1024];
            String other_dir = make_fixed_width_string(other_dir_space);
            append_ss(&other_dir, work_dir);
            tld_change_directory(&other_dir, path_of_directory(prefix));
            prefix = front_of_directory(prefix);
            
            files = get_file_list(app, expand_str(other_dir));
        }
        
        int32_t file_index = -1;
        while (in->type == UserInputKey && in->key.keycode == keycode) {
            if (in->key.modifiers[MDFR_NONE]) { // NOTE: This seems to be either a bug with 4coder, or an error in the 4coder_API headers, as this is 1 if _shift_ is held...?
                if (file_index < 0) {
                    file_index = files.count;
                }
                
                for (--file_index; file_index >= 0; --file_index) {
                    if (match_part_insensitive_cs(files.infos[file_index].filename, prefix)) {
                        break;
                    }
                }
            } else {
                for (++file_index; file_index < files.count; ++file_index) {
                    if (match_part_insensitive_cs(files.infos[file_index].filename, prefix)) {
                        break;
                    }
                }
            }
            
            bar_string->size = incomplete_string.max;
            if (file_index >= 0 && file_index < files.count) {
                append_ss(bar_string, make_string(files.infos[file_index].filename + prefix.size, files.infos[file_index].filename_len - prefix.size));
            } else if (file_index >= files.count) {
                file_index = -1;
            } else if (file_index < 0) {
                file_index = files.count;
            } else {
                // NOTE: Unreachable
                Assert(false);
            }
            
            *in = get_user_input(app, EventOnAnyKey, EventOnEsc);
        }
        
        if (!in_work_dir) {
            free_file_list(app, files);
        }
    }
}

// Allow the user to reenter a string into an already open query bar.
// Note that start_query_bar has to be called before this function.
static int32_t
tld_requery_user_string(Application_Links *app, Query_Bar *bar) {
    User_Input in;
    int32_t success = 1;
    int32_t good_character = 0;
    
    while (true) {
        in = get_user_input(app, EventOnAnyKey, EventOnEsc | EventOnButton);
        if (in.abort) {
            success = 0;
            break;
        }
        
        good_character = 0;
        if (key_is_unmodified(&in.key) && in.key.character != 0) {
            good_character = 1;
        }
        
        if (in.type == UserInputKey) {
            if (in.key.keycode == '\n' || in.key.keycode == '\t') {
                break;
            } else if (in.key.keycode == key_back) {
                if (bar->string.size > 0) {
                    --bar->string.size;
                }
            } else if (good_character) {
                append_s_char(&bar->string, in.key.character);
            }
        }
    }
    
    terminate_with_null(&bar->string);
    return success;
}

// Displays the specified message in a Query_Bar and waits for user input,
// so as to make sure that the user acknowledged the message at least.
static void
tld_show_acknowledged_message(Application_Links *app, String title, String message) {
    Query_Bar message_bar = {0};
    message_bar.prompt = title;
    message_bar.string = message;
    start_query_bar(app, &message_bar, 0);
    
    User_Input in = get_user_input(app, EventOnAnyKey, EventOnButton);
    end_query_bar(app, &message_bar, 0);
}

#define tld_show_error(message) tld_show_acknowledged_message(app, make_lit_string("ERROR: "), make_lit_string(message));

#define tld_show_warning(message) tld_show_acknowledged_message(app, make_lit_string("WARNING: "), make_lit_string(message));

#endif