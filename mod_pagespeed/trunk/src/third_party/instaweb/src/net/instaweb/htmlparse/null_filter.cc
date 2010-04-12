// Copyright 2010 and onwards Google Inc.
// Author: jmarantz@google.com (Joshua Marantz)

#include <stdio.h>
#include "net/instaweb/htmlparse/public/file_driver.h"
#include "net/instaweb/util/public/file_message_handler.h"
#include "net/instaweb/htmlparse/public/file_statistics_log.h"
#include "net/instaweb/util/public/stdio_file_system.h"

int null_filter(int argc, char** argv) {
  int ret = 1;

  if ((argc < 2) || (argc > 4)) {
    fprintf(stdout, "Usage: %s input_file [- | output_file] [log_file]\n",
            argv[0]);
    return ret;
  }

  const char* infile = argv[1];
  net_instaweb::FileMessageHandler message_handler(stderr);
  net_instaweb::StdioFileSystem file_system;
  net_instaweb::FileDriver file_driver(&message_handler, &file_system);
  const char* outfile = NULL;
  std::string outfile_buffer;
  const char* statsfile = NULL;
  std::string statsfile_buffer;

  if (argc >= 3) {
    outfile = argv[2];
  } else if (net_instaweb::FileDriver::GenerateOutputFilename(
                  infile, &outfile_buffer)) {
    outfile = outfile_buffer.c_str();
    fprintf(stdout, "Null rewriting %s into %s\n", infile, outfile);
  } else {
    message_handler.FatalError(infile, 0, "Cannot generate output filename");
  }

  if (argc >= 4) {
    statsfile = argv[3];
  } else if (net_instaweb::FileDriver::GenerateStatsFilename(
                 infile, &statsfile_buffer)) {
    statsfile = statsfile_buffer.c_str();
    fprintf(stdout, "Logging statistics for %s into %s\n",
            infile, statsfile);
  } else {
    message_handler.FatalError(infile, 0, "Cannot generate stats file name");
  }

  if (file_driver.ParseFile(infile, outfile, statsfile)) {
    ret = 0;
  }

  return ret;
}
