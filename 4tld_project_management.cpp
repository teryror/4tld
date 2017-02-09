/******************************************************************************
File: 4tld_project_management.cpp
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
******************************************************************************/

#ifndef TLDPM_BUILD_CONFIGURATIONS_CAPACITY
#define TLDPM_BUILD_CONFIGURATIONS_CAPACITY 16
#endif

struct tld_Project {
    String working_directory;
    String source_directory;
    
    String       build_configurations[TLDPM_BUILD_CONFIGURATIONS_CAPACITY];
    unsigned int build_configurations_count;
    unsigned int build_configurations_current;
};

tld_Project tld_project_load_from_buffer(Application_Links *app, int32_t buffer_id, Partition *memory) {
    tld_Project result = {0};
    int pos = 0;
    
    Buffer_Summary buffer = get_buffer(app, buffer_id, AccessAll);
    result.working_directory = path_of_directory(make_string(buffer.file_name, buffer.file_name_len));
    
    char chunk[1024];
    int chunk_size = sizeof(chunk);
    Stream_Chunk stream = {0};
    
    if (init_stream_chunk(&stream, app, &buffer, pos, chunk, chunk_size)) {
        do {
            for (; pos < stream.end; ++pos) {
                // TODO: Parse project file into result
            }
        } while (forward_stream_chunk(&stream));
    }
    
    return result;
}

// TODO: Make a function for opening all files in a tld_Project.source_directory

#ifdef TLDPM_IMPLEMENT_COMMANDS
#undef TLDPM_IMPLEMENT_COMMANDS

// TODO: Make a function for initializing this stuff
static tld_Project tld_current_project = {0};
static void *__tld_current_project_memory_internal = {0};
static Partition tld_current_project_memory = {0};

// TODO: Make a command for running the tld_Project.build_configurations_current
// TODO: Make a command for switching changing the tld_Project.build_configurations_current

#endif