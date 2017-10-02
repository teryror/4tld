/******************************************************************************
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
*******************************************************************************
LICENSE

This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.
******************************************************************************/

#include "4tld_user_interface.h"

// TODO: Store the hot_directory with the state, so that we don't glitch when the hot directory is changed underneath us

struct tld_file_manager_state {
    Range * cells;
    int32_t entry_count;
    int32_t directory_count;
    int32_t selected_index;
    bool32 showing_search_results;
};

char tld_files_dir_header[] =
"\n===[ Directories ]=========================================================================\n";
char tld_files_divider[] = "\n===[ Files ]===============================================================================\n";

static tld_file_manager_state
tld_print_directory(Application_Links *app,
                    Buffer_Summary *buffer,
                    String dir, String needle_file)
{
    tld_file_manager_state new_state = {0};
    
    buffer_replace_range(app, buffer, 0, buffer->size, expand_str(dir));
    buffer_replace_range(app, buffer, buffer->size, buffer->size, literal(tld_files_dir_header));
    tldui_table_printer printer = tldui_make_table(buffer, 30, 3);
    
    File_List contents = get_file_list(app, expand_str(dir));
    new_state.entry_count = contents.count;
    new_state.cells = (Range *) malloc(contents.count * sizeof(Range));
    if (new_state.cells == 0) return new_state;
    
    Range * next_cell = new_state.cells;
    
    for (uint32_t i = 0; i < contents.count; ++i) {
        String file_name = make_string(contents.infos[i].filename, contents.infos[i].filename_len);
        if (contents.infos[i].folder) {
            if (needle_file.str && match_ss(file_name, needle_file)) {
                new_state.selected_index = (int32_t)(next_cell - new_state.cells);
            }
            
            new_state.directory_count += 1;
            *next_cell++ = tldui_print_table_cell(app, &printer, file_name);
        }
    }
    
    buffer_replace_range(app, buffer, buffer->size, buffer->size, literal(tld_files_divider));
    printer.current_column = 0;
    
    for (uint32_t i = 0; i < contents.count; ++i) {
        String file_name = make_string(contents.infos[i].filename, contents.infos[i].filename_len);
        if (!contents.infos[i].folder) {
            if (needle_file.str && match_ss(file_name, needle_file)) {
                new_state.selected_index = (int32_t)(next_cell - new_state.cells);
            }
            
            *next_cell++ = tldui_print_table_cell(app, &printer, file_name);
        }
    }
    
    free_file_list(app, contents);
    return new_state;
}

#ifndef TLDFM_SEARCH_RESULT_CAPACITY
#define TLDFM_SEARCH_RESULT_CAPACITY 1024
#endif

static void
tld_print_search_results_recursive(Application_Links *app,
                                   Buffer_Summary *buffer,
                                   String base_path,
                                   String pattern,
                                   int32_t hot_dir_len,
                                   tld_file_manager_state *new_state)
{
    String base_path_visible = substr_tail(base_path, hot_dir_len);
    
    File_List contents = get_file_list(app, expand_str(base_path));
    for (uint32_t i = 0;
         i < contents.count && new_state->entry_count < TLDFM_SEARCH_RESULT_CAPACITY;
         ++i)
    {
        if (contents.infos[i].folder) continue;
        
        String file_name = make_string(contents.infos[i].filename, contents.infos[i].filename_len);
        if (tld_fuzzy_match_ss(pattern, file_name)) {
            new_state->cells[new_state->entry_count] = make_range(
                buffer->size, buffer->size + base_path_visible.size + file_name.size);
            new_state->entry_count += 1;
            
            buffer_replace_range(app, buffer, buffer->size, buffer->size,
                                 expand_str(base_path_visible));
            buffer_replace_range(app, buffer, buffer->size, buffer->size, expand_str(file_name));
            buffer_replace_range(app, buffer, buffer->size, buffer->size, literal("\n"));
        }
        
    }
    
    for (uint32_t i = 0;
         i < contents.count && new_state->entry_count < TLDFM_SEARCH_RESULT_CAPACITY;
         ++i)
    {
        if (contents.infos[i].folder) {
            int32_t old_size = base_path.size;
            
            if (base_path.memory_size - old_size > contents.infos[i].filename_len) {
                append(&base_path,
                       make_string(contents.infos[i].filename, contents.infos[i].filename_len));
                append(&base_path, "/");
                
                tld_print_search_results_recursive(app, buffer, base_path, pattern, hot_dir_len, new_state);
                
                base_path.size = old_size;
            }
        }
    }
    
    free_file_list(app, contents);
}

