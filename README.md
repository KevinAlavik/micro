# Micro Programming Languge
Micro is a small low-level programming language designed for systems programming.

## Design
The design is wip, but i guess this could be called a subset of C. Even tho it doesnt comply with any C standards, it uses nearly the exact same grammar as C. It produces QBE IL (https://c9x.me/compile/doc/il.html) which later gets assembled and linked by clang.

## Why?
I want a basic low-level language that is a little bit more comfortable than C, without big runtimes. Since this is made primarly for freestanding envoirnments and kernel development.