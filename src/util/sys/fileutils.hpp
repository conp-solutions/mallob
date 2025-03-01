
#ifndef DOMPASCH_MALLOB_FILE_UTILS_H
#define DOMPASCH_MALLOB_FILE_UTILS_H

#include <string>

class FileUtils {

public:
    static int mkdir(const std::string& dir);
    static int mergeFiles(const std::string& globstr, const std::string& dest, bool removeOriginals);
    static int append(const std::string& srcFile, const std::string& destFile);
    static int rm(const std::string& file);
    static bool isRegularFile(const std::string& file);
};

#endif