char tld_files_search_header[] =
"\n===[ Search Results ]======================================================================\n";

static tld_file_manager_state
tld_print_search_results(Application_Links *app,
                         Buffer_Summary *buffer,
                         String base_path,
                         String pattern)
{
    tld_file_manager_state new_state = {0};
    new_state.cells = (Range *) malloc(TLDFM_SEARCH_RESULT_CAPACITY * sizeof(Range));
    new_state.showing_search_results = true;
    if (new_state.cells == 0) return new_state;
    
    buffer_replace_range(app, buffer, 0, buffer->size, expand_str(base_path));
    buffer_replace_range(app, buffer, buffer->size, buffer->size, literal(tld_files_search_header));
    
    tld_print_search_results_recursive(app, buffer, base_path, pattern, base_path.size, &new_state);
    
    return new_state;
}

static int32_t
__tld_get_column_index(Application_Links *app,
                       Buffer_Summary *buffer,
                       Range *cells,
                       int32_t cell_count,
                       int32_t index)
{
    int32_t result = 0;
    while (index > 0) {
        Range r = make_range(cells[index - 1].max, cells[index].min);
        char chunk[256];
        
        Assert(sizeof(chunk) >= (r.max - r.min));
        buffer_read_range(app, buffer, r.min, r.max, chunk);
        for (int i = 0; i < r.max - r.min; ++i) {
            if (chunk[i] == '\n') {
                goto double_break;
            }
        }
        
        result += 1;
        index  -= 1;
    }
    
    double_break:
    return result;
}

static int32_t
__tld_get_remaining_columns(Application_Links *app,
                            Buffer_Summary *buffer,
                            Range *cells,
                            int32_t cell_count,
                            int32_t index)
{
    int32_t result = 0;
    while (index < cell_count - 1) {
        Range r = make_range(cells[index].max, cells[index + 1].min);
        char chunk[256];
        
        Assert(sizeof(chunk) >= (r.max - r.min));
        buffer_read_range(app, buffer, r.min, r.max, chunk);
        for (int i = 0; i < r.max - r.min; ++i) {
            if (chunk[i] == '\n') {
                goto double_break;
            }
        }
        
        result += 1;
        index  += 1;
    }
    
    double_break:
    return result;
}

static inline void
tld_files_view_update_highlight(Application_Links *app,
                                View_Summary *view,
                                tld_file_manager_state *state)
{
    if (state->entry_count) {
        view_set_highlight(app, view, state->cells[state->selected_index].min,
                           state->cells[state->selected_index].max, true);
    } else {
        view_set_highlight(app, view, 0, 0, 0);
    }
}

static uint32_t tld_files_buffer_mapid = 0;

static tld_file_manager_state tld_files_state = {0};

CUSTOM_COMMAND_SIG(tld_files_move_left) {
    if (tld_files_state.cells == 0 || tld_files_state.entry_count == 0) return;
    
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    int32_t selected_index = tld_files_state.selected_index - 1;
    if (selected_index < 0) selected_index = 0;
    
    tld_files_state.selected_index = selected_index;
    
    view_set_highlight(app, &view, tld_files_state.cells[tld_files_state.selected_index].min,
                       tld_files_state.cells[tld_files_state.selected_index].max, true);
}

