#ifndef STANDARDPATHS_H
#define STANDARDPATHS_H

#include <QString>

namespace StandardPaths {

// Directory that holds everything dropped into the basket. Created on first
// use so callers can write into it immediately.
QString tokriDir();

}

#endif // STANDARDPATHS_H
