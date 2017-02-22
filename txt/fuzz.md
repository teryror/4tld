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

***

Since that is working fine, it is time to think about the precise constraints we'll want to express. In the first iteration of this chapter, I showed the skeleton of a heuristic that I thought would work, and derived the optimal values for all the weights, given some additional constraints.

The problem here was that I defined one of the constraints in a naive fashion, so it did not actually represent the properties I wanted to express. This time around, I started by defining constraints again, except I derived the constraints from actual examples, rather than informal definitions of the rules.

I implement the new heuristic in this commit, but leave the explanation for now. Here it is in the abstract:

    score(i, j) = if (score(i - 1, j - 1) && A[i] == B[j]) {
        let x = c + s
        
        max(score(i - 1, j - 1) + x, score(i, j - 1))
    } else {
        score(i, j - 1)
    }

Here, `x` is the 'value' of the match, `c` and `s` being bonuses for matching under certain conditions. If you read the article I linked in the introduction, you can probably guess what those conditions are; `s` is the __separator bonus__, awarded to matches following spaces, underscores, etc.

    let s = if (is_separator(B[j - 1])) S else 0

Where `S` is the actual weight we need to tune.

`c` is the __consecutive match bonus__. In my old heuristic, this looked much the same (`let c = if (A[i - 1] == B[j - 1]) Cb else Ca`), which is what caused one of my problems: You can weight this such that, up to a given pattern length, abbreviations will always be preferred to consecutive matches, despite `S < Cb - Ca`, which is necessary to ensure that one long sequence will always score higher than multiple sequences. However, I did not take into account that sequences will often start after a separator, giving them both bonuses, making it impossible for abbreviations to score higher than sequences.

Turns out you cannot have both while also keeping `Cb` constant. So I replaced it with

    let c = num_of_consecutive_matches(i, j)

You can implement that by nesting a third loop and counting the matches again, but I instead opted to fill an auxiliary table, which just needs a function of the familiar form:

    num_of_consecutive_matches(i, j) = if (A[i] == B[j]) {
        num_of_consecutive_matches(i - 1, j - 1) + 1
    } else {
        0
    }

This allows us to choose `S` rather freely, depending on the length of the longest abreviation we expect to see:

| `m` | `score(seq) - S = (m + 1) * (m / 2)` | `min(S) = (score(seq) - S) / (m - 1)` |
| ---:| ------------------------------------:| -------------------------------------:|
|   2 |                                    3 |                                     3 |
|   3 |                                    6 |                                     3 |
|   4 |                                   10 |                                 3 1/3 |
|   5 |                                   15 |                                 3 3/4 |
|   6 |                                   21 |                                 4 1/2 |
|   7 |                                   28 |                                 4 2/3 |
|   8 |                                   36 |                                 5 1/7 |
|   9 |                                   45 |                                 5 5/8 |
|  10 |                                   55 |                                 6 1/9 |

I feel like users entering a pure abbreviation longer than a couple characters seems unlikely, so we'll probably use `S = 4`.

If you paid close attention, you'll note that my new heuristic does not take match position into account. This is because my old one did, and I found it to be the cause of many unintuitive results given my data set: relative paths to files, where the part you're most likely trying to match is at the end of the string. By not introducing bias towards the end either, we make a simpler rule, which generalizes better.

***

As the quote by the Sublime dev indicates, matching transpositions might be nice. However, there's `(m * (m - 1)) / 2` of those, `m` being the length of the pattern, increasing the running time from `O(m * n)` to `O(m^3 * n)`. It wouldn't even be that useful, as typos usually only switch adjacent letters. I assume this is what he actually meant, and there's only `m` such permutations, making this much more feasible.

It might even be possible to account for this inside the scoring function; if that could be done in constant time, with no extra storage required, you're not exactly "sacrificing a little speed", as compared to any other constraint, though.