CUSTOM_COMMAND_SIG(tld_files_move_right) {
    if (tld_files_state.cells == 0 || tld_files_state.entry_count == 0) return;
    
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    int32_t selected_index = tld_files_state.selected_index + 1;
    if (selected_index >= tld_files_state.entry_count)
        selected_index = tld_files_state.entry_count - 1;
    tld_files_state.selected_index = selected_index;
    
    view_set_highlight(app, &view, tld_files_state.cells[tld_files_state.selected_index].min,
                       tld_files_state.cells[tld_files_state.selected_index].max, true);
}

CUSTOM_COMMAND_SIG(tld_files_move_up) {
    if (tld_files_state.cells == 0 || tld_files_state.entry_count == 0) return;
    
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    int32_t current_column = __tld_get_column_index(app, &buffer,
                                                    tld_files_state.cells,
                                                    tld_files_state.entry_count,
                                                    tld_files_state.selected_index);
    if (tld_files_state.selected_index - current_column) {
        int32_t last_on_prev_line = tld_files_state.selected_index - current_column - 1;
        int32_t prev_line_col_count = 1 + __tld_get_column_index(app, &buffer,
                                                                 tld_files_state.cells,
                                                                 tld_files_state.entry_count,
                                                                 last_on_prev_line);
        if (prev_line_col_count > current_column) {
            tld_files_state.selected_index -= prev_line_col_count;
        } else {
            tld_files_state.selected_index -= current_column + 1;
        }
    } else {
        tld_files_state.selected_index = 0;
    }
    
    view_set_highlight(app, &view, tld_files_state.cells[tld_files_state.selected_index].min,
                       tld_files_state.cells[tld_files_state.selected_index].max, true);
}

CUSTOM_COMMAND_SIG(tld_files_move_down) {
    if (tld_files_state.cells == 0 || tld_files_state.entry_count == 0) return;
    
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    int32_t current_column = __tld_get_column_index(app, &buffer,
                                                    tld_files_state.cells,
                                                    tld_files_state.entry_count,
                                                    tld_files_state.selected_index);
    int32_t remaining_columns = __tld_get_remaining_columns(app, &buffer,
                                                            tld_files_state.cells,
                                                            tld_files_state.entry_count,
                                                            tld_files_state.selected_index);
    int32_t first_on_next_line = tld_files_state.selected_index + remaining_columns + 1;
    
    if (first_on_next_line >= tld_files_state.entry_count) {
        tld_files_state.selected_index = tld_files_state.entry_count - 1;
    } else {
        int32_t remaining_on_next_line = __tld_get_remaining_columns(app, &buffer,
                                                                     tld_files_state.cells,
                                                                     tld_files_state.entry_count,
                                                                     first_on_next_line);
        if (remaining_on_next_line > current_column) {
            tld_files_state.selected_index = first_on_next_line + current_column;
        } else {
            tld_files_state.selected_index = first_on_next_line + remaining_on_next_line;
        }
    }
    
    view_set_highlight(app, &view, tld_files_state.cells[tld_files_state.selected_index].min,
                       tld_files_state.cells[tld_files_state.selected_index].max, true);
}

CUSTOM_COMMAND_SIG(tld_files_find) {
    if (tld_files_state.cells == 0 || tld_files_state.entry_count == 0) return;
    
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    char find_bar_space[1024];
    Query_Bar find_bar = {0};
    find_bar.prompt = make_lit_string("Go to File: ");
    find_bar.string = make_fixed_width_string(find_bar_space);
    start_query_bar(app, &find_bar, 0);
    
    int selected_index = 0;
    
    while (true) {
        User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
        
        char file_name_space[1024];
        String file_name = make_fixed_width_string(file_name_space);
        
        if (in.abort || in.key.keycode == '\n') {
            tld_files_state.selected_index = selected_index;
            break;
        } else if (in.key.keycode == key_back) {
            selected_index = 0;
            backspace_utf8(&find_bar.string);
        } else if (key_is_unmodified(&in.key)) {
            uint8_t character[4];
            uint32_t length = to_writable_character(in, character);
            if (length != 0 && find_bar.string.memory_size - find_bar.string.size > (int32_t)length)
            {
                append_ss(&find_bar.string, make_string((char *) &character, length));
            }
        }
        
        for (int i = selected_index;
             i < tld_files_state.entry_count; ++i)
        {
            Range r = tld_files_state.cells[i];
            if (file_name.memory_size > r.max - r.min &&
                buffer_read_range(app, &buffer, r.min, r.max, file_name.str))
            {
                file_name.size = r.max - r.min;
                if (tld_fuzzy_match_ss(find_bar.string, file_name)) {
                    selected_index = i;
                    break;
                }
            }
        }
        
        view_set_highlight(app, &view, tld_files_state.cells[selected_index].min,
                           tld_files_state.cells[selected_index].max, true);
    }
    
    end_query_bar(app, &find_bar, 0);
}

