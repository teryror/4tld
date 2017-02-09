/******************************************************************************
File: 4tld_project_management.cpp
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
******************************************************************************/
#include "4tld_user_interface.h"

#ifndef TLDPM_BUILD_CONFIGURATIONS_CAPACITY
#define TLDPM_BUILD_CONFIGURATIONS_CAPACITY 16
#endif

#ifndef TLDPM_SOURCE_EXTENSIONS
#define TLDPM_SOURCE_EXTENSIONS { "cpp", "hpp", "c", "h", "cc", "rs" }
#endif

struct tld_Project {
    String working_directory;
    String source_directory;
    
    String       build_configurations[TLDPM_BUILD_CONFIGURATIONS_CAPACITY];
    unsigned int build_configurations_count;
    unsigned int build_configurations_current;
};

inline void
tld_project_parse_line(tld_Project *project, char *line, int line_length, Partition *memory) {
    if (line_length <= 0) return;
    
    char *line_end = line + line_length;
    while (line < line_end && (*line == ' ' || *line == '\t')) {
        ++line;
    }
    
    if (line < line_end && *line == '#') return;
    
    char *ident = line;
    while (line < line_end && (*line != ' ' && *line != '\t')) {
        ++line;
    }
    int ident_length = (int)(line - ident);
    
    while (line < line_end && (*line == ' ' || *line == '\t')) {
        ++line;
    }
    
    int content_length = (int)(line_end - line);
    
    if (match_sc(make_string(ident, ident_length), "source.dir")) {
        project->source_directory.str = (char *)partition_allocate(memory, content_length);
        project->source_directory.memory_size = content_length;
        
        copy_partial_ss(&project->source_directory, make_string(line, content_length));
    } else if (match_sc(make_string(ident, ident_length), "build.config") &&
               project->build_configurations_count < TLDPM_BUILD_CONFIGURATIONS_CAPACITY) {
        String new_build_config;
        new_build_config.str = (char *)partition_allocate(memory, content_length);
        new_build_config.memory_size = content_length;
        
        copy_partial_ss(&new_build_config, make_string(line, content_length));
        
        project->build_configurations[project->build_configurations_count] = new_build_config;
        project->build_configurations_count += 1;
    }
}

tld_Project
tld_project_load_from_buffer(Application_Links *app, int32_t buffer_id, Partition *memory) {
    tld_Project result = {0};
    int pos = 0, beginning_of_line = 0;
    
    Buffer_Summary buffer = get_buffer(app, buffer_id, AccessAll);
    result.working_directory = path_of_directory(make_string(buffer.file_name, buffer.file_name_len)); // TODO(Test): Wouldn't this crash if we close the buffer?
    
    char current_line[2048];
    
    char chunk[1024];
    int chunk_size = sizeof(chunk);
    Stream_Chunk stream = {0};
    
    if (init_stream_chunk(&stream, app, &buffer, pos, chunk, chunk_size)) {
        do {
            for (; pos < stream.end; ++pos) {
                if (stream.data[pos] == '\n') {
                    buffer_read_range(app, &buffer, beginning_of_line, pos, current_line);
                    int current_line_length = pos - beginning_of_line;
                    
                    tld_project_parse_line(&result, current_line, current_line_length, memory);
                    
                    beginning_of_line = pos + 1;
                }
            }
        } while (forward_stream_chunk(&stream));
        
        buffer_read_range(app, &buffer, beginning_of_line, pos, current_line);
        int current_line_length = pos - beginning_of_line;
        tld_project_parse_line(&result, current_line, current_line_length, memory);
    }
    
    if (result.source_directory.str == 0) {
        result.source_directory = result.working_directory;
    }
    
    return result;
}

void tld_project_open_source_files(Application_Links *app, tld_Project *project, Partition *memory) {
    char *extension_list[] = TLDPM_SOURCE_EXTENSIONS;
    int32_t extension_count = sizeof(extension_list) / sizeof(extension_list[0]);
    open_all_files_with_extension(app, memory, extension_list, extension_count, true);
}

#ifdef TLDPM_IMPLEMENT_COMMANDS
#undef TLDPM_IMPLEMENT_COMMANDS

static tld_Project tld_current_project = {0};
static void *__tld_current_project_memory_internal = {0};
static Partition tld_current_project_memory = {0};

void tld_project_memory_init() {
    int32_t size = 8 * 1024;
    __tld_current_project_memory_internal = malloc(size);
    tld_current_project_memory = make_part(__tld_current_project_memory_internal, size);
}

void tld_project_memory_free() {
    free(__tld_current_project_memory_internal);
    tld_current_project_memory = {0};
}

CUSTOM_COMMAND_SIG(tld_current_project_build) {
    String build_command = tld_current_project.build_configurations[tld_current_project.build_configurations_current];
    
    Buffer_Summary buffer;
    View_Summary view;
    tld_display_buffer_by_name(app, make_lit_string("*build*"), &buffer, &view, true, AccessAll);
    
    Buffer_Identifier buffer_id = {0};
    buffer_id.id = buffer.buffer_id;
    
    exec_system_command(app, &view, buffer_id,
                        tld_current_project.working_directory.str,
                        tld_current_project.working_directory.size,
                        build_command.str, build_command.size,
                        CLI_OverlapWithConflict | CLI_CursorAtEnd);
}

CUSTOM_COMMAND_SIG(tld_current_project_change_build_config) {
    if (!tld_current_project.build_configurations_count) return;
    
    Query_Bar build_configurations[TLDPM_BUILD_CONFIGURATIONS_CAPACITY];
    
    for (int i = tld_current_project.build_configurations_count - 1; i >= 0; --i) {
        build_configurations[i].prompt = make_lit_string("  ");
        build_configurations[i].string = tld_current_project.build_configurations[i];
        start_query_bar(app, &build_configurations[i], false);
    }
    
    unsigned int selected_index = tld_current_project.build_configurations_current;
    
    while (true) {
        build_configurations[selected_index].prompt = make_lit_string("* ");
        
        User_Input in = get_user_input(app, EventOnAnyKey, EventOnButton);
        if (in.abort || in.key.keycode == key_esc || in.key.keycode == 0) {
            return;
        } else if (in.key.keycode == key_up || in.key.keycode == 'i') {
            build_configurations[selected_index].prompt = make_lit_string("  ");
            
            if (selected_index == 0) {
                selected_index = tld_current_project.build_configurations_count;
            }
            
            --selected_index;
        } else if (in.key.keycode == key_down || in.key.keycode == 'k') {
            build_configurations[selected_index].prompt = make_lit_string("  ");
            ++selected_index;
            
            if (selected_index >= tld_current_project.build_configurations_count) {
                selected_index = 0;
            }
        } else if (in.key.keycode == '\n') {
            tld_current_project.build_configurations_current = selected_index;
            exec_command(app, tld_current_project_build);
            
            return;
        }
    }
}

#endif