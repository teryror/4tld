/******************************************************************************
File: 4tld_project_management.cpp
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
*******************************************************************************
LICENSE

This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.
*******************************************************************************
This file hosts a replacement for the default project management code. I
decided to make this because the existing one gives the per-project
configuration too much power, and I would rather have more consistent
behaviour enforced.

This parses project data from a buffer, rather than a file, so I suggest
calling tld_project_load_from_buffer from the open file hook when you detect a
project file.

A project specification consists of blank lines, comment lines, and key/value
pairs on a single line. Comment lines are designated with a '#' character,
after any optional leading whitespace.

Key/Value pairs are separated by any amount of whitespace. The keys this code
understands are as follows:
* source.file  - The relative path to a source file. You may use a single '*'
  character as a wildcard, e.g. `source.file src/*.cpp` to open multiple files
  at once. Note that source files are opened in a separate call to
  tld_project_open_source_files.
* build.config - A system command line for use by the
  tld_project_build_current_config function.
* debug.config - A system command line for use by the
  tld_project_debug_current_config function. Note that this is sort of stub
  function, meant for later extension, if 4coder ever gets integrated
  debugging features.
  
Preprocessor Variables:
* TLDPM_SOURCE_FILES_CAPACITY - The maximum number of source.file values that
  will actually be stored on a project.
* TLDPM_CONFIGURATIONS_CAPACITY - The maximum number of both build.config and
  source.config values that will be stored on a project. Note that, if you
  are using the default commands, this should not be set to anything higher
  than 7, as 4coder can only display 8 query bars, and one is already used
  for the hints.
* TLDPM_IMPLEMENT_COMMANDS - If this is defined, the custom commands listed
  below are implemented. This is set up this way, so that you can define your
  own variations without compiling commands you won't use, and other command
  packs can utilize these commands if you _are_ using them.
  
Provided Commands:
* tld_current_project_build
* tld_current_project_save_and_build
* tld_current_project_change_build_config
* tld_current_project_save_and_change_build_config
* tld_current_project_debug
* tld_current_project_build_and_debug
* tld_current_project_save_build_and_debug
* tld_current_project_change_debug_config
* tld_current_project_build_and_change_debug_config
* tld_current_project_save_build_and_change_debug_config

Note that, to use these commands, the global project has to be initialized. To
do so, call tld_project_memory_init() before opening the project. I do this in
start hook, but you could do this at pretty much any time.
******************************************************************************/
#include "4tld_user_interface.h"

#ifndef TLDPM_SOURCE_FILES_CAPACITY
#define TLDPM_SOURCE_FILES_CAPACITY 16
#endif

#ifndef TLDPM_CONFIGURATIONS_CAPACITY
#define TLDPM_CONFIGURATIONS_CAPACITY 7
#endif

struct tld_Project {
    String working_directory;
    
    String   source_files[TLDPM_SOURCE_FILES_CAPACITY];
    uint32_t source_files_count;
    
    String   build_configurations[TLDPM_CONFIGURATIONS_CAPACITY];
    uint32_t build_configurations_count;
    uint32_t build_configurations_current;
    
    String   debug_configurations[TLDPM_CONFIGURATIONS_CAPACITY];
    uint32_t debug_configurations_count;
    uint32_t debug_configurations_current;
};

