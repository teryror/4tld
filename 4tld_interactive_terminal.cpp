/******************************************************************************
File: 4tld_interactive_terminal.cpp
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
******************************************************************************/
#include "4tld_user_interface.h"

CUSTOM_COMMAND_SIG(tld_iterm_session_start) {
    Buffer_Summary buffer;
    View_Summary view;
    tld_display_buffer_by_name(app, make_lit_string("*terminal*"),
                               &buffer, &view, false, AccessAll);
    Buffer_Identifier buffer_id = {0};
    buffer_id.id = buffer.buffer_id;
    
    char dir_space[1024];
    String dir = make_fixed_width_string(dir_space);
    dir.size = directory_get_hot(app, dir.str, dir.memory_size);
    Query_Bar dir_bar = {0};
    dir_bar.prompt = dir;
    
    char cmd_space[1024];
    String cmd = make_fixed_width_string(cmd_space);
    Query_Bar cmd_bar = {0};
    cmd_bar.prompt = make_lit_string("> ");
    cmd_bar.string = cmd;
    
    start_query_bar(app, &cmd_bar, 0);
    start_query_bar(app, &dir_bar, 0);
    
    while (tld_requery_user_string(app, &cmd_bar)) {
        exec_system_command(app, &view, buffer_id,
                            expand_str(dir_bar.string), expand_str(cmd_bar.string),
                            CLI_OverlapWithConflict | CLI_CursorAtEnd);
        cmd_bar.string.size = 0;
    }
}