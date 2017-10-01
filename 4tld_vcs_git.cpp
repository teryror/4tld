/******************************************************************************
Author: Tristan Dannenberg
Notice: No warranty is offered or implied; use this code at your own risk.
*******************************************************************************
LICENSE

This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.
******************************************************************************/

static int32_t
tld_git_run_command(Application_Links * app,
                    Buffer_Summary * output_buffer,
                    String work_dir, String command)
{
    buffer_replace_range(app, output_buffer, 0, output_buffer->size, 0, 0);
    exec_system_command(app, 0, buffer_identifier(output_buffer->buffer_id),
                        expand_str(work_dir), expand_str(command),
                        CLI_OverlapWithConflict | CLI_CursorAtEnd);
    
    String fcoder_output_suffix = make_lit_string("exited with code ");
    int32_t fcoder_output_suffix_pos = -1;
    
    buffer_seek_string_backward(
        app, output_buffer, output_buffer->size, 0,
        expand_str(fcoder_output_suffix),
        &fcoder_output_suffix_pos);
    
    // NOTE: This fires because exec_system_command flushes to the output buffer only after the command has run
    Assert(fcoder_output_suffix_pos >= 0);
    
    int32_t exit_code_size = output_buffer->size - (fcoder_output_suffix_pos + fcoder_output_suffix.size);
    Assert(exit_code_size <= 4);
    
    char exit_code[4];
    buffer_read_range(app, output_buffer,
                      fcoder_output_suffix_pos + fcoder_output_suffix.size,
                      output_buffer->size, exit_code);
    int32_t result = str_to_int_s(make_string(exit_code, exit_code_size));
    
    buffer_replace_range(app, output_buffer, fcoder_output_suffix_pos, output_buffer->size, 0, 0);
    
    return result;
}

void tld_git_print_status(Application_Links * app,
                          String work_dir,
                          View_Summary * target_view,
                          Buffer_Summary * target_buffer)
{
    Buffer_Summary output_buffer = tldui_get_empty_buffer_by_name(
        app, literal("*git-output*"), true, true, AccessAll);
    int32_t error_code = tld_git_run_command(
        app, &output_buffer, work_dir, make_lit_string("git status -z"));
    
    if (error_code) {
        // TODO: Display error
        return;
    }
    
    // TODO: Parse the command output and pretty print a status screen
}

void tld_git_print_log(Application_Links * app,
                       String work_dir,
                       View_Summary * target_view,
                       Buffer_Summary * target_buffer)
{
    Buffer_Summary output_buffer = tldui_get_empty_buffer_by_name(
        app, literal("*git-output*"), true, true, AccessAll);
    int32_t error_code = tld_git_run_command(
        app, &output_buffer, work_dir, make_lit_string("git log --raw -z"));
    
    if (error_code) {
        // TODO: Display error
        return;
    }
    
    // TODO: Parse the command output and pretty print the commit log
}

static tld_version_control_system tld_version_control_system_git = {
    "VCS_GIT", tld_git_print_status, tld_git_print_log
};
