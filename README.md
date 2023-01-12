# DATO
DATO (DATa Oriented), is a procedural programming language that focus on data oriented programming. Some of the inspirations for DATO are: C, C++, JAI, Assembly and others. The original DATO compiler is written in C.

## Goals
- [ ] Compiled to a native instruction set
- [ ] Turing-complete
- [ ] Self-hosted
- [ ] C function calls

## Quick Start
DATO code is separeted in sections, one for creating data and one for manipulating it. Heres an exemple:
```dato
data:
i32 x;

system:
i32 foo(i32 a, i32 b) {
	return a + b;
}
```
Code that manipulate data are called **systems**, these are the equivalent of a function or procedure in other languages.
