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
            set_active_view(app, view);
            
            if (!view->exists) {
                *view = get_view_first(app, AccessAll);
            }
        }
    } else if (prefer_inactive_view) {
        *view = open_view(app, view, ViewSplit_Right);
        view_set_setting(app, view, ViewSetting_ShowScrollbar, false);
    }
    view_set_buffer(app, view, buffer->buffer_id, 0);
}

#endif