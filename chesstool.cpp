#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

struct SyncPts {
  int current;
  int total;
};

void check_arguments(int, char**);
void initialize_chess_tool();
void first_execution();
int run_command(string);
SyncPts read_sync_pts();
int read_total_sync_pts();
int read_current_sync_pts();
void explore_program();
void print_crash_report(int*);
void print_oreo_cookie();

static const char*                                      RUN_SH = "./run.sh ";
static const char*                                      TRACK_SYNC_PTS_FILE_NAME = ".tracksyncpts";
static const char*                                      TEST_PROGRAM;
static int                                              TOTAL_SYNC_PTS = -1;

int main(int argc, char *argv[])
{
  check_arguments(argc, argv);

  initialize_chess_tool();

  first_execution();

  explore_program();

  return 0;
}

// Check for program arguments, exit if invalid
void check_arguments(int argc, char *argv[])
{
  if (argc != 2 || !*argv[1]) {
    fprintf(stderr, "Invalid arguments provided to chesstool.\nUsage: ./chesstool <binaryfile>\n");
    exit(0);
  }
  TEST_PROGRAM = argv[1];
}

// Initialize tracking file with initialization values
void initialize_chess_tool()
{
  fprintf(stderr, "Initializing CHESS tool...\n");
  ofstream outfile;
  outfile.open(TRACK_SYNC_PTS_FILE_NAME, ios_base::trunc);
  outfile << "0/0";
  outfile.close();
}

// Find synchronization points and write to tracking file
void first_execution()
{
  fprintf(stderr, "========== Finding Synchronization Points ==========\n");

  string firstExecutionCommand = RUN_SH;
  firstExecutionCommand.append(TEST_PROGRAM);
  run_command(firstExecutionCommand);

  SyncPts pts;
  try {
    pts = read_sync_pts();
  } catch (int e) {
    fprintf(stderr, "!!!!!!!!!! Terminating CHESS tool !!!!!!!!!!\n\n");
    exit(0);
  }

  fprintf(stderr, "========== Finding Synchronization Points Complete ==========\n\n");

  TOTAL_SYNC_PTS = pts.total;
}

// Helper method for executing bash script
int run_command(string command)
{
  const char *cmd = command.c_str();
  // fprintf(stderr, "Running command: %s\n", cmd);
  return system(cmd);
}

// Read tracking file and returns SyncPts struct
SyncPts read_sync_pts()
{
  string input;
  ifstream infile;
  infile.open(TRACK_SYNC_PTS_FILE_NAME);

  if (!infile) {
    fprintf(stderr, "Error: No data in %s\n", TRACK_SYNC_PTS_FILE_NAME);
    throw -1;
  }

  while (!infile.eof())
    infile >> input;
  infile.close();

  int len = input.length();
  int DELIMITER_POS = input.find('/');
  if (DELIMITER_POS == (int)string::npos) {
    fprintf(stderr, "Error: Corrupt data in %s\n", TRACK_SYNC_PTS_FILE_NAME);
    throw -1;
  }

  string current = input.substr(0, DELIMITER_POS);
  string total = input.substr(DELIMITER_POS + 1, len);

  SyncPts syncPts;
  syncPts.current = atoi(current.c_str());
  syncPts.total = atoi(total.c_str());

  return syncPts;
}

// Read tracking file and returns the total synchronization points found
int read_total_sync_pts()
{
  SyncPts pts = read_sync_pts();
  return pts.total;
}

// Read tracking file and returns the current synchronization point found
int read_current_sync_pts()
{
  SyncPts pts = read_sync_pts();
  return pts.current;
}

// Write to tracking file with values in SyncPts argument
void update_track_sync_pts_file(SyncPts pts)
{
    string newString;
    stringstream temp1;
    stringstream temp2;
    temp1 << pts.current;
    newString.append(temp1.str());
    newString.append("/");
    temp2 << pts.total;
    newString.append(temp2.str());

    // Write back to file
    ofstream outfile;
    outfile.open(TRACK_SYNC_PTS_FILE_NAME);

    if (outfile)
        outfile.write(newString.c_str(), (int)newString.length());

    outfile.close();
}

// Use CHESS algorithm to explore test program
// If program does not return appropriate status code, we assume a crash occurred
// Iterate through each synchronization point for a total of TOTAL_SYNC_PTS
void explore_program()
{
  int current = read_current_sync_pts();
  int *crashes = (int *)malloc(sizeof(int) * TOTAL_SYNC_PTS);
  int numCrashes = 0;

  while (current <= TOTAL_SYNC_PTS) {
    string NthExecutionCommand = RUN_SH;
    NthExecutionCommand.append(TEST_PROGRAM);

    fprintf(stderr, "========== Executing program %d/%d ==========\n", current, TOTAL_SYNC_PTS);
    int statusCode = run_command(NthExecutionCommand);

    if (statusCode != 0) {
      fprintf(stderr, "!!!!!!!!!! Program crashed at synchronization point %d/%d !!!!!!!!!!\n\n", current, TOTAL_SYNC_PTS);
      crashes[numCrashes++] = current;
    } else {
      fprintf(stderr, "========== Execution complete ==========\n\n");
    }

    current++;
  }

  crashes[numCrashes] = '\0';

  print_crash_report(crashes);
}

// Print crash report
void print_crash_report(int *crashes)
{
  fprintf(stderr, "========== Crash Report Begin ==========\n");

  if (!*crashes) {
    fprintf(stderr, "No crash occurred! You get a cookie.\n");
    print_oreo_cookie();
  }

  for (int i = 0; crashes[i] != '\0'; i++) {
    fprintf(stderr, "Crash occurred at synchronization point %d/%d\n", crashes[i], TOTAL_SYNC_PTS);
  }

  fprintf(stderr, "========== Crash Report End ==========\n");
}

// ASCII art - oreo cookie
void print_oreo_cookie()
{
  fprintf(stderr, "         _.:::::._\n");
  fprintf(stderr, "       .:::'_|_':::.\n");
  fprintf(stderr, "      /::' --|-- '::\\\n");
  fprintf(stderr, "     |:\" .---\"---. ':|\n");
  fprintf(stderr, "     |: ( O R E O ) :|\n");
  fprintf(stderr, "     |:: `-------' ::|\n");
  fprintf(stderr, "      \\:::.......:::/\n");
  fprintf(stderr, "       ':::::::::::'\n");
  fprintf(stderr, "          `'\"\"\"'`\n");
}