# TimedLine

Insert in a line the time it takes to write to the terminal.

```console
$ for i in {1..5} ; do
>   sleep 0.$(($RANDOM%1000)) # waits between 0 and 1000 milliseconds
>   echo num $i
> done | ./timedline '[%ss] %i'
[0.375s] num 1
[0.527s] num 2
[0.783s] num 3
[0.159s] num 4
[0.203s] num 5
```

## Format

- `%%`: an `%` character
- `%i`: input text
- `%t`: time in hh:mm:ss.ms format
- `%s`: time in seconds
- `%m`: time in milliseconds
- `%u`: time in microseconds
- `%nF`: with
   - `n`: An integer corresponding to the minimum size of the displayed number. Spaces (or zeros if the number begins with 0) are placed to the left.
   - `F`: `s`, `m` or `u` formats.


# Compilation

```sh
g++ -Wall -Wextra -std=c++17 -DNDEBUG -O3 -flto -fno-exceptions -fuse-ld=gold -s timedline.cpp -o timedline
```
