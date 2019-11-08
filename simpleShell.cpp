/*
 * Copyright(C) 2019 zampaze@miamioh.edu
 */

/* 
 * File:   zampaze_hw4.c++
 * Author: zackz
 *
 * Created on September 18, 2019, 5:31 PM
 * 
 * Create a custom shell
 */


#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <cstdlib>

using namespace std;
using StrVec = std::vector<std::string>;

const int READ  = 0;
const int WRITE = 1;

/**
 * Print the command in the vector
 * @param vec String Vector
 */
void printCommand(StrVec& vec) {
    std::cout << "Running:";
    for (auto s : vec) {
        std::cout << " " << s;
    }
    std::cout << std::endl;
}

/**
 * Trim a string from front extraneous input and return it
 * @param str
 * @return 
 */
std::string trimmer(std::string& str) {
    const std::string chars = "\t\n\v\f\r";  // what to look for
    // trim the front of the string first (do not want extraneous chars read)

    return str.erase(0, str.find_first_not_of(chars));
}

/**
 * Checks if a string starts with a shell comment: # or is empty
 * @param str
 * @return true if it starts with a comment
 */
bool commentCheck(std::string& str) {
    return (str == "" || str.front() == '#');
}

/**
 * See if process SERIAL, PARALLEL file or regular
 * If regular, put input back in buffer
 * @param in
 * @param type
 */
void whatProcess(std::istream& in, std::string& type) {
    if (type != "SERIAL" && type != "PARALLEL") {
        for (int i = type.size() - 1; i >= 0; i--) {
            in.putback(type.at(i));
        }
    }
}

/**
 * Check if a process will be PARALLEL SERIAL or regular
 * @param is
 * @return 
 */
char checkProcessType(std::istringstream& is) {
    std::string type;
    is >> std::quoted(type);

    if (type == "PARALLEL") {
        return 'p';
    } else if (type == "SERIAL") {
        return 's';
    } else if (type == "exit") {
        return 'e';
    } else {
        whatProcess(is, type);
        return 'r';
    }
}

/**
 * Move the argument to the command vector and remove from overall vector
 * @param cmd Command Vector
 * @param inputVec Overall Vector
 */
void moveArgToCommand(StrVec& cmd, StrVec& inputVec, size_t index) {
    cmd.push_back(inputVec.at(index));
    inputVec.erase(inputVec.begin());  // remove from main input vector
}

/**
 * Convenience method to setup arguments (as pointers) 
 * and call the execvp system call
 * 
 * @param argList The list of arguments with the 1st entry always
 * being the path to the full command to be run.  
 */
void myExec(StrVec& argList) {
    std::vector<char*> args;  // list of pointers to each word
    for (auto& s : argList) {
        args.push_back(&s[0]);  // address of 1st character in string
    }
    // add the nullptr to the end
    args.push_back(nullptr);


    execvp(args[0], &args[0]);
}

/**
 * Run the actual shell process from the input commands
 * @param inputVec
 */
void runShell(StrVec& inputVec) {
    const int pid = fork();
    int status;
    if (pid == 0) {
        // Child process: Run the Command
        myExec(inputVec);  // Execute Commands
        // here if fails
        perror("execve failed");
        exit(-1);  // force exit with the child program with error: failed
    } else if (pid > 0) {
        // Parent process: Wait for the Child
        waitpid(pid, &status, 0);
        std::cout << "Exit code: " << status << std::endl;
    }
}

/**
 * Run a fork in parallel
 * @param pids
 * @param indCmd
 */
void runParallelFork(std::vector<int>& pids, StrVec& indCmd) {
    int pid = fork();
    pids.push_back(pid);
    if (pid == 0) {
        // Child process: Run the Command
        myExec(indCmd);  // Execute Commands
        // here if fails
        perror("execve failed");
        exit(-1);  // force exit with the child program with error: failed
    }
}


/**
 * Run the program in parallel when given the command
 * @param args
 */
void parallel(StrVec& inputVec, char process) {
    if (inputVec.size() == 0 || process != 'p') {
        return;
    }
    // parallel so process
    std::vector<int> pids;  // hold pids to call waitpid on each

    // loop while commands to run exist
    while (inputVec.size() > 0) {
        StrVec indCmd;  // individual command string to run
        for (size_t i = 0; i < inputVec.size();) {
            // separate into individual command sequences
            if (inputVec.at(i) == "next_command") {
                inputVec.erase(inputVec.begin());
                break;
            }
            // add to individual command vector
            moveArgToCommand(indCmd, inputVec, i);
        }

        printCommand(indCmd);  // print what will be ran

        runParallelFork(pids, indCmd);
    }


    for (auto pid : pids) {
        int status;
        waitpid(pid, &status, 0);
        std::cout << "Exit code: " << status << std::endl;
    }
}

/**
 * Run a program in SERIAL
 * @param inputVec
 * @param process
 */
void serial(StrVec& inputVec, char process) {
    if (inputVec.size() == 0 || process != 's') {
        return;
    }
    // serial so process
    // loop while commands to run exist
    while (inputVec.size() > 0) {
        StrVec indCmd;  // individual command string to run
        for (size_t i = 0; i < inputVec.size();) {
            // separate into individual command sequences
            if (inputVec.at(i) == "next_command") {
                inputVec.erase(inputVec.begin());
                break;
            }
            // add to individual command vector
            moveArgToCommand(indCmd, inputVec, i);
        }
        printCommand(indCmd);  // print what will be ran

        runShell(indCmd);  // run the individual command
    }
}