__Side Note:__ I think I actually found a way to approximate this, though it requires reading `score(i - 2, j - 1)`, therefore needs a larger ring buffer, and will also match strings that are missing characters from the pattern entirely. Additionally, I haven't even tested it, much less proven it to work, which I don't consider worthwhile considering the drawbacks. Figuring this out is left as an exercise for the reader (as I don't feel confident in posting claims based on hunches like this).

All in all, I think Sublime just iterates over all transpositions and runs the search with all of them, which is easily parallelized, and basically trivial compared to the actual search algorithm. Either that, linear searches inside the scoring function (which also matches his mention of sacrificing speed), or a different algorithm altogether.

## Optimizations
So, how can we make this go faster?

My implementation already uses one optimization: given that the rows up to the rows of the score table up to the first match do not vary from the initial row at all, we can skip ahead in the string, potentially exiting early if there are not enough characters left to produce a full match.

We can be even more clever and skip evaluation of a number of cells: Consider the following table:

    j\i  0 1 2 3 4 5
           p a t r n
     0   1 0 0 0 0 0  // This row is constant
     1 p 1 x 0 0 0 0
     2 a 1 x x 0 0 0
     3 t 1 x x x 0 0
     4 t 1 x x x x 0
     5 e 1 x x x x x
     6 r 1 x x x x x
     7 n 1 x x x x x
     8   1 x x x x x
     9 m _ x x x x x
    10 a _ _ x x x x
    11 t _ _ _ x x x
    12 c _ _ _ _ x x
    13 h _ _ _ _ _ X

Since the first row is constant, the triangle of `0` at the top right is constant as well, while the triangle of `_` at the bottom left does not affect `X`, the score of the match, at all. This is because, with our current heuristic, the value of a given cell only depends on the cell above it, and the cell the immediate left of _that_. That means that we can shave off an additional `m^2` evaluations of our heuristic by reshaping the loop boundaries of the algorithm. (__Note:__ this is the first one I actually measured. I didn't do any rigorous, statistical analysis, but it gave an estimated speedup of about 50%).

Even more specific to our application, we can exploit the fact that we are building up the search pattern incrementally; appending a character to the pattern will never turn a non-match into a match, so we can reduce the number of strings we attempt to match as pattern length increases. We can provide results for pattern prefixes (i.e. support instant backspace) by storing them. I will try this option last, as it does not apply to applications outside of the fuzzy list panels, and would require a significant change to my query code. (__Note:__ this turned out to be even more effective than I imagined. I'm now interactively searching the list I used for testing in about 0.1% of the time it took when I started optimizing).

There's more hardware-oriented optimizations to consider as well: We could try to rephrase our heuristic to use as few branches as possible, maybe even none at all. This would prevent pipeline flushes due to branch prediction going wrong.

Doing so would also open up the path to using SIMD instructions, allowing evaluation of up to 16 cells at a time. While probably a really fun problem in itself, I doubt we're going to be doing this, since it imposes some quite heavy restrictions on the rest of the code, and even the score itself, which would probably have to fit in an unsigned byte value, which is cutting it really close, considering its maximum value is given as a quadratic equation.

If your use case allows preprocessing, while also requiring the performance boost from indexing, you should look into [BK-Trees][5] and [Levenshtein Automata][6], one of which could probably be modified to support our custom heuristics. I haven't looked into that yet, as a list of filenames is extremely volatile, and is therefore rebuilt on demand in my code.

[1]: https://blog.forrestthewoods.com/reverse-engineering-sublime-text-s-fuzzy-match-4cffeed33fdb#.d05n81yjy
[2]: https://www.reddit.com/r/programming/comments/4cfz8r/reverse_engineering_sublime_texts_fuzzy_match/d1i7unr/
[3]: https://en.wikipedia.org/wiki/Levenshtein_distance
[4]: https://en.wikipedia.org/wiki/Wagner%E2%80%93Fischer_algorithm
[5]: http://blog.notdot.net/2007/4/Damn-Cool-Algorithms-Part-1-BK-Trees
[6]: http://blog.notdot.net/2010/07/Damn-Cool-Algorithms-Levenshtein-Automata