// Parse a single line from a project file
inline bool32
tld_project_parse_line(tld_Project *project, char *line, int line_length, Partition *memory) {
    if (line_length <= 0) return true;
    
    char *line_end = line + line_length;
    while (line < line_end && (*line == ' ' || *line == '\t')) {
        ++line;
    }
    
    if (line < line_end && *line == '#') return true;
    
    char *ident = line;
    while (line < line_end && (*line != ' ' && *line != '\t')) {
        ++line;
    }
    int ident_length = (int)(line - ident);
    
    while (line < line_end && (*line == ' ' || *line == '\t')) {
        ++line;
    }
    
    int content_length = (int)(line_end - line);
    
    if (match_sc(make_string(ident, ident_length), "source.file") &&
        project->source_files_count < TLDPM_SOURCE_FILES_CAPACITY) {
        String new_source_file;
        new_source_file.str = (char *)partition_allocate(memory, content_length);
        new_source_file.memory_size = content_length;
        
        if (new_source_file.str) {
            copy_partial_ss(&new_source_file, make_string(line, content_length));
            
            project->source_files[project->source_files_count] = new_source_file;
            project->source_files_count += 1;
        } else {
            return false;
        }
    } else if (match_sc(make_string(ident, ident_length), "build.config") &&
               project->build_configurations_count < TLDPM_CONFIGURATIONS_CAPACITY) {
        String new_build_config;
        new_build_config.str = (char *)partition_allocate(memory, content_length);
        new_build_config.memory_size = content_length;
        
        if (new_build_config.str) {
            copy_partial_ss(&new_build_config, make_string(line, content_length));
            
            project->build_configurations[project->build_configurations_count] = new_build_config;
            project->build_configurations_count += 1;
        } else {
            return false;
        }
        
    } else if (match_sc(make_string(ident, ident_length), "debug.config") &&
               project->debug_configurations_count < TLDPM_CONFIGURATIONS_CAPACITY) {
        String new_debug_config;
        new_debug_config.str = (char *)partition_allocate(memory, content_length);
        new_debug_config.memory_size = content_length;
        
        if (new_debug_config.str) {
            copy_partial_ss(&new_debug_config, make_string(line, content_length));
            
            project->debug_configurations[project->debug_configurations_count] = new_debug_config;
            project->debug_configurations_count += 1;
        } else {
            return false;
        }
        
    }
    
    return true;
}

// Parse a full project file from a buffer
tld_Project
tld_project_load_from_buffer(Application_Links *app, int32_t buffer_id, Partition *memory) {
    tld_Project result = {0};
    int pos = 0, beginning_of_line = 0;
    
    Buffer_Summary buffer = get_buffer(app, buffer_id, AccessAll);
    result.working_directory = path_of_directory(make_string(buffer.file_name, buffer.file_name_len));
    
    String working_directory_copy;
    working_directory_copy.str = (char *)partition_allocate(memory, result.working_directory.size);
    working_directory_copy.memory_size = result.working_directory.size;
    if (!working_directory_copy.str) {
        tld_show_error("Out of project memory -- failed to load project!");
        return {0};
    }
    copy_partial_ss(&working_directory_copy, result.working_directory);
    result.working_directory = working_directory_copy;
    
    char current_line[2048];
    
    char chunk[1024];
    int chunk_size = sizeof(chunk);
    Stream_Chunk stream = {0};
    
    bool32 success = true;
    
    if (init_stream_chunk(&stream, app, &buffer, pos, chunk, chunk_size)) {
        do {
            for (; pos < stream.end; ++pos) {
                if (stream.data[pos] == '\n') {
                    buffer_read_range(app, &buffer, beginning_of_line, pos, current_line);
                    int current_line_length = pos - beginning_of_line;
                    
                    success &= tld_project_parse_line(&result, current_line,
                                                      current_line_length, memory);
                    
                    beginning_of_line = pos + 1;
                }
            }
        } while (forward_stream_chunk(&stream));
        
        buffer_read_range(app, &buffer, beginning_of_line, pos, current_line);
        int current_line_length = pos - beginning_of_line;
        tld_project_parse_line(&result, current_line, current_line_length, memory);
    }
    
    if (!success) {
        tld_show_warning("Out of project memory -- some features may not work as configured.");
    }
    
    return result;
}

// Reset the memory partition, and reuse it in parsing a new project
tld_Project
tld_project_reload_from_buffer(Application_Links *app, int32_t buffer_id, Partition *memory) {
    memory->pos = 0;
    return tld_project_load_from_buffer(app, buffer_id, memory);
}

