## A single header library for playing Protracker mod files

* Simple interface
* 1000 lines of C
* Dual license MIT / Public Domain
* Only supports .mod files for now
* Sounds just like 1992 all over again

example.c contains a simple example program showing usage.
Loads a mod file and saves out the first 30 seconds as a wav file.

Compile with:
 gcc example.c -o example -std=c99

Run:
 example <modfile.mod>


A large selection of example mod files can be found [here](https://modarchive.org/)
