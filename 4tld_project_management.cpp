/******************************************************************************
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
*******************************************************************************
LICENSE

This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.
*******************************************************************************
NOTE that currently, build configurations and test configurations function
practically identically. This will change as Lysa or other debugger frontends
for 4coder become available and are integrated into 4tld.
******************************************************************************/

struct tld_project;

typedef void (*tld_vcs_print_status)(Application_Links *, String, View_Summary *, Buffer_Summary *);
typedef void (*tld_vcs_print_log)(Application_Links *, String, View_Summary *, Buffer_Summary *);

struct tld_version_control_system {
    char * identifier;
    tld_vcs_print_status print_status;
    tld_vcs_print_log print_log;
};

struct tld_project {
    String project_file;
    
    String source_dir;
    char ** source_extensions;
    int32_t source_extension_count;
    
    String build_dir;
    char ** build_configs;
    int32_t build_config_count;
    int32_t build_config_current_index;
    
    String test_dir;
    char ** test_configs;
    int32_t test_config_count;
    int32_t test_config_current_index;
    
    tld_version_control_system * vcs;
    String version_control_dir;
};

#ifndef TLDPM_SRC_EXTENSION_CAPACITY
#define TLDPM_SRC_EXTENSION_CAPACITY 8
#endif

#ifndef TLDPM_BUILD_CONFIG_CAPACITY
#define TLDPM_BUILD_CONFIG_CAPACITY 16
#endif

#ifndef TLDPM_TEST_CONFIG_CAPACITY
#define TLDPM_TEST_CONFIG_CAPACITY 16
#endif

#ifndef TLDPM_VCS_CAPACITY
#define TLDPM_VCS_CAPACITY 8
#endif

static tld_version_control_system * tld_project_version_control_systems[TLDPM_VCS_CAPACITY];
static int tld_project_version_control_system_count = 0;

static inline bool32
tld_project_register_version_control_system(tld_version_control_system * vcs) {
    if (tld_project_version_control_system_count >= TLDPM_VCS_CAPACITY) return false;
    
    tld_project_version_control_systems[tld_project_version_control_system_count++] = vcs;
    return true;
}

static inline void
tld_project_read_string_array(Config_Item item,
                              char * array_identifier,
                              Config_Array_Reader * array_reader,
                              char ** dest_array,
                              int32_t * dest_count,
                              int32_t dest_capacity)
{
    if (config_array_var(item, array_identifier, 0, array_reader)) {
        Config_Item array_element;
        while (config_array_next_item(array_reader,
                                      &array_element))
        {
            String value;
            value.str = (char *) malloc(1024);
            value.size = 0;
            value.memory_size = 1024;
            
            if ((*dest_count < dest_capacity) &&
                config_string_var(array_element, 0, 0, &value))
            {
                terminate_with_null(&value);
                dest_array[*dest_count] = value.str;
                *dest_count += 1;
            }
        }
    }
}

static inline void
tld_project_read_relative_path(String *dest, String base_path, String rel_path) {
    if (rel_path.size) {
        // TODO: Use directory_cd once that is fixed
        
        dest->size = 0;
        dest->memory_size = base_path.size + rel_path.size;
        dest->str = (char *) malloc(dest->memory_size);
        
        append_ss(dest, base_path);
        append_ss(dest, rel_path);
    } else {
        *dest = base_path;
    }
}