CUSTOM_COMMAND_SIG(tld_files_find_recursive) {
    if (tld_files_state.cells == 0 || tld_files_state.entry_count == 0) return;
    
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    char find_bar_space[1024];
    Query_Bar find_bar = {0};
    find_bar.prompt = make_lit_string("Find File: ");
    find_bar.string = make_fixed_width_string(find_bar_space);
    start_query_bar(app, &find_bar, 0);
    
    while (true) {
        User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
        
        if (in.abort) {
            return;
        } else if (in.key.keycode == '\n') {
            break;
        } else if (in.key.keycode == key_back) {
            backspace_utf8(&find_bar.string);
        } else if (key_is_unmodified(&in.key)) {
            uint8_t character[4];
            uint32_t length = to_writable_character(in, character);
            if (length != 0 && find_bar.string.memory_size - find_bar.string.size > (int32_t)length)
            {
                append_ss(&find_bar.string, make_string((char *) &character, length));
            }
        }
    }
    
    char hot_dir_space[1024];
    String hot_dir = make_fixed_width_string(hot_dir_space);
    hot_dir.size = directory_get_hot(app, hot_dir.str, hot_dir.memory_size);
    
    tld_files_state = tld_print_search_results(app, &buffer, hot_dir, find_bar.string);
    tld_files_view_update_highlight(app, &view, &tld_files_state);
}

CUSTOM_COMMAND_SIG(tld_files_open_selected) {
    if (tld_files_state.cells == 0 || tld_files_state.entry_count == 0) return;
    
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    char hot_dir_space[1024];
    String hot_dir = make_fixed_width_string(hot_dir_space);
    hot_dir.size = directory_get_hot(app, hot_dir.str, hot_dir.memory_size);
    
    Range r = tld_files_state.cells[tld_files_state.selected_index];
    if (tld_files_state.selected_index < tld_files_state.directory_count) {
        if (hot_dir.memory_size - hot_dir.size > r.max - r.min) {
            if (buffer_read_range(app, &buffer, r.min, r.max, hot_dir.str + hot_dir.size)) {
                hot_dir.size += r.max - r.min;
                append(&hot_dir, "/");
                
                free(tld_files_state.cells);
                tld_files_state = tld_print_directory(app, &buffer, hot_dir, {0});
                directory_set_hot(app, expand_str(hot_dir));
            }
        }
    } else {
        if (hot_dir.memory_size - hot_dir.size > r.max - r.min) {
            if (buffer_read_range(app, &buffer, r.min, r.max, hot_dir.str + hot_dir.size)) {
                hot_dir.size += r.max - r.min;
                Buffer_Summary new_buffer = create_buffer(app, expand_str(hot_dir), 0);
                
                if (new_buffer.exists) {
                    free(tld_files_state.cells);
                    tld_files_state = {0};
                    
                    kill_buffer(app, buffer_identifier(buffer.buffer_id),
                                view.view_id, BufferKill_AlwaysKill);
                    
                    view_set_setting(app, &view, ViewSetting_ShowFileBar, 1);
                    view_set_highlight(app, &view, 0, 0, 0);
                    view_set_buffer(app, &view, new_buffer.buffer_id, 0);
                }
                
                return;
            }
        }
    }
    
    tld_files_view_update_highlight(app, &view, &tld_files_state);
}

