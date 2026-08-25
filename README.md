C-Shell

A simple custom shell implemented in C for Linux/WSL. It supports command parsing, built-in commands, pipes, input/output redirection, and other shell functionality.
How to Run

Open the terminal in the project root directory:
cd C-shell
Compile the project:
make
Run the shell:
./cshell
Alternatively:
make run

Main Features
Command execution
Command parsing and lexing
hop
reveal
peek
locate
Pipes (|)
Input redirection (<)
Output redirection (> and >>)
Multiple commands and redirections
Project Structure
C-shell/
├── include/     # Header files
├── src/         # C source files
├── Makefile
└── README.md
