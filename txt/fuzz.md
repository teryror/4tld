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

Since that is working fine, it is time to think about the precise constraints we'll want to express, so let's recap what we know from the sources I provided in the introduction:

* Consecutively matched characters are better than individual matches. Ideally, one long sequence is better than multiple sequences, even if they sum to the same length.
* Matches following a separator (` `, `_`, `-`, `.`, `/`, `\\`), as well as capital matches following a lower case letter, are better than matches in the middle of a word. How much more depends on how you want to weigh abbreviations against sequences.
* The first match is worth more depending on how close it is to the start of the string. Other than this, unmatched characters do not incur a penalty (the first blog post says otherwise, but one commentor in the reddit discussion provides evidence that this is a bad idea).

Since we won't be dealing with transpositions for now (see the next section for my reasoning for this), we should be able to come up with a scoring system to satisfy these constraints. 

To get the first two, we first have to revisit the `match` function from the previous section, and make the `score` function, which will basically just give the pattern length if the string matches, and 0 otherwise. We'll also rephrase it a little bit to make the upcoming extensions easier to understand:

    score(i, j) = if (score(i - 1, j - 1) && A[i] == B[j]) {
        let x = 1
        
        max(score(i - 1, j - 1) + x, score(i, j - 1))
    } else {
        score(i, j - 1)
    }

Here, `x` is the value of the match. Our goal now is to find a value for `x`. We'll use named constants for now, and try to come up with reasonable values for them once we can see how they will interact:

    let x = c + s + p

Let's take this apart further:

* `c` is the __consecutive match bonus__. This is actually the most tricky one of the three - if we want to satisfy what I called the _ideal_ property above, that is. Otherwise, we can just choose some constant to apply conditionally here.
  
  However, if we want one long sequence to be worth more than two that are half its length, we need to determine the sequence's length somehow. Obviously, we could scan back linearly to do this on the fly, incurring a potentially heavy performance cost. We could also fill a second table, keeping track of the match lengths, giving us free reign at the cost of additional memory. One idea I had, that turned out to be wrong, was to make this a multiplicative bonus, i.e. `let c = if (A[i - 1] == B[j - 1]) C * score(i - 1, j - 1) else 0`, where `C`is the __consecutive bonus coefficient__.
  
  I decided to make my life easier for now, and go with `let c = if (A[i - 1] != B[j - 1]) Ca else Cb`, i.e. the first match in a sequence is worth `Ca` points, all subsequent matches are worth `Cb` points.
* `s`, the __separator bonus__, is probably the most obvious: `let s = if (is_separator(B[j - 1])) S else 0`, where `S` is the actual constant we care about. Extending the `is_separator` function to detect `camelCase` should be trivial.
* `p` is the the __proximity bonus__, awarded only to the first match. You could awkwardly define this conditionally on `i`, but we'll just say that `score(0, j) = max(P - j, 1)`, where `P` is the __maximal proximity bonus__. I chose 1 as the second parameter because I want the empty pattern to match all strings. You can also imagine applying other functions to this term, e.g. if you want the bonus to fall of exponentially, rather than linearly.

If you want to go straight to the tuning of the constants, skip the next two sections.

***

As the quote by the Sublime dev indicates, matching transpositions might be nice. However, there's `(m * (m - 1)) / 2` of those, `m` being the length of the pattern, increasing the running time from `O(m * n)` to `O(m^3 * n)`. It wouldn't even be that useful, as typos usually only switch adjacent letters. I assume this is what he actually meant, and there's only `m` such permutations, making this much more feasible.

It might even be possible to account for this inside the scoring function; if that could be done in constant time, with no extra storage required, you're not exactly "sacrificing a little speed", as compared to any other constraint, though.