static void
tld_project_read_from_file(Application_Links *app,
                           tld_project *dest,
                           char *file_name,
                           int32_t file_name_len)
{
    Partition *part = &global_part;
    FILE *file = fopen(file_name, "rb");
    
    String base_path = path_of_directory(make_string(file_name, file_name_len));
    
    if (file) {
        Temp_Memory temp = begin_temp_memory(part);
        
        char *mem = 0;
        int32_t size = 0;
        bool32 file_read_success = file_handle_dump(part, file, &mem, &size);
        fclose(file);
        
        if (file_read_success) {
            *dest = {0};
            
            dest->project_file = make_string(file_name, file_name_len);
            
            dest->source_extensions = (char **) malloc(
                sizeof(char *) * TLDPM_SRC_EXTENSION_CAPACITY);
            dest->build_configs = (char **) malloc(
                sizeof(char *) * TLDPM_BUILD_CONFIG_CAPACITY);
            dest->test_configs = (char **) malloc(
                sizeof(char *) * TLDPM_TEST_CONFIG_CAPACITY);
            
            Cpp_Token_Array array;
            array.count = 0;
            array.max_count = (1 << 20)/sizeof(Cpp_Token);
            array.tokens = push_array(&global_part, Cpp_Token, array.max_count);
            
            Cpp_Keyword_Table kw_table = {0};
            Cpp_Keyword_Table pp_table = {0};
            lexer_keywords_default_init(part, &kw_table, &pp_table);
            
            Cpp_Lex_Data S = cpp_lex_data_init(false, kw_table, pp_table);
            Cpp_Lex_Result result = cpp_lex_step(&S, mem, size + 1,
                                                 HAS_NULL_TERM, &array,
                                                 NO_OUT_LIMIT);
            
            char source_dir_space[256];
            String source_dir = make_fixed_width_string(source_dir_space);
            
            char build_dir_space[256];
            String build_dir = make_fixed_width_string(build_dir_space);
            
            char build_err_format_space[16];
            String build_error_format = make_fixed_width_string(build_err_format_space);
            
            char test_dir_space[256];
            String test_dir = make_fixed_width_string(test_dir_space);
            
            char version_control_system_space[16];
            String vcs = make_fixed_width_string(version_control_system_space);
            
            Config_Array_Reader array_reader;
            
            if (result == LexResult_Finished) {
                for (int32_t i = 0; i < array.count; ++i) {
                    Config_Line config_line = read_config_line(array, &i);
                    if (config_line.read_success) {
                        Config_Item item = get_config_item(
                            config_line, mem, array);
                        
                        config_string_var(item, "source_dir", 0, &source_dir);
                        config_string_var(item, "build_dir", 0, &build_dir);
                        config_string_var(item, "test_dir", 0, &test_dir);
                        
                        config_identifier_var(
                            item, "build_error_format", 0, &build_error_format);
                        config_identifier_var(
                            item, "version_control_system", 0, &vcs);
                        
                        tld_project_read_string_array(item, "source_extensions", &array_reader,
                                                      dest->source_extensions,
                                                      &dest->source_extension_count,
                                                      TLDPM_SRC_EXTENSION_CAPACITY);
                        tld_project_read_string_array(item, "build_configs", &array_reader,
                                                      dest->build_configs,
                                                      &dest->build_config_count,
                                                      TLDPM_BUILD_CONFIG_CAPACITY);
                        tld_project_read_string_array(item, "test_configs", &array_reader,
                                                      dest->test_configs,
                                                      &dest->test_config_count,
                                                      TLDPM_TEST_CONFIG_CAPACITY);
                    }
                }
                
                tld_project_read_relative_path(&dest->source_dir, base_path, source_dir);
                tld_project_read_relative_path(&dest->build_dir,  base_path, build_dir);
                tld_project_read_relative_path(&dest->test_dir,   base_path, test_dir);
                
                if (build_error_format.size) {
                    // TODO: Implement error parsers and set them here
                    // (which ones are compatible, or even interchangeable?)
                    if (match(build_error_format, "ERR_STYLE_GCC")) {
                    } else if (match(build_error_format, "ERR_STYLE_MSVC")) {
                    } else if (match(build_error_format, "ERR_STYLE_CLANG")) {
                    } else if (match(build_error_format, "ERR_STYLE_RUSTC")) {
                    } else {
                        Assert(false); // TODO: Report unsupported error format
                    }
                }
                
                if (vcs.size) {
                    for (int i = 0; i < tld_project_version_control_system_count; ++i) {
                        if (match(vcs, tld_project_version_control_systems[i]->identifier)) {
                            dest->vcs = tld_project_version_control_systems[i];
                            break;
                        }
                    }
                    
                    if (dest->vcs == 0) {
                        Assert(false); // TODO: Report unsupported VCS error
                    }
                }
            }
        }
        
        end_temp_memory(temp);
    }
}

