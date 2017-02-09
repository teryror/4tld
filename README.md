# 4tld
My customization code for [4coder](http://4coder.net) by Allen Webster.

## Overview
* **4tld_custom.cpp** is the main file that I actually build.
  It defines all the trivial or highly specific custom commands and binds them
  all in a modal set up. This isn't really intended to be reused.
* **4tld\_find\_and\_replace.cpp** is a drop-in file containing functionality
  for a search that works more like what you see in a GUI editor.
  You can `#include` this in your own customization code and call
  `tld_interactive_find_and_replace(5)`, or first
  `#define TLDFR_IMPLEMENT_COMMANDS`, to get custom commands that
  you can bind directly.
* **4tld\_project\_management.cpp** is another drop-in file containing a simple
  system similar to *4coder\_project\_commands.cpp*, with some minor changes
  according to my personal preference. After including this file you need to call
  `tld_project_load_from_buffer(3)` to get a `tld_Project`.
  If you wish to use the predefined commands, you need to first `#define
  TLDPM_IMPLEMENT_COMMANDS`, and call `tld_project_memory_init()` on startup.
* **build.bat** builds 4tld_custom.cpp using `pushd` and `popd`, so that
  this repository can be cloned into the directory containing 4ed.exe,
  buildsuper.bat, etc.
  It will probably complain about not knowing the command `ctime`, in
  which case you're doing yourself a major disservice. There's a bunch
  of forks flying around github, but you can check out the version I use
  [here](https://gist.github.com/cmuratori/8c909975de4bb071056b4ec1651077e8).

## Planned customizations
* I used to have a custom command that would ask for a command name, lookup the
  corresponding command in an array, and execute it. I also had small program to
  accompany it, generating the array at compile time, with no extra work required.
  I would love to bring that back in a more robust fashion.
* The existing `git_quick_save` command is pretty useful, and I would like to do
  more to make version control invisible.
* I might experiment with the frame rendering hooks and scroll rules to add an
  animated cursor, and keep it closer to the (vertical) center of the screen,
  respectively.
* Finally, I've been putting off a clean-up pass since about version 4.0.5, when
  I wrote the original version of these customizations. I really need to clean up
  the way I handle panels, and should make sure behaviour between commands is more
  consistent.
