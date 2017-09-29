# 4tld
My customization code for the [4coder text editor](http://4coder.net) by
Allen Webster.

I split the more complicated commands into reusable drop-in command packs in
roughly the same style as Allen's provided customization layer.

See the source files for documentation on each module.

## Find and Replace
![4tld_find_and_replace.cpp Demo](https://imgur.com/FdayQpi.png)

## File Manager
![4tld_file_manager.cpp Demo](https://i.imgur.com/lnaJNcM.png)

## Interactive Terminal
![4tld_interactive_terminal.cpp Demo](https://imgur.com/6ol7B4d.png)

## Project Management
![4tld_project_management.cpp Demo](https://imgur.com/WCK8SSC.png)

## Hex Viewer
![4tld_hex_view.cpp Demo](https://imgur.com/jgkH0pQ.png)

## Fuzzy Matching
![4tld_user_interface.h Demo](https://imgur.com/iCq51Km.png)

## Planned Customizations
* A [magit](https://magit.vc/)-inspired git frontend,
* Multiple compile error parsers to be configured on a per-project basis,
* A spell-checker for use with .md files, string literals and comments.
* An abstraction over the Views API to get all commands to consistently handle
  my two-column layout,
* More abstractions to build User Interfaces,
* There's multiple debuggers in development within the Handmade community, so I
  won't bother working on my own for now.
  However, once [Lysa](https://www.patreon.com/CaptainKraft) becomes usable,
  I'll probably develop my own debugger interface.