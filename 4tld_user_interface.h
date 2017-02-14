/******************************************************************************
File: 4tld_user_interface.h
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
******************************************************************************/
#ifndef FTLD_USER_INTERFACE_H
#define FTLD_USER_INTERFACE_H

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
}

#define tld_show_error(message) tld_show_acknowledged_message(app, make_lit_string("ERROR: "), make_lit_string(message));

#define tld_show_warning(message) tld_show_acknowledged_message(app, make_lit_string("WARNING: "), make_lit_string(message));

#endif