__Side Note:__ I think I actually found a way to approximate this, though it requires reading `score(i - 2, j - 1)`, therefore needs a larger ring buffer, and will also match strings that are missing characters from the pattern entirely. Additionally, I haven't even tested it, much less proven it to work, which I don't consider worthwhile considering the drawbacks. Figuring this out is left as an exercise for the reader (as I don't feel confident in posting claims based on hunches like this).

All in all, I think Sublime just iterates over all transpositions and runs the search with all of them, which is easily parallelized, and basically trivial compared to the actual search algorithm. Either that, linear searches inside the scoring function (which also matches his mention of sacrificing speed), or a different algorithm altogether.

***

Now, back to our scoring function -- it's time to tune the parameters! This might actually be an interesting machine learning problem, but I feel I'm deep enough in this rabbit hole already, so I'll leave this as another exercise for the reader.

Let's focus on `Ca` and `Cb` first. Let's rephrase the 'ideal property' I posited earlier: `c_total(seq_len) > sum(map(k, c_total))`, where `c_total(seq_len) = Ca * (seq_len - 1) * Cb`, and `sum(k) == seq_len`. It seems obvious to me that `c_total(seq_len) - sum(map(k, c_total)) = (Cb - Ca) * len(k)`, though I haven't actually proven it. Given that, however, the 'ideal property' would hold as long as `Cb > Ca`.

Next, we'll consider a simple case to determine constraints on `S`: given a pattern A of length `m`, and two strings B and C, both of which can match A in exactly one way; B containing A as a substring, and C containing `m` words, each starting with the respective character in A. What's the cutoff point `M`, where you would like one to overtake the other?

    c_total(M) = Ca + (Cb * M) - Cb
    s_total(M) = Ca * M + S * M
    
            c_total(M) = s_total(M)
    Ca + (Cb * M) - Cb = Ca * M + S * M    | /M
      Ca/M + Cb - Cb/M = Ca + S            | -Ca
                     S = Ca/M - Ca + Cb - Cb/M
                     S = Ca * (1/M - 1) + Cb * (1 - 1/M)
                     S = (M - 1) / M * (Cb - Ca)

I'd like to keep the average score as low as possible, to simplify a few things in the implementation, so I'll arbitrarily set `Ca = 1`, giving `Cb = M + 1` and `S = M - 1` (you could also set `Cb = z * M + 1, S = z * (M - 1)`, if you desired a larger `Cb - Ca` for some reason). With that, we defined one easy-to-grasp parameter to tune three of our constants with. That just leaves `P`.

However, I'm tired, and will derive its optimal value in the next commit -- or not, as I just had a stroke of inspiration concerning how to optimize this, and want to put that down real quick.

## Optimizations
We now have a heuristic to give us a decent score for any given pattern vs. any given string, and an algorithm to evaluate it. Now, can we make it faster?

I can think of three optimizations - two of which are mutually exclusive (unless we're trying to match extremely long patterns), while one will compound equally with either of the two, by potentially recognizing non-matches more quickly.

Starting with that last one, since the first row of the table (full pattern vs. empty string) is constant, and, in our heuristic at least, the only value that will change in the iterations before the first match is the first cell (representing the empty pattern vs. string prefix of length `j`, which is computed independently of the actual heuristic), we can skip evaluating all rows before the first match, by linearly scannnig the string. We can then also insert an early exit, if there are not enough characters left in the string to match the full pattern.

The first of the two other optimizations relies on a similar realization that some cells don't need to be evaluated. Consider the following table:

    j\i  0 1 2 3 4 5
           p a t r n
     0   1 0 0 0 0 0  // This row is constant
            \|\|\|\|
     1 p 1 x 0 0 0 0
              \|\|\|
     2 a 1 x x 0 0 0
                \|\|
     3 t 1 x x x 0 0
                  \|
     4 t 1 x x x x 0
     5 e 1 x x x x x
     6 r 1 x x x x x
     7 n 1 x x x x x
     8   1 x x x x x
          \|\|\|\|\|
     9 m _ x x x x x
            \|\|\|\|
    10 a _ _ x x x x
              \|\|\|
    11 t _ _ _ x x x
                \|\|
    12 c _ _ _ _ x x
                  \|
    13 h _ _ _ _ _ X

The `\|` indicate data dependencies. Since the first row is constant, the triangle of `0` at the top right is constant as well, while the triangle of `_` at the bottom left does not affect `X`, the score of the match, at all. That means that we can shave off an additional `m^2` evaluations of our heuristic by reshaping the loop boundaries of the algorithm.

The alternative is to use SIMD instructions to evaluate 16 cells of the table at a time, though this does require reshaping our heuristic again, to use bitwise operations and other math tricks to get rid of the conditional operations. I haven't tried doing that yet, but I feel like it should be possible.

This concludes the topic of optimization for now, we'll come back to it once I actually implemented the heuristic and can profile all our options here.

[1]: https://blog.forrestthewoods.com/reverse-engineering-sublime-text-s-fuzzy-match-4cffeed33fdb#.d05n81yjy
[2]: https://www.reddit.com/r/programming/comments/4cfz8r/reverse_engineering_sublime_texts_fuzzy_match/d1i7unr/
[3]: https://en.wikipedia.org/wiki/Levenshtein_distance
[4]: https://en.wikipedia.org/wiki/Wagner%E2%80%93Fischer_algorithm