/**
 * Launch the actual process for the pipe
 * @param cmd
 * @param rw
 * @param pipefd
 * @return returns the pid
 */
int runPipe(StrVec& cmd, const int rw, int pipefd[]) {
    // Fork -- make child process
    const int pid = fork();
    if (pid != 0) {
        return pid;  // Parent process has nothing to do so return
    }
    
    // now in child process
    
    // Close the ends of pipe not used
    close(pipefd[!rw]);
    // Tie end of the pipe to the input or output
    dup2(pipefd[rw], rw);
    // Raw args to be passed to command
    std::vector<char*> args;   
    for (std::string& arg : cmd) {
        args.push_back(&arg[0]);  // Add pointer to argument
    }
    args.push_back(nullptr);
    // Run the command
    execvp(args[0], &args[0]);
    return -1;  // if this is reached, there is an error
}

/**
 * Open the pipes and close the pipes when finished running
 * @param cmd1
 * @param cmd2
 */
void openClosePipes(StrVec& cmd1, StrVec& cmd2) {
    int pipefd[2];
    pipe(pipefd);
    const int pid1 = runPipe(cmd1, WRITE, pipefd);
    const int pid2 = runPipe(cmd2, READ,  pipefd);
    
    // wait for commands to finish
    waitpid(pid1, nullptr, 0);
    close(pipefd[1]);  // close pipe so second command knows input is done
    waitpid(pid2, nullptr, 0);
}


/**
 * Piped command so split into respective commands and then run
 * @param inputVec
 * @param process
 */
void piped(StrVec& inputVec, char process) {
    if (inputVec.size() == 0 || process != '|') {
        return;
    }

    // piped so process
    // separate into commands
    StrVec cmd1;
    StrVec cmd2;
    bool firstCmd = true;
    while (inputVec.size() > 0) {
        if (firstCmd) {
            if (inputVec.at(0) == "|") {
                // first command is done -- hit pipe
                firstCmd = !firstCmd;
                inputVec.erase(inputVec.begin());  // remove pipe
            } else { 
                moveArgToCommand(cmd1, inputVec, 0);
            }
        } else { 
            moveArgToCommand(cmd2, inputVec, 0);
        }
    }

    // Run the commands
    openClosePipes(cmd1, cmd2);
}

/**
 * Process the given command
 * @param inputVec
 * @param process
 */
void processCommand(StrVec& inputVec, char process) {
    if (process == 'r') {
        // regular process
        runShell(inputVec);
    } else if (process == 'p') {
        // PARALLEL process
        parallel(inputVec, process);
    } else if (process == '|') {
        // piped process
        piped(inputVec, process);
    } else {
        // SERIAL
        serial(inputVec, process);
    }
}

void checkIfPiped(char& process, StrVec& inputVec) {
    auto res = std::find(std::begin(inputVec), std::end(inputVec), "|");
    if (res != std::end(inputVec)) {
        process = '|';
    }
}

/**
 * Process the input from a user and returns it as a String Vector
 * @param input stream
 * @return String vector
 */
StrVec inputProcessing(std::istream& in, char& process) {
    StrVec inputVec;  // command vector
    std::string item;
    if (process == 'r') {
        // regular process - finish reading the single line
        while (in >> std::quoted(item)) {
            inputVec.push_back(item);
        }
        // check if it is piped at all
        checkIfPiped(process, inputVec);
    } else {
        // process is PARALLEL or SERIAL - read from file line by line
        std::string line;
        while (std::getline(in, line)) {
            line = trimmer(line);
            if (line.size() == 0 || line.at(0) == '#') {
                // skip this as it is a comment or empty line
            } else {
                std::istringstream is(line);
                while (is >> std::quoted(item)) {
                    inputVec.push_back(item);
                }
                // signal when reading command end
                inputVec.push_back("next_command");
            }
        }
    }

    return inputVec;
}

/**
 * The Main While Loop Process
 * @param std::string line
 * @param StrVec inputVec
 * @return 
 */
void processLoop(std::string& line, StrVec& inputVec, bool& exit) {
    if (commentCheck(line)) {
        // this is a comment or blank line
    } else {
        // this is not a comment or blank line: PROCESS
        // create stringstream of line
        std::istringstream is(line);

        // check if serial or parallel then process accordingly
        char process = checkProcessType(is);
        if (process == 'p') {
            // PARALLEL - process commands through file
            std::string file;
            is >> std::quoted(file);
            std::ifstream in(file);
            inputVec = inputProcessing(in, process);
        } else if (process == 's') {
            // SERIAL - process commands through file
            std::string file;
            is >> std::quoted(file);
            std::ifstream in(file);
            inputVec = inputProcessing(in, process);
        } else if (process == 'r') {
            // regular -- process this single line
            inputVec = inputProcessing(is, process);
            printCommand(inputVec);
        } else {
            // exit command
            exit = true;
            return;
        }
        processCommand(inputVec, process);
    }  // end process block     
}

/*
 * The launch point for the program
 */
int main(int argc, char** argv) {
    std::string line;
    while (std::cout << "> ", std::getline(std::cin, line)) {
        StrVec inputVec;

        line = trimmer(line);  // trim extraneous input

        // check if comment or blank
        bool exit = false;
        processLoop(line, inputVec, exit);

        if (exit) {
            // exit command given, quit
            break;
        }
    }  // end while loop
    return 0;
}

