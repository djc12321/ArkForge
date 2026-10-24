#pragma once
#include <string>
#include <sys/types.h>
// Private, per-command process service. It only starts bundled official Git programs.
pid_t StartGitProcessBroker(const std::string &libraryDirectory,const std::string &socketPath,const std::string &credential = "");
