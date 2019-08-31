#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
rm -rf $DIR/../../thirdparty/or-tools
git clone https://github.com/google/or-tools.git -b v7.3 $DIR/../../thirdparty/or-tools
ORTOOLS="$DIR/../../thirdparty/or-tools/"
rm -rf $ORTOOLS/.git
rm -rf $ORTOOLS/.travis
rm -rf $ORTOOLS/bazel
rm -rf $ORTOOLS/binder
rm -rf $ORTOOLS/cmake
rm -rf $ORTOOLS/dependencies
rm -rf $ORTOOLS/docs
rm -rf $ORTOOLS/examples
rm -rf $ORTOOLS/makefiles
rm -rf $ORTOOLS/patches
rm $ORTOOLS/.appveyor.yml
rm $ORTOOLS/.gitignore
rm $ORTOOLS/.pylintrc
rm $ORTOOLS/.travis.yml
rm $ORTOOLS/CMakeLists.txt
rm $ORTOOLS/CONTRIBUTING.md
rm $ORTOOLS/Dependencies.txt
rm $ORTOOLS/Makefile
rm $ORTOOLS/or-tools.code-workspace
rm $ORTOOLS/pom.xml
rm $ORTOOLS/README.md
rm $ORTOOLS/test.py.in
rm $ORTOOLS/Version.txt
rm $ORTOOLS/WORKSPACE
rm -rf $ORTOOLS/ortools/algorithms/csharp
rm -rf $ORTOOLS/ortools/algorithms/java
rm -rf $ORTOOLS/ortools/algorithms/python
rm -rf $ORTOOLS/ortools/algorithms/samples
rm -rf $ORTOOLS/ortools/graph/csharp
rm -rf $ORTOOLS/ortools/graph/java
rm -rf $ORTOOLS/ortools/graph/python
rm -rf $ORTOOLS/ortools/graph/samples
rm -rf $ORTOOLS/ortools/dotnet
rm -rf $ORTOOLS/tools
rm -rf $ORTOOLS/ortools/constraint_solver/samples
rm -rf $ORTOOLS/ortools/constraint_solver/java
rm -rf $ORTOOLS/ortools/constraint_solver/python
rm -rf $ORTOOLS/ortools/linear_solver/csharp
rm -rf $ORTOOLS/ortools/sat
rm -rf $ORTOOLS/ortools/util/csharp
rm -rf $ORTOOLS/ortools/util/python
rm -rf $ORTOOLS/ortools/util/java
rm -rf $ORTOOLS/ortools/linear_solver/samples
rm -rf $ORTOOLS/ortools/data/python
rm -rf $ORTOOLS/ortools/com