static void
tld_project_free(tld_project * proj) {
    for (int i = 0; i < proj->source_extension_count; ++i) {
        free(proj->source_extensions[i]);
    }
    free(proj->source_extensions);
    
    for (int i = 0; i < proj->build_config_count; ++i) {
        free(proj->build_configs[i]);
    }
    free(proj->build_configs);
    
    for (int i = 0; i < proj->test_config_count; ++i) {
        free(proj->test_configs[i]);
    }
    free(proj->test_configs);
    
    *proj = {0};
}

static tld_project tld_current_project = {0};

CUSTOM_COMMAND_SIG(tld_project_open_all_code) {
    tld_project * proj = &tld_current_project;
    
    if (proj->source_dir.str && proj->source_extension_count) {
        // open_all_files_with_extension_internal uses the tail memory in the
        // directory string -- however, this may be part of proj->project_file,
        // which must not be overwritten, so we copy source_dir before starting
        
        char src_dir_space[4096];
        String src_dir = make_fixed_width_string(src_dir_space);
        append_ss(&src_dir, proj->source_dir);
        
        if (src_dir.str[src_dir.size - 1] != '/' &&
            src_dir.str[src_dir.size - 1] != '\\')
        {
            append_s_char(&src_dir, '/');
        }
        
        open_all_files_with_extension_internal(app, src_dir, proj->source_extensions,
                                               proj->source_extension_count, false);
    }
}

CUSTOM_COMMAND_SIG(tld_reload_project) {
    String project_path = tld_current_project.project_file;
    if (project_path.str) {
        tld_project_free(&tld_current_project);
        tld_project_read_from_file(app, &tld_current_project,
                                   expand_str(project_path));
    }
}

CUSTOM_COMMAND_SIG(tld_project_show_vcs_status) {
    tld_project * proj = &tld_current_project;
    
    if (proj->vcs) {
        Buffer_Summary buffer;
        View_Summary view;
        
        tld_display_buffer_by_name(app, make_lit_string("*vc-status*"),
                                   &buffer, &view, true, AccessAll);
        proj->vcs->print_status(app, proj->source_dir, &view, &buffer);
    }
}

CUSTOM_COMMAND_SIG(tld_build_project) {
    tld_project * proj = &tld_current_project;
    
    if (proj->build_config_count) {
        if (proj->build_config_current_index >= proj->build_config_count) {
            proj->build_config_current_index = 0;
        }
        
        char * build_config_raw = proj->build_configs[proj->build_config_current_index];
        String build_config = make_string_slowly(build_config_raw);
        
        Buffer_Summary buffer;
        View_Summary view;
        
        tld_display_buffer_by_name(app, make_lit_string("*build*"),
                                   &buffer, &view, true, AccessAll);
        exec_system_command(app, &view, buffer_identifier(buffer.buffer_id),
                            expand_str(proj->build_dir), expand_str(build_config),
                            CLI_OverlapWithConflict | CLI_CursorAtEnd);
        
        // TODO: Parse error locations according to the configured error parser
        // and construct a jump buffer, potentially with a prettier error list
    }
}

