This directory only contains the files which were
modified to compile taglib as a static library under
microsoft visual studio.

The other files can be checkout from svn:
svn://anonsvn.kde.org/home/kde/trunk/kdesupport/taglib


The following msvc-project settings necessary to make it work:

Configuation properties/C++/Code Generation/Struct Member Alignment: 1 byte
Configuation properties/C++/Optimization/Whole Program Optimization: No