// Open all source files as specified by the project file
void tld_project_open_source_files(Application_Links *app, tld_Project *project, Partition *memory) {
    int32_t max_size = partition_remaining(memory);
    Temp_Memory temp = begin_temp_memory(memory);
    char *scratch_space = push_array(memory, char, max_size);
    
    String dir = make_string_cap(scratch_space, 0, max_size);
    append_ss(&dir, project->working_directory);
    
    for (unsigned int i = 0; i < project->source_files_count; ++i) {
        String specified_file = project->source_files[i];
        String file_name = front_of_directory(specified_file);
        int32_t prefix_len = find_s_char(file_name, 0, '*');
        
        if (prefix_len < specified_file.size) {
            // NOTE: Wildcarded string
            tld_change_directory(&dir, path_of_directory(specified_file));
            
            String prefix = make_string(file_name.str, prefix_len);
            String suffix = make_string(file_name.str  + prefix_len + 1,
                                        file_name.size - prefix_len - 1);
            
            File_List all_files = get_file_list(app, expand_str(dir));
            for (int j = 0; j < all_files.count; ++j) {
                File_Info next_file = all_files.infos[j];
                if (next_file.folder) continue;
                
                String next_file_name = make_string(next_file.filename, next_file.filename_len);
                String next_file_name_end = make_string(next_file_name.str + next_file_name.size - suffix.size, suffix.size);
                
                if (match_part_insensitive_ss(next_file_name, prefix) &&
                    match_insensitive_ss(next_file_name_end, suffix))
                {
                    int32_t dir_old_size = dir.size;
                    append_ss(&dir, next_file_name);
                    open_file(app, 0, expand_str(dir), true, false);
                    dir.size = dir_old_size;
                }
            }
            free_file_list(app, all_files);
        } else {
            // NOTE: Normal file name
            append_ss(&dir, specified_file);
            open_file(app, 0, expand_str(dir), true, false);
        }
        
        dir.size = project->working_directory.size;
    }
}

// Build the project using the currently selected build configuration
static bool32
tld_project_build_current_config(Application_Links *app, tld_Project *project,
                                 Buffer_Identifier buffer, View_Summary *view)
{
    uint32_t config_index = project->build_configurations_current;
    return exec_system_command(app, view, buffer, expand_str(project->working_directory),
                               expand_str(project->build_configurations[config_index]),
                               CLI_OverlapWithConflict | CLI_CursorAtEnd);
}

// Run the currently selected debug configuration
static bool32
tld_project_debug_current_config(Application_Links *app, tld_Project *project,
                                 Buffer_Identifier buffer, View_Summary *view)
{
    uint32_t config_index = project->debug_configurations_current;
    return exec_system_command(app, view, buffer, expand_str(project->working_directory),
                               expand_str(project->debug_configurations[config_index]),
                               CLI_OverlapWithConflict | CLI_CursorAtEnd);
}

#ifdef TLDPM_IMPLEMENT_COMMANDS

// The global project object and associated memory
static tld_Project tld_current_project = {0};
static void *__tld_current_project_memory_internal = {0};
static Partition tld_current_project_memory = {0};

#ifndef TLD_HOME_DIRECTORY
#define TLD_HOME_DIRECTORY
static void
tld_get_home_directory(Application_Links *app, String *dest) {
    if (tld_current_project.working_directory.str) {
        copy_ss(dest, tld_current_project.working_directory);
    } else {
        dest->size = directory_get_hot(app, dest->str, dest->memory_size);
    }
}
#endif

// Allocate the global project memory
void tld_project_memory_init() {
    int32_t size = 8 * 1024;
    __tld_current_project_memory_internal = malloc(size);
    tld_current_project_memory = make_part(__tld_current_project_memory_internal, size);
}

// Free the global project memory
void tld_project_memory_free() {
    free(__tld_current_project_memory_internal);
    tld_current_project_memory = {0};
}

// Build the project using the currently selected configuration,
// with the output in the *build* buffer
CUSTOM_COMMAND_SIG(tld_current_project_build) {
    if (!tld_current_project.working_directory.str) return;
    
    Buffer_Summary buffer;
    View_Summary view;
    tld_display_buffer_by_name(app, make_lit_string("*build*"), &buffer, &view, true, AccessAll);
    
    Buffer_Identifier buffer_id = {0};
    buffer_id.id = buffer.buffer_id;
    
    tld_project_build_current_config(app, &tld_current_project, buffer_id, &view);
}

// Save all dirty buffers, then build the project using the currently selected configuration,
// with the output in the *build* buffer.
CUSTOM_COMMAND_SIG(tld_current_project_save_and_build) {
    save_all_dirty_buffers(app);
    exec_command(app, tld_current_project_build);
}

