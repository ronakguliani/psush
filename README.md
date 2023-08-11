# psush: a Unix Shell

This project implements a Unix shell in C, providing users with a command-line interface to interact with their operating system. The shell supports executing both built-in commands and external programs, input/output redirection, piping, and maintains a command history.

## Features
- **Command Execution**: Execute an array of commands, from fundamental built-ins like "cd," "echo," and "history" to other external programs

- **Input/Output Redirection**: Have input and output redirection using the `<` and `>` operators. Seamlessly redirect data streams between your commands and files, enhancing data manipulation

- **Sophisticated Command Piping**: Step into the realm of command orchestration with the `|` operator. Create  pipelines that  transport data between commands

- **Verbose Mode**: Higher level of command execution understanding with the `-v` command-line option

## Getting Started

1. **Build the Project**: Open your terminal and navigate to the project directory. Then, simply type:
```
make
```

Once the project is compiled, run the shell by typing:
```
./psush
```
