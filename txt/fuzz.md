# Fuzzy Matching in 4tld

## Background
Sublime, Textmate and Atom all ship with it, vim and emacs have popular plugins to do it, yet [nobody really seems to know how it is implemented][1]: fuzzy string search has you typing less and is just plain cool on a technical level, which is probably why programmers the world over love it so much.

I've never really used an editor that has it, but since it's apparently such a big deal, and I want 4coder to succeed, I decided to give implementing it a shot. I remembered reading the blog post I linked above when it got popular on reddit several months ago, and looked it up as an initial point of research.

However, I also found [a comment][2] on the article by the developer of Sublime:

> Sublime Text author here. Nice writeup! The actual algorithm that Sublime Text uses is similar in principle, but the implementation is rather different in the name of speed.
> 
> There are a couple of things you may want to tweak for better quality matching: you likely want to search more exhaustively for a better match, e.g., searching for "lll" in the UE4 filename list matches "SVisualLoggerLogsList.h", but not as well as it should, as it doesn't pickup all the upper case Ls. If you don't mind sacrificing a little speed, then it's also nice to handle transpositions, so that "sta" can still match against "SpawnActorTimer", just with a lower score - Sublime Text 3 does this.

The way this is phrased leads me to believe that Sublime uses an algorithm that finds the match positions yielding the maximum score, rather than just _a_ score. I'd heard of the [Levenshtein Distance][3] before, which does something similar, so I decided to look into that again.

## The Algorithm
In case you didn't know, this is the minimal number of _edits_ you have to perform on a string A to get another string B, where an edit can be a deletion ("main" -> "man"), an insertion ("man" -> "mane"), or a substition ("mane" -> "sane").

With some reasoning, you should be able to convince yourself that, given two strings matching the same pattern, the Levenshtein distance will always favour the shorter string. While that is one desirable property, it does not account for the 'quality' of the match, which includes the location of matching characters (abbreviations matching phrases) or the length of sequential matches (substrings matching longer strings).

While it doesn't make for a great heuristic by itself, calculating the Levenshtein distance falls in the same general category of finding the optimal configuration of matching characters. Since it's such a well understood problem, we can probably adapt an existing algorithm to our specific case.

[One such algorithm][4], by Wagner and Fischer, uses dynamic programming, essentially building a table of the distances between all possible pairs of prefixes of A and B, starting with two empty strings. This exploits the fact that the distance function can be expressed as

    dist(i, j) = if (A[i] == B[j]) {
        dist(i - 1, j - 1)
    } else {
        1 + min(dist(i - 1, j),
                dist(i, j - 1),
                dist(i - 1, j - 1))
    }

__Optimization Time!__ Since the value of a cell in the table only depends on the value to its immediate left, the value above it, and the value to the immediate left of _that_, we can actually discard all but two rows. We can also discard all values left of the diagonally adjacent cell; since we have yet to calculate the current cell, and everything to its right/below it, we can therefore get away with a ring buffer of size `m + 1`, where `m` is the length of the shorter string. This leaves us with a `O(n * m)` running time and `O(m)` required memory.

The cool thing to note here is that it's trivial to generalize the Wagner-Fischer algorithm for any function of the form

    f(i, j) = g(f(i - 1, j), f(i, j - 1), f(i - 1, j - 1))

where `g` can be _any_ function you want. In fact, you can allow `g` to access any cell in the ring buffer without additional cost. If it's really necessary, you could even buffer multiple rows - as long as it's a constant number, you'd still be running in linear space.

The problem then becomes expressing an ergonomic heuristic as such a function `g(...)`.

## The Heuristic
Before we actually worry about purposefully introducing bias, let's see if we can recreate the 'dumb' heuristic I use in the list search GUI for testing purposes:

    static int32_t
    tld_fuzzy_match_ss(String key, String val) {
        if (val.size < key.size) return 0;
        
        int i = 0, j = 0;
        while (i < key.size && j < val.size) {
            if ((char_is_upper(key.str[i]) && key.str[i] == val.str[j]) ||
                (key.str[i] == char_to_lower(val.str[j])))
            {
                ++i;
            }
            
            ++j;
        }
        
        if (i == key.size) return 1;
        return 0;
    }

Note that this does not produce a score at all; this function returns 1 if all the characters in the pattern can be found in the string, in the original order. Lower-case letters in the pattern will match with upper-case letters in the string, but the reverse is NOT true.

Now, remember that `match(i, j)` is supposed to be the score for the pair of _prefixes_ of length `i` (for the pattern) and `j` (for the candidate), respectively. That means that, if we already matched at `match(i, j - 1)`, we don't need to evaluate anything else. Otherwise we don't consider the pair a match, unless both the current character pair matches, and `match(i - 1, j - 1)` was true:

    match(i, j) = match(i, j - 1) || (match(i - 1, j - 1) && A[i] == B[j])

Since I haven't actually implemented any of this yet, this is where I take a break to do so, to check my reasoning. Temporarily storing a paragraph like this in the history of the article is one advantage of keeping the devblog in the same repository as the actual code! -- Turns out it works!

[1]: https://blog.forrestthewoods.com/reverse-engineering-sublime-text-s-fuzzy-match-4cffeed33fdb#.d05n81yjy
[2]: https://www.reddit.com/r/programming/comments/4cfz8r/reverse_engineering_sublime_texts_fuzzy_match/d1i7unr/
[3]: https://en.wikipedia.org/wiki/Levenshtein_distance
[4]: https://en.wikipedia.org/wiki/Wagner%E2%80%93Fischer_algorithm