// Select another build configuration to be used in future builds, then build the project
CUSTOM_COMMAND_SIG(tld_current_project_change_build_config) {
    if (!tld_current_project.build_configurations_count) return;
    
    bool32 changed = tld_query_persistent_option(app, make_lit_string("Build Configuration:"),
                                                 tld_current_project.build_configurations, 
                                                 tld_current_project.build_configurations_count,
                                                 &tld_current_project.build_configurations_current);
    
    if (changed) {
        Buffer_Summary buffer;
        View_Summary view;
        tld_display_buffer_by_name(app, make_lit_string("*build*"),
                                   &buffer, &view, true, AccessAll);
        
        Buffer_Identifier buffer_id = {0};
        buffer_id.id = buffer.buffer_id;
        
        tld_project_build_current_config(app, &tld_current_project, buffer_id, &view);
    }
}

// Save all dirty buffers, then select another build configuration
// to be used in future builds, then build the project
CUSTOM_COMMAND_SIG(tld_current_project_save_and_change_build_config) {
    save_all_dirty_buffers(app);
    exec_command(app, tld_current_project_change_build_config);
}

// Run the currently selected debug configuration, with the output in the *debug* buffer
CUSTOM_COMMAND_SIG(tld_current_project_debug) {
    if (!tld_current_project.working_directory.str) return;
    
    Buffer_Summary buffer;
    View_Summary view;
    tld_display_buffer_by_name(app, make_lit_string("*debug*"), &buffer, &view, true, AccessAll);
    
    Buffer_Identifier buffer_id = {0};
    buffer_id.id = buffer.buffer_id;
    
    tld_project_debug_current_config(app, &tld_current_project, buffer_id, &view);
}

// Build the project using the current configuration,
// then run the currently selected debug configuration
CUSTOM_COMMAND_SIG(tld_current_project_build_and_debug) {
    if (!tld_current_project.working_directory.str) return;
    
    Buffer_Summary buffer;
    View_Summary view;
    tld_display_buffer_by_name(app, make_lit_string("*debug*"), &buffer, &view, true, AccessAll);
    
    Buffer_Identifier buffer_id = {0};
    buffer_id.id = buffer.buffer_id;
    
    if (tld_project_build_current_config(app, &tld_current_project, buffer_id, &view)) {
        tld_project_debug_current_config(app, &tld_current_project, buffer_id, &view);
    }
}

// Save all dirty buffers, then build the project using the current configuration,
// then run the currently selected debug configuration
CUSTOM_COMMAND_SIG(tld_current_project_save_build_and_debug) {
    save_all_dirty_buffers(app);
    exec_command(app, tld_current_project_build_and_debug);
}

// Select another debug configuration to be used in future runs, then run
CUSTOM_COMMAND_SIG(tld_current_project_change_debug_config) {
    if (!tld_current_project.debug_configurations_count) return;
    bool32 changed = tld_query_persistent_option(app, make_lit_string("Debug Configuration:"),
                                                 tld_current_project.debug_configurations,
                                                 tld_current_project.debug_configurations_count,
                                                 &tld_current_project.debug_configurations_current);
    
    if (changed) {
        Buffer_Summary buffer;
        View_Summary view;
        tld_display_buffer_by_name(app, make_lit_string("*debug*"),
                                   &buffer, &view, true, AccessAll);
        
        Buffer_Identifier buffer_id = {0};
        buffer_id.id = buffer.buffer_id;
        
        tld_project_debug_current_config(app, &tld_current_project, buffer_id, &view);
    }
}

// Build the project using the current configuration,
// then select another debug configuration to be used in future runs, then run
CUSTOM_COMMAND_SIG(tld_current_project_build_and_change_debug_config) {
    if (!tld_current_project.working_directory.str) return;
    
    Buffer_Summary buffer;
    View_Summary view;
    tld_display_buffer_by_name(app, make_lit_string("*debug*"), &buffer, &view, true, AccessAll);
    
    Buffer_Identifier buffer_id = {0};
    buffer_id.id = buffer.buffer_id;
    
    if (tld_project_build_current_config(app, &tld_current_project, buffer_id, &view)) {
        exec_command(app, tld_current_project_change_debug_config);
    }
}

// Save all dirty buffers, then build the project using the current configuration,
// then select another debug configuration to be used in future runs, then run
CUSTOM_COMMAND_SIG(tld_current_project_save_build_and_change_debug_config) {
    save_all_dirty_buffers(app);
    exec_command(app, tld_current_project_build_and_change_debug_config);
}

#endif