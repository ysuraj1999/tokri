#ifndef STANDARDNAMES_H
#define STANDARDNAMES_H

#include <QString>

class StandardNames {
public:
    enum Names {
        Directory,
        LockFile,
        LocalServer
    };

    // TODO make StandardPaths
    static QString get(Names name){
        switch (name){
        case Directory:
            return "Tokri";
        case LockFile:
            return "com.ysuraj.Tokri.lock";
        case LocalServer:
            return "com.ysuraj.Tokri.ipc";
        default:
            return "";
        }
    }
};

#endif // STANDARDNAMES_H
