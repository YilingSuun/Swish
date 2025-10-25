# Swish: Simple Working Implementation Shell

## Introduction
This progeam is a simplified shell called **swish**.
This program is a use of system calls, focusing on process creation, process management, I/O, and signal handling.  
While a single system call may seem simple, the real challenge of systems programming lies in combining system calls to build powerful tools.  

A shell is the program you interact with in a terminal. It starts processes, manages I/O, and handles user interaction.  
Our shell `swish` is less feature-rich than `bash`, but will implement several key features.

---

## 1.⚙️ Installation
Here's some code you can use on the command line:

1. Clone the repository
```bash
git clone https://github.com/YilingSuun/Swish.git
```  
<br>
2. Enter the directory

```bash
cd YilingSuun/Swish
```  
<br>
3. Compile the shell

```bash
make
```  
<br>
4. Run the shell

```bash
./swish
```  
<br>
After running, the prompt will display as:

```python
@>
```

---

## 2.🎮 Interact with swish
**Some Examples**
- In your command line, use:
```bash
# Display the path of the current working directory
@> pwd
# Change the current working directory
@> cd /tmp
```
- Run external programs:
```bash
# List the files and subdirectories in the current directory
@> ls -l > out.txt
# Search for lines in file.txt that contain the string "foo"
@> grep "foo" file.txt
```

- Redirect input/output:
```bash
# Write the command output to the "out.txt" file instead of printing it to the terminal
@> ls -l > out.txt
# Pass the contents of input.txt as input to wc -l, count the number of lines in the file
@> wc -l < input.txt
```

- Use background jobs:
```bash
# Run the "long_running_program" in the background without blocking the current shell
@> ./long_running_program &
# List all the running background tasks along with their corresponding numbers
@> jobs
#Bring the background task numbered 0 back to the foreground for execution
@> fg 0
```

---

## 3.✨ Features
This program covers several important systems programming topics:

- String tokenization using `strtok()`  
- Working directory management with `getcwd()` and `chdir()`  
- Program execution using `fork()` and `execvp()`  
- Process management with `wait()` and `waitpid()`  
- I/O redirection with `open()` and `dup2()`  
- Signal handling with `setpgid()`, `tcsetpgrp()`, and `sigaction()`  
- Foreground and background job control using signals and `kill()`  

---

## 4.✨ Makefile Commands
- `make` : Compile and produce `swish` executable  
- `make clean` : Remove compiled files  
- `make clean-tests` : Remove files created during tests  
- `make zip` : Create submission zip for Gradescope  
- `make test` : Run all test cases  
- `make test testnum=5` : Run test case #5 only  

---

## 5.✨ Feature and Tasks

### 1. String Tokenization
- Implement `tokenize()` in `swish_funcs.c` using `strtok()`  
- Add each token into a `strvec_t` vector  
- Example: input `ls -l -a` → tokens `[ls, -l, -a]`  

### 2. Working Directory Management
- Implement built-in commands:
  - `pwd` → prints current working directory  
  - `cd [dir]` → changes directory, defaults to `$HOME` if no argument  
- Use `getcwd()`, `chdir()`, and `getenv("HOME")`  

### 3. Running Commands
- If not a built-in command:
  - `fork()` a child process  
  - In child: `run_command()` → build `argv[]`, call `execvp()`  
  - In parent: wait for child with `waitpid()`  

### 4. I/O Redirection
- Detect `<`, `>`, `>>` tokens in input  
- Use `open()` and `dup2()` to redirect stdin/stdout  
- Operators and filenames must not be passed to `execvp()`  

Examples:  
```bash
ls -l > out.txt
cat < file.txt
wc -l < input.txt > output.txt
```

### 5. Basic Signal Management
- Use `setpgid()` to move child into new process group  
- Parent uses `tcsetpgrp()` to give child foreground control  
- Restore control to parent after child terminates  
- In child: reset `SIGTTOU` and `SIGTTIN` to default handlers  

### 6. Stopped Processes (Jobs)
- Detect stopped jobs using `waitpid(..., WUNTRACED)`  
- Store jobs in `job_list_t`  
- Implement `fg <index>` command to resume stopped jobs in foreground  
- Resume job steps:
  - `tcsetpgrp()` to move job to foreground  
  - `kill(SIGCONT)` to resume  
  - `waitpid()` until job terminates/stops again  

### 7. Background Jobs
- Support running commands with `&` at the end  
- Add background job to `job_list_t`  
- Extend `resume_job()` to support `bg <index>`  
- Implement:
  - `wait-for <index>` → wait for one background job  
  - `wait-all` → wait for all background jobs  

---

## 6. Starter Code Overview
| File              | Purpose                                   |
|-------------------|-------------------------------------------|
| **Makefile**      | Build system                              |
| **string_vector.c/h** | Resizable array for tokens            |
| **job_list.c/h**  | Linked list of jobs                       |
| **swish.c**       | Main shell loop                           |
| **swish_funcs.c/h** | Helper functions for shell features     |
| **testius**       | Script to run automated tests             |
| **test_cases/**   | Folder with test inputs/outputs           |


---


## Notes
- Some features (signals, job control) may behave differently outside the sanctioned Docker environment  