CUSTOM_COMMAND_SIG(tld_files_click_select) {
    if (tld_files_state.cells == 0) return;
    
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    Mouse_State mouse = get_mouse_state(app);
    float rx = 0, ry = 0;
    
    int32_t old_selection = tld_files_state.selected_index;
    if (global_point_to_view_point(&view, mouse.x, mouse.y, &rx, &ry)) {
        view_set_cursor(app, &view, seek_xy(rx, ry, 1, view.unwrapped_lines), 0);
        int32_t pos = view.cursor.pos;
        
        for (int i = 0; i < tld_files_state.entry_count; ++i) {
            if (tld_files_state.cells[i].min <= pos &&
                pos <= tld_files_state.cells[i].max)
            {
                tld_files_state.selected_index = i;
                break;
            }
        }
        
        if (old_selection == tld_files_state.selected_index) {
            exec_command(app, tld_files_open_selected);
        } else {
            tld_files_view_update_highlight(app, &view, &tld_files_state);
        }
    }
}

CUSTOM_COMMAND_SIG(tld_files_goto_parent_directory) {
    if (tld_files_state.cells == 0) return;
    
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    char hot_dir_space[1024];
    String hot_dir = make_fixed_width_string(hot_dir_space);
    hot_dir.size = directory_get_hot(app, hot_dir.str, hot_dir.memory_size);
    
    if (!tld_files_state.showing_search_results) {
        if (directory_cd(app, hot_dir.str, &hot_dir.size, hot_dir.memory_size, literal(".."))) {
            free(tld_files_state.cells);
            directory_set_hot(app, expand_str(hot_dir));
        }
    }
    
    tld_files_state = tld_print_directory(app, &buffer, hot_dir, {0});
    tld_files_view_update_highlight(app, &view, &tld_files_state);
}

CUSTOM_COMMAND_SIG(tld_files_close_buffer) {
    if (tld_files_state.cells == 0) return;
    
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    kill_buffer(app, buffer_identifier(buffer.buffer_id), view.view_id, BufferKill_AlwaysKill);
    
    view_set_setting(app, &view, ViewSetting_ShowFileBar, 1);
    view_set_highlight(app, &view, 0, 0, 0);
    
    free(tld_files_state.cells);
    tld_files_state = {0};
}

// Display the *files* buffer and pretty print the contents of the current hot directory
CUSTOM_COMMAND_SIG(tld_files_show_hot_dir) {
    View_Summary view = get_active_view(app, AccessAll);
    view_set_setting(app, &view, ViewSetting_ShowFileBar, 0);
    
    Buffer_Summary buffer = get_buffer_by_name(app, literal("*files*"), AccessAll);
    if (!buffer.exists) {
        buffer = create_buffer(app, literal("*files*"), BufferCreate_AlwaysNew);
        buffer_set_setting(app, &buffer, BufferSetting_Unimportant, 1);
        
        Assert(buffer_set_setting(app, &buffer, BufferSetting_MapID, tld_files_buffer_mapid));
    }
    view_set_buffer(app, &view, buffer.buffer_id, 0);
    
    char hot_dir_space[1024];
    String hot_dir = make_fixed_width_string(hot_dir_space);
    hot_dir.size = directory_get_hot(app, hot_dir.str, hot_dir.memory_size);
    
    tld_files_state = tld_print_directory(app, &buffer, hot_dir, {0});
    view_set_highlight(app, &view, tld_files_state.cells[tld_files_state.selected_index].min,
                       tld_files_state.cells[tld_files_state.selected_index].max, true);
}

#ifndef TLD_FILES_DIR_STACK_CAP
#define TLD_FILES_DIR_STACK_CAP 32
#endif

static String tld_files_directory_stack[TLD_FILES_DIR_STACK_CAP];
static int32_t tld_files_directory_stack_size = 0;

