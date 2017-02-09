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
    result.working_directory = path_of_directory(make_string(buffer.file_name, buffer.file_name_len)); // TODO(Test): Wouldn't this crash if we close the buffer?
    
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

void tld_project_open_source_files(Application_Links *app, tld_Project *project) {
    
}

// TODO: Make a function for opening all files in a tld_Project.source_directory

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
    // TODO: Stub
}

CUSTOM_COMMAND_SIG(tld_current_project_change_build_config) {
    // TODO: Stub
}

#endif