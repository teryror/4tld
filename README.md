# 4tld
My customization code for [4coder](http://4coder.net) by Allen Webster.

I split the more involved commands into reusable drop-in command packs in
roughly the same style as Allen's provided code.

See the source files for documentation on each module.

## Find and Replace
![4tld_find_and_replace.cpp Demo](http://i.imgur.com/MIwuMkQ.png)

## Fuzzy Matching
![4tld_fuzzy_match.cpp Demo](http://i.imgur.com/k00mNuL.png)

## Interactive Terminal
![4tld_interactive_terminal.cpp Demo](http://i.imgur.com/hlig8VH.png)

## Project Management
![4tld_project_management.cpp Demo](http://i.imgur.com/7tblX1A.png)

## Planned customizations
* There is some basic code intelligence stuff that I think is viable to
  implement with just the 4cpp lexer and my fuzzy matching -- I might
  experiment with this in the near future.
* If 4coder ever gets some additional platform independent file system
  abstractions, I'll look into making a directory editor inspired by dired mode
  in emacs.
* The existing `git_quick_save` command is pretty useful, and I would like to do
  more to make version control invisible.
* Finally, I've been putting off a clean-up pass since about version 4.0.5, when
  I wrote the original version of these customizations. I really need to clean
  up the way I handle panels, and should make sure behaviour between commands is
  more consistent.