CUSTOM_COMMAND_SIG(tld_files_directory_push) {
    if (tld_files_directory_stack_size >= TLD_FILES_DIR_STACK_CAP) return;
    
    tld_files_directory_stack[tld_files_directory_stack_size].str = (char *)malloc(1024);
    tld_files_directory_stack[tld_files_directory_stack_size].memory_size = 1024;
    tld_files_directory_stack[tld_files_directory_stack_size].size =
        directory_get_hot(app,
                          tld_files_directory_stack[tld_files_directory_stack_size].str, tld_files_directory_stack[tld_files_directory_stack_size].memory_size);
    
    tld_files_directory_stack_size += 1;
}

CUSTOM_COMMAND_SIG(tld_files_directory_pop) {
    if (tld_files_directory_stack_size == 0) return;
    tld_files_directory_stack_size -= 1;
    
    directory_set_hot(app, expand_str(
        tld_files_directory_stack[tld_files_directory_stack_size]));
    free(tld_files_directory_stack[tld_files_directory_stack_size].str);
    
    tld_files_directory_stack[tld_files_directory_stack_size] = {0};
    exec_command(app, tld_files_show_hot_dir);
}

CUSTOM_COMMAND_SIG(tld_files_show_directory_of_current_file) {
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary file_buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    String hot_dir;
    String file_name = {0};
    if (file_buffer.file_name) {
        String file_name_ = make_string(file_buffer.file_name, file_buffer.file_name_len);
        
        hot_dir = path_of_directory(file_name_);
        file_name = front_of_directory(file_name_);
        
        directory_set_hot(app, expand_str(hot_dir));
    } else {
        char hot_dir_space[1024];
        hot_dir = make_fixed_width_string(hot_dir_space);
        hot_dir.size = directory_get_hot(app, hot_dir.str, hot_dir.memory_size);
    }
    
    view_set_setting(app, &view, ViewSetting_ShowFileBar, 0);
    
    Buffer_Summary buffer = get_buffer_by_name(app, literal("*files*"), AccessAll);
    if (!buffer.exists) {
        buffer = create_buffer(app, literal("*files*"), BufferCreate_AlwaysNew);
        buffer_set_setting(app, &buffer, BufferSetting_Unimportant, 1);
        buffer_set_setting(app, &buffer, BufferSetting_MapID, tld_files_buffer_mapid);
    }
    view_set_buffer(app, &view, buffer.buffer_id, 0);
    
    tld_files_state = tld_print_directory(app, &buffer, hot_dir, file_name);
    view_set_highlight(app, &view, tld_files_state.cells[tld_files_state.selected_index].min,
                       tld_files_state.cells[tld_files_state.selected_index].max, true);
}

static void
tld_files_bind_map(Bind_Helper *context, uint32_t mapid) {
    tld_files_buffer_mapid = mapid;
    
    begin_map(context, mapid);
    
    bind(context, key_mouse_left, MDFR_NONE, tld_files_click_select);
    
    bind(context, 'j', MDFR_NONE, tld_files_move_left);
    bind(context, 'l', MDFR_NONE, tld_files_move_right);
    bind(context, 'i', MDFR_NONE, tld_files_move_up);
    bind(context, 'k', MDFR_NONE, tld_files_move_down);
    
    bind(context, 'f', MDFR_NONE, tld_files_find);
    bind(context, 'F', MDFR_NONE, tld_files_find_recursive);
    
    bind(context, key_back, MDFR_NONE, tld_files_goto_parent_directory);
    bind(context, '\n', MDFR_NONE, tld_files_open_selected);
    bind(context, '.', MDFR_NONE, tld_files_directory_push);
    bind(context, ',', MDFR_NONE, tld_files_directory_pop);
    
    /* TODO: Quick navigation commands 
    bind(context, 'g', MDFR_NONE, tld_files_goto_directory);
    */
    
    /* TODO: File operations
    bind(context, 'c', MDFR_NONE, tld_files_copy);
    bind(context, 'd', MDFR_NONE, tld_files_delete);
    bind(context, 'n', MDFR_NONE, tld_files_new);
    bind(context, 'x', MDFR_NONE, tld_files_move);
    bind(context, '\t', MDFR_NONE, tld_files_); // Two-pane mode?
    */
    
    bind(context, key_esc, MDFR_NONE, tld_files_close_buffer);
    
    end_map(context);
}