CUSTOM_COMMAND_SIG(tld_test_project) {
    tld_project * proj = &tld_current_project;
    
    if (proj->test_config_count) {
        if (proj->test_config_current_index >= proj->test_config_count) {
            proj->test_config_current_index = 0;
        }
        
        char * test_config_raw = proj->test_configs[proj->test_config_current_index];
        String test_config = make_string_slowly(test_config_raw);
        
        Buffer_Summary buffer;
        View_Summary view;
        
        tld_display_buffer_by_name(app, make_lit_string("*test*"),
                                   &buffer, &view, true, AccessAll);
        exec_system_command(app, &view, buffer_identifier(buffer.buffer_id),
                            expand_str(proj->test_dir), expand_str(test_config),
                            CLI_OverlapWithConflict | CLI_CursorAtEnd);
    }
}

// TODO: Parts of this function should definitely be extracted into 4tld_user_interface.h
static inline bool32
tld_change_config_impl(Application_Links *app,
                       char * buffer_name, int32_t buffer_name_len,
                       char ** list, int32_t list_len,
                       int32_t * list_selected_index)
{
    bool32 result = false;
    
    View_Summary active_view = get_active_view(app, AccessAll);
    
    View_Summary config_view = open_view(app, &active_view, ViewSplit_Top);
    view_set_split_proportion(app, &config_view, 0.25f);
    view_set_setting(app, &config_view, ViewSetting_ShowScrollbar, false);
    
    Buffer_Summary config_buffer = get_buffer_by_name(app, buffer_name, buffer_name_len, AccessAll);
    if (!config_buffer.exists) {
        config_buffer = create_buffer(app, buffer_name, buffer_name_len, BufferCreate_AlwaysNew);
        buffer_set_setting(app, &config_buffer, BufferSetting_Unimportant, true);
        buffer_set_setting(app, &config_buffer, BufferSetting_ReadOnly, true);
    }
    view_set_buffer(app, &config_view, config_buffer.buffer_id, 0);
    
    buffer_replace_range(app, &config_buffer, 0, config_buffer.size, 0, 0);
    Range * ranges = (Range *) malloc(sizeof(Range) * list_len);
    Range * next_cell = ranges;
    
    for (int i = 0; i < list_len; ++i) {
        String config = make_string_slowly(list[i]);
        
        *next_cell++ = make_range(config_buffer.size, config_buffer.size + config.size);
        
        buffer_replace_range(
            app, &config_buffer, config_buffer.size, config_buffer.size, expand_str(config));
        buffer_replace_range(
            app, &config_buffer, config_buffer.size, config_buffer.size, literal("\n"));
    }
    
    int32_t selected_index = *list_selected_index;
    User_Input in;
    do {
        Range r = ranges[selected_index];
        view_set_highlight(app, &config_view, r.min, r.max, 1);
        
        in = get_user_input(app, EventOnAnyKey, EventOnEsc);
        
        if (in.key.keycode == key_up || in.key.keycode == 'i') {
            if (selected_index > 0)
                selected_index -= 1;
        } else if (in.key.keycode == key_down || in.key.keycode == 'k') {
            if (selected_index < list_len - 1)
                selected_index += 1;
        } else if (in.key.keycode == '\n') {
            *list_selected_index = selected_index;
            
            result = true;
            break;
        }
    } while (!in.abort);
    
    free(ranges);
    kill_buffer(app, buffer_identifier(config_buffer.buffer_id),
                config_view.view_id, BufferKill_AlwaysKill);
    close_view(app, &config_view);
    
    return result;
}

CUSTOM_COMMAND_SIG(tld_change_build_config) {
    bool32 success = tld_change_config_impl(app, literal("*build-config*"),
                                            tld_current_project.build_configs,
                                            tld_current_project.build_config_count,
                                            &tld_current_project.build_config_current_index);
    if (success) {
        exec_command(app, tld_build_project);
    }
}

CUSTOM_COMMAND_SIG(tld_change_test_config) {
    bool32 success = tld_change_config_impl(app, literal("*test-config*"),
                                            tld_current_project.test_configs,
                                            tld_current_project.test_config_count,
                                            &tld_current_project.build_config_current_index);
    if (success) {
        exec_command(app, tld_test_project